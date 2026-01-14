#include "analysisflowcontroller.h"

#include "analysiscoordinator.h"
#include "analysisresultspresenter.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifurecordlistmodel.h"
#include "engineanalysistab.h"
#include "kifudisplay.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "pvboarddialog.h"

#include <limits>
#include <QString>
#include <QObject>
#include <QtGlobal>
#include <QPointer>

AnalysisFlowController::AnalysisFlowController(QObject* parent)
    : QObject(parent)
{
}

void AnalysisFlowController::start(const Deps& d, KifuAnalysisDialog* dlg)
{
    if (!d.sfenRecord || d.sfenRecord->isEmpty()) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: sfenRecord が未準備です。棋譜読み込み後に実行してください。"));
        return;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: 解析モデルが未準備です。"));
        return;
    }
    if (!d.usi) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: Usi インスタンスが未初期化です。"));
        return;
    }
    if (!dlg) return;

    // Cache deps
    m_sfenRecord    = d.sfenRecord;
    m_moveRecords   = d.moveRecords;
    m_recordModel   = d.recordModel;
    m_analysisModel = d.analysisModel;
    m_analysisTab   = d.analysisTab;
    m_usi           = d.usi;
    m_logModel      = d.logModel;
    m_activePly     = d.activePly;
    m_blackPlayerName = d.blackPlayerName;
    m_whitePlayerName = d.whitePlayerName;
    m_usiMoves      = d.usiMoves;
    m_err           = d.displayError;

    // ★ sfenRecordとusiMovesの整合性をチェック（デバッグ用）
    const qsizetype sfenSize = m_sfenRecord ? m_sfenRecord->size() : 0;
    const qsizetype usiSize = m_usiMoves ? m_usiMoves->size() : 0;
    qDebug().noquote() << "[AnalysisFlowController::start] sfenRecord.size=" << sfenSize
                       << " usiMoves.size=" << usiSize
                       << " (expected: sfenSize == usiSize + 1)";
    if (m_usiMoves && sfenSize != usiSize + 1) {
        qWarning().noquote() << "[AnalysisFlowController::start] WARNING: sfenRecord and usiMoves size mismatch!"
                             << " Will use recordModel fallback for lastUsiMove extraction.";
        // 注意: m_usiMovesをnullptrにしない。境界チェックで対応し、範囲外の場合は棋譜表記から抽出する
    }

    m_prevEvalCp    = 0; // 差分用の前回値をリセット
    m_pendingPly    = -1; // 一時結果をリセット
    m_lastCommittedPly = -1; // GUI更新用の結果をリセット
    m_stoppedByUser = false; // 中止フラグをリセット

    // Coordinator（初回のみ作成、シグナル接続は毎回）
    if (!m_coord) {
        AnalysisCoordinator::Deps cd;
        cd.sfenRecord = m_sfenRecord;       // ★ 解析対象のSFEN列
        m_coord = new AnalysisCoordinator(cd, this);
    }

    // (C) 進捗を結果モデルへ投入（毎回再接続）
    QObject::disconnect(m_coord, &AnalysisCoordinator::analysisProgress, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::analysisProgress,
        this,    &AnalysisFlowController::onAnalysisProgress_,
        Qt::UniqueConnection
        );

    // (C-2) position準備時に盤面データを更新
    QObject::disconnect(m_coord, &AnalysisCoordinator::positionPrepared, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::positionPrepared,
        this,    &AnalysisFlowController::onPositionPrepared_,
        Qt::UniqueConnection
        );

    // (C-3) 解析完了時に最後の結果を発行
    QObject::disconnect(m_coord, &AnalysisCoordinator::analysisFinished, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::analysisFinished,
        this,    &AnalysisFlowController::onAnalysisFinished_,
        Qt::UniqueConnection
        );

    // (A) AC → エンジンへ USI 文字列を橋渡し（毎回再接続）
    QObject::disconnect(m_coord, &AnalysisCoordinator::requestSendUsiCommand, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::requestSendUsiCommand,
        m_usi,   &Usi::sendRaw,
        Qt::UniqueConnection
        );

    // (B-1) Usi::bestMoveReceived → AC へ直接通知
    QObject::disconnect(m_usi, &Usi::bestMoveReceived, nullptr, nullptr);
    QObject::connect(
        m_usi, &Usi::bestMoveReceived,
        this,  &AnalysisFlowController::onBestMoveReceived_,
        Qt::UniqueConnection
        );

    // (B-2) Usi::infoLineReceived → AC へ直接通知
    QObject::disconnect(m_usi, &Usi::infoLineReceived, nullptr, nullptr);
    QObject::connect(
        m_usi, &Usi::infoLineReceived,
        this,  &AnalysisFlowController::onInfoLineReceived_,
        Qt::UniqueConnection
        );

    // (B-3) Usi::thinkingInfoUpdated → 漢字PV取得用
    QObject::disconnect(m_usi, &Usi::thinkingInfoUpdated, nullptr, nullptr);
    QObject::connect(
        m_usi, &Usi::thinkingInfoUpdated,
        this,  &AnalysisFlowController::onThinkingInfoUpdated_,
        Qt::UniqueConnection
        );

    // 結果ビュー（Presenter）— 既存APIは showWithModel(...)
    if (!m_presenter) {
        m_presenter = new AnalysisResultsPresenter(this);
        // 中止ボタンのシグナルを接続
        QObject::connect(
            m_presenter, &AnalysisResultsPresenter::stopRequested,
            this,        &AnalysisFlowController::stop,
            Qt::UniqueConnection
            );
        // 行ダブルクリックのシグナルを接続（読み筋表示用）
        QObject::connect(
            m_presenter, &AnalysisResultsPresenter::rowDoubleClicked,
            this,        &AnalysisFlowController::onResultRowDoubleClicked_,
            Qt::UniqueConnection
            );
        // 行選択のシグナルを接続（棋譜欄・将棋盤・分岐ツリー連動用）
        QObject::connect(
            m_presenter, &AnalysisResultsPresenter::rowSelected,
            this,        &AnalysisFlowController::analysisResultRowSelected,
            Qt::UniqueConnection
            );
    }
    m_presenter->showWithModel(m_analysisModel);
    m_presenter->setStopButtonEnabled(true);  // 解析開始時は有効

    // ダイアログ設定を AC オプションへ反映
    applyDialogOptions_(dlg);

    // エンジン起動
    const int  engineIdx = dlg->engineNumber();
    const auto engines   = dlg->engineList();
    if (engineIdx < 0 || engineIdx >= engines.size()) {
        if (m_err) m_err(QStringLiteral("エンジン選択が不正です。"));
        return;
    }
    const QString enginePath = engines.at(engineIdx).path;
    const QString engineName = dlg->engineName();
    
    // 思考タブのエンジン名を設定
    if (m_logModel) {
        m_logModel->setEngineName(engineName);
    }
    
    m_usi->startAndInitializeEngine(enginePath, engineName); // usi→usiok/setoption/isready→readyok

    // ★ 解析用の盤面データを初期化（info行のPV解析に必要）
    m_usi->prepareBoardDataForAnalysis();

    // 解析開始
    m_running = true;
    m_coord->startAnalyzeRange();
}

void AnalysisFlowController::stop()
{
    qDebug().noquote() << "[AnalysisFlowController::stop] called, m_running=" << m_running;
    
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_stoppedByUser = true;  // ユーザーによる中止を記録
    
    // AnalysisCoordinatorを停止
    if (m_coord) {
        m_coord->stop();
    }
    
    // Usiにquitコマンドを送信してエンジンを停止
    if (m_usi) {
        m_usi->sendQuitCommand();
    }
    
    // 中止ボタンを無効化
    if (m_presenter) {
        m_presenter->setStopButtonEnabled(false);
    }
    
    // 停止シグナルを発行
    Q_EMIT analysisStopped();
    
    qDebug().noquote() << "[AnalysisFlowController::stop] analysis stopped";
}

void AnalysisFlowController::applyDialogOptions_(KifuAnalysisDialog* dlg)
{
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = dlg->byoyomiSec() * 1000;

    const int sfenSize = static_cast<int>(m_sfenRecord->size());
    
    // 解析範囲の最大値を設定
    // 注: sfenRecordには終局指し手（投了、中断など）は含まれないため、
    //     sfenSize - 1 が最後の指し手の局面インデックスとなる
    int maxEndPly = sfenSize - 1;
    
    // ダイアログの設定に基づいて範囲を決定
    if (dlg->initPosition()) {
        // "開始局面から最終手まで"が選択された場合
        opt.startPly = 0;
        opt.endPly = qMax(0, maxEndPly);
    } else {
        // 範囲指定が選択された場合
        opt.startPly = qBound(0, dlg->startPly(), maxEndPly);
        opt.endPly = qBound(opt.startPly, dlg->endPly(), maxEndPly);
    }
    
    qDebug().noquote() << "[AnalysisFlowController::applyDialogOptions_] startPly=" << opt.startPly
                       << "endPly=" << opt.endPly << "maxEndPly=" << maxEndPly;

    opt.multiPV    = 1;    // ダイアログ未対応なら 1 固定
    opt.centerTree = true;

    m_coord->setOptions(opt);
}

// ======================
//  スロット実装（非ラムダ）
// ======================

void AnalysisFlowController::onUsiCommLogChanged_()
{
    if (!m_coord || !m_logModel) return;

    const QString line = m_logModel->usiCommLog().trimmed();
    if (line.startsWith(QStringLiteral("info "))) {
        m_coord->onEngineInfoLine(line);
    }
    // bestmoveはonBestMoveReceived_で処理するため、ここでは処理しない
}

void AnalysisFlowController::onBestMoveReceived_()
{
    qDebug().noquote() << "[AnalysisFlowController::onBestMoveReceived_] called, pendingPly=" << m_pendingPly;
    
    // ThinkingInfoPresenterのバッファをフラッシュして、最新の漢字PVを取得
    if (m_usi) {
        m_usi->flushThinkingInfoBuffer();
    }
    
    // 一時保存した結果を確定
    commitPendingResult_();
    
    if (!m_coord) return;
    m_coord->onEngineBestmoveReceived(QString());
}

void AnalysisFlowController::onInfoLineReceived_(const QString& line)
{
    // info行を受け取り、AnalysisCoordinatorに転送
    // 漢字PV変換はThinkingInfoPresenterが行い、onThinkingInfoUpdated_で受け取る
    qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] line=" << line.left(80);
    if (!m_coord) {
        qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] m_coord is null!";
        return;
    }
    
    m_coord->onEngineInfoLine(line);
}

void AnalysisFlowController::onThinkingInfoUpdated_(const QString& /*time*/, const QString& /*depth*/,
                                                    const QString& /*nodes*/, const QString& /*score*/,
                                                    const QString& pvKanjiStr, const QString& /*usiPv*/,
                                                    const QString& /*baseSfen*/)
{
    // ThinkingInfoPresenterからの漢字PVを保存（思考タブと同じ内容を棋譜解析結果に使用）
    if (!pvKanjiStr.isEmpty()) {
        m_pendingPvKanji = pvKanjiStr;
        qDebug().noquote() << "[AnalysisFlowController::onThinkingInfoUpdated_] saved pvKanjiStr=" << pvKanjiStr.left(50);
    }
}

void AnalysisFlowController::onPositionPrepared_(int ply, const QString& sfen)
{
    // 各局面の解析開始時のログ
    qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] ply=" << ply << "sfen=" << sfen.left(50);
    
    // Usiにも設定（ThinkingInfoPresenter経由での変換用）
    if (m_usi) {
        // SFENから盤面データを生成
        QString pureSfen = sfen;
        if (pureSfen.startsWith(QStringLiteral("position sfen "))) {
            pureSfen = pureSfen.mid(14);
        } else if (pureSfen.startsWith(QStringLiteral("position "))) {
            pureSfen = pureSfen.mid(9);
        }
        
        ShogiBoard tempBoard;
        tempBoard.setSfen(pureSfen);
        QVector<QChar> boardData = tempBoard.boardData();
        if (boardData.size() == 81) {
            m_usi->setClonedBoardData(boardData);
        }
        
        // ThinkingInfoPresenterに基準SFENを設定（手番情報用）
        m_usi->setBaseSfen(pureSfen);
        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] called setBaseSfen with pureSfen=" << pureSfen.left(50);
        
        // ★ 開始局面に至った最後のUSI指し手を設定（読み筋表示ウィンドウのハイライト用）
        // ply=0は開始局面なので指し手なし、ply>=1はm_usiMoves[ply-1]が最後の指し手
        QString lastUsiMove;
        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] ply=" << ply
                           << " m_usiMoves=" << m_usiMoves
                           << " m_usiMoves->size()=" << (m_usiMoves ? m_usiMoves->size() : -1);
        if (m_usiMoves && ply > 0 && ply <= m_usiMoves->size()) {
            lastUsiMove = m_usiMoves->at(ply - 1);
            qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] extracted lastUsiMove from m_usiMoves[" << (ply - 1) << "]=" << lastUsiMove;
        } else if (m_recordModel && ply > 0 && ply < m_recordModel->rowCount()) {
            // フォールバック: 棋譜表記からUSI形式の指し手を抽出
            // 形式: 「▲７六歩(77)」または「△５五角打」など
            KifuDisplay* moveDisp = m_recordModel->item(ply);
            if (moveDisp) {
                QString moveLabel = moveDisp->currentMove();
                qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] extracting USI move from kanji:" << moveLabel;
                lastUsiMove = extractUsiMoveFromKanji_(moveLabel);
                qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] extracted lastUsiMove from kanji:" << lastUsiMove;
            }
        } else {
            qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] no lastUsiMove: ply=" << ply << " is out of range or m_usiMoves/m_recordModel is null";
        }
        m_usi->setLastUsiMove(lastUsiMove);
        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] setLastUsiMove called with:" << lastUsiMove;
        
        // 直前の指し手の移動先を設定（読み筋の最初の指し手で「同」表記を正しく判定するため）
        // ply=0は開始局面なので直前の指し手なし
        // ply>=1の場合、その局面に至った指し手はrecordModel->item(ply)（ply番目の指し手）
        bool previousMoveSet = false;
        if (m_recordModel && ply > 0 && ply < m_recordModel->rowCount()) {
            KifuDisplay* prevDisp = m_recordModel->item(ply);  // plyの指し手（その局面に至った指し手）
            if (prevDisp) {
                QString prevMoveLabel = prevDisp->currentMove();
                qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] prevMoveLabel from recordModel[" << ply << "]:" << prevMoveLabel;
                
                // 漢字の移動先を抽出して整数座標に変換
                // 形式: 「▲７六歩(77)」または「△同　銀(31)」
                static const QString senteMark = QStringLiteral("▲");
                static const QString goteMark = QStringLiteral("△");
                
                qsizetype markPos = prevMoveLabel.indexOf(senteMark);
                if (markPos < 0) {
                    markPos = prevMoveLabel.indexOf(goteMark);
                }
                
                if (markPos >= 0 && prevMoveLabel.length() > markPos + 2) {
                    QString afterMark = prevMoveLabel.mid(markPos + 1);
                    
                    // 「同」の場合はスキップ（前回の移動先をそのまま使用）
                    if (!afterMark.startsWith(QStringLiteral("同"))) {
                        // 「７六」のような漢字座標を取得
                        QChar fileChar = afterMark.at(0);  // 全角数字 '１'〜'９'
                        QChar rankChar = afterMark.at(1);  // 漢数字 '一'〜'九'
                        
                        // 全角数字を整数に変換（'１'=0xFF11 → 1）
                        int fileTo = 0;
                        if (fileChar >= QChar(0xFF11) && fileChar <= QChar(0xFF19)) {
                            fileTo = fileChar.unicode() - 0xFF11 + 1;
                        }
                        
                        // 漢数字を整数に変換
                        int rankTo = 0;
                        static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
                        qsizetype rankIdxPos = kanjiRanks.indexOf(rankChar);
                        if (rankIdxPos >= 0) {
                            rankTo = static_cast<int>(rankIdxPos) + 1;
                        }
                        
                        if (fileTo >= 1 && fileTo <= 9 && rankTo >= 1 && rankTo <= 9) {
                            m_usi->setPreviousFileTo(fileTo);
                            m_usi->setPreviousRankTo(rankTo);
                            previousMoveSet = true;
                            qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] setPreviousMove from recordModel:"
                                               << "fileTo=" << fileTo << "rankTo=" << rankTo;
                        }
                    } else {
                        // 「同」の場合は、前回設定した座標をそのまま維持
                        previousMoveSet = true;
                        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] previousMove kept (同 notation)";
                    }
                }
            }
        }
        
        if (!previousMoveSet) {
            // 開始局面（ply=0）または取得失敗の場合は移動先をリセット
            m_usi->setPreviousFileTo(0);
            m_usi->setPreviousRankTo(0);
            qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] reset previousMove";
        }
        
        // SFENから手番を抽出してGameControllerに設定
        // 形式: "盤面 手番 駒台 手数" 例: "lnsgkgsnl/... b - 1"
        if (m_gameController) {
            // " b " または " w " を探す
            bool isPlayer1Turn = pureSfen.contains(QStringLiteral(" b "));
            ShogiGameController::Player player = isPlayer1Turn 
                ? ShogiGameController::Player1 
                : ShogiGameController::Player2;
            m_gameController->setCurrentPlayer(player);
            qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] set player=" 
                               << (isPlayer1Turn ? "P1(sente)" : "P2(gote)");
        } else {
            qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] m_gameController is null!";
        }
    } else {
        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] m_usi is null!";
    }
    
    // ★ 通常対局と同じ流れ：
    // 1. 局面と指し手を確定（上記で完了）
    // 2. GUI更新（棋譜欄ハイライト、将棋盤更新）
    // 3. エンジンにコマンド送信
    // 4. 思考タブ更新（info行受信時）
    
    // 前の手の評価値をGUIに反映（m_lastCommittedPlyが確定した手数）
    if (m_lastCommittedPly >= 0) {
        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] emitting analysisProgressReported: ply=" 
                           << m_lastCommittedPly << ", scoreCp=" << m_lastCommittedScoreCp;
        Q_EMIT analysisProgressReported(m_lastCommittedPly, m_lastCommittedScoreCp);
        
        // リセット
        m_lastCommittedPly = -1;
    }
    
    // ★ 現在解析する局面に将棋盤を移動（INT_MINで評価値追加をスキップ）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] moving board to ply=" << ply;
    Q_EMIT analysisProgressReported(ply, POSITION_ONLY_MARKER);
    
    // ★ 思考タブをクリアしてからgoコマンドを送信
    if (m_usi) {
        m_usi->requestClearThinkingInfo();
    }
    
    // ★ GUI更新後にgoコマンドを送信
    if (m_coord) {
        qDebug().noquote() << "[AnalysisFlowController::onPositionPrepared_] calling sendGoCommand";
        m_coord->sendGoCommand();
    }
}

void AnalysisFlowController::onAnalysisProgress_(int ply, int /*depth*/, int /*seldepth*/,
                                                 int scoreCp, int mate,
                                                 const QString& pv, const QString& /*raw*/)
{
    // 結果を一時保存（bestmove時に確定）
    // 最新のinfo行で更新し続ける
    // 注意: m_pendingPvKanjiはonInfoLineReceived_で設定されるため、ここではクリアしない
    //       plyが変わった場合でも、onInfoLineReceived_が先に呼ばれて設定済み
    m_pendingPly = ply;
    m_pendingScoreCp = scoreCp;
    m_pendingMate = mate;
    m_pendingPv = pv;
    
    qDebug().noquote() << "[AnalysisFlowController::onAnalysisProgress_] ply=" << ply << "pv=" << pv.left(30) << "pvKanji=" << m_pendingPvKanji.left(30);
}

void AnalysisFlowController::commitPendingResult_()
{
    if (!m_analysisModel) {
        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] m_analysisModel is null";
        return;
    }

    // m_pendingPlyが-1の場合は定跡（infoなしでbestmoveが来た）
    // m_coordから現在のplyを取得
    int ply = m_pendingPly;
    bool isBook = false;
    if (ply < 0 && m_coord) {
        ply = m_coord->currentPly();
        isBook = true;
        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] book move detected, ply=" << ply;
    }
    
    if (ply < 0) {
        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] skipped: ply=" << ply;
        return;
    }

    const int scoreCp = m_pendingScoreCp;
    const int mate = m_pendingMate;
    // 漢字PVがあればそれを使用、なければUSI形式PV、定跡なら「定跡」
    QString pv;
    if (isBook) {
        pv = QStringLiteral("（定跡）");
    } else if (!m_pendingPvKanji.isEmpty()) {
        pv = m_pendingPvKanji;
    } else {
        pv = m_pendingPv;
    }

    // 一時保存をリセット
    m_pendingPly = -1;
    m_pendingPvKanji.clear();

    // 指し手ラベル（m_recordModelから取得、なければフォールバック）
    QString moveLabel;
    if (m_recordModel && ply >= 0 && m_recordModel->rowCount() > ply) {
        KifuDisplay* disp = m_recordModel->item(ply);
        if (disp) {
            moveLabel = disp->currentMove();
        }
    }
    // 最終フォールバック
    if (moveLabel.isEmpty()) {
        moveLabel = QStringLiteral("ply %1").arg(ply);
    }

    // 評価値 / 差分（定跡の場合は0）
    // エンジンの評価値は「手番側から見た評価値」なので、
    // 先手視点で統一するために後手番（奇数ply）の場合は符号を反転
    bool isGoteTurn = (ply % 2 == 1);  // 奇数plyは後手番
    
    QString evalStr;
    int curVal = 0;
    if (isBook) {
        evalStr = QStringLiteral("-");
        curVal = m_prevEvalCp;  // 差分0
    } else if (mate != 0) {
        // 詰み表示も後手番なら反転
        int adjustedMate = isGoteTurn ? -mate : mate;
        evalStr = QStringLiteral("mate %1").arg(adjustedMate);
        curVal  = m_prevEvalCp; // 詰みは差分0扱い（前回値維持）
    } else if (scoreCp != std::numeric_limits<int>::min()) {
        // 後手番なら評価値を反転
        int adjustedScore = isGoteTurn ? -scoreCp : scoreCp;
        evalStr = QString::number(adjustedScore);
        curVal  = adjustedScore;
    } else {
        evalStr = QStringLiteral("0");
        curVal  = 0;
    }
    const QString diff = isBook ? QStringLiteral("-") : QString::number(curVal - m_prevEvalCp);
    m_prevEvalCp = curVal;

    qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] ply=" << ply << "moveLabel=" << moveLabel << "evalStr=" << evalStr << "pv=" << pv.left(30);
    
    // ★ KifuAnalysisResultsDisplay は (Move, Eval, Diff, PV) の4引数
    KifuAnalysisResultsDisplay* resultItem = new KifuAnalysisResultsDisplay(
        moveLabel,
        evalStr,
        diff,
        pv
        );
    
    // USI形式PVを設定（括弧で囲まれた確率情報を除去）
    QString usiPv = m_pendingPv;
    // 末尾の括弧部分（例: "(100.00%)"）を除去
    qsizetype parenPos = usiPv.indexOf('(');
    if (parenPos > 0) {
        usiPv = usiPv.left(parenPos).trimmed();
    }
    resultItem->setUsiPv(usiPv);
    
    // 局面SFENを設定
    if (m_sfenRecord && ply >= 0 && ply < m_sfenRecord->size()) {
        QString sfen = m_sfenRecord->at(ply);
        // "position sfen ..."形式の場合は除去
        if (sfen.startsWith(QStringLiteral("position sfen "))) {
            sfen = sfen.mid(14);
        } else if (sfen.startsWith(QStringLiteral("position "))) {
            sfen = sfen.mid(9);
        }
        resultItem->setSfen(sfen);
    }
    
    // 最後の指し手（USI形式）を設定（読み筋表示ウィンドウのハイライト用）
    // ply=0 は開始局面なので指し手なし、ply>=1 は usiMoves[ply-1] が最後の指し手
    QString lastMove;
    if (m_usiMoves && ply > 0 && ply <= m_usiMoves->size()) {
        lastMove = m_usiMoves->at(ply - 1);
        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] lastMove from m_usiMoves[" << (ply - 1) << "]=" << lastMove;
    } else if (m_recordModel && ply > 0 && ply < m_recordModel->rowCount()) {
        // フォールバック: 棋譜表記からUSI形式の指し手を抽出
        KifuDisplay* moveDisp = m_recordModel->item(ply);
        if (moveDisp) {
            QString kanjiMoveStr = moveDisp->currentMove();
            lastMove = extractUsiMoveFromKanji_(kanjiMoveStr);
            qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] lastMove from kanji:" << kanjiMoveStr << "->" << lastMove;
        }
    } else {
        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] no lastMove: m_usiMoves=" << m_usiMoves
                           << "ply=" << ply
                           << "usiMoves.size=" << (m_usiMoves ? m_usiMoves->size() : -1);
    }
    if (!lastMove.isEmpty()) {
        resultItem->setLastUsiMove(lastMove);
        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] setLastUsiMove: ply=" << ply << "lastMove=" << lastMove;
    }
    
    // 候補手を設定（前の行の読み筋の最初の指し手）
    int prevRow = m_analysisModel->rowCount() - 1;  // 今追加しようとしている行の1つ前
    if (prevRow >= 0) {
        KifuAnalysisResultsDisplay* prevItem = m_analysisModel->item(prevRow);
        if (prevItem) {
            QString prevPv = prevItem->principalVariation();
            if (!prevPv.isEmpty() && prevPv != QStringLiteral("（定跡）")) {
                // 最初の指し手を取得（スペース区切りまたは末尾まで）
                // 読み筋は "▲７六歩(77)△８四歩(83)..." のような形式
                QString candidateMove;
                
                // 先手/後手を示す記号
                static const QString senteMark = QStringLiteral("▲");
                static const QString goteMark = QStringLiteral("△");
                
                // 漢字表記の場合: 先頭から次の△/▲の前までを取得
                qsizetype nextMark = -1;
                if (prevPv.startsWith(senteMark) || prevPv.startsWith(goteMark)) {
                    // 2文字目以降で次の△/▲を探す
                    qsizetype sentePos = prevPv.indexOf(senteMark, 1);
                    qsizetype gotePos = prevPv.indexOf(goteMark, 1);
                    
                    if (sentePos > 0 && gotePos > 0) {
                        nextMark = qMin(sentePos, gotePos);
                    } else if (sentePos > 0) {
                        nextMark = sentePos;
                    } else if (gotePos > 0) {
                        nextMark = gotePos;
                    }
                }
                
                if (nextMark > 0) {
                    candidateMove = prevPv.left(nextMark);
                } else {
                    // △/▲が見つからなければ全体（1手のみの読み筋）
                    candidateMove = prevPv;
                }
                
                // 前の行の指し手の移動先と候補手の移動先が同じ場合は「同」表記に変換
                // 例: 前の指し手「△８八角成(22)」、候補手「▲８八銀(79)」→「▲同　銀(79)」
                // 注: prevMoveLabelは「   3 ▲２二角成(88)」のように行番号が先頭についている場合がある
                QString prevMoveLabel = prevItem->currentMove();
                qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] prevMoveLabel=" << prevMoveLabel << "candidateMove=" << candidateMove;
                
                if (candidateMove.length() >= 3) {
                    // 前の指し手から移動先を抽出
                    // 行番号を除去して▲/△の位置を見つける
                    QString prevDestination;
                    qsizetype prevMarkPos = prevMoveLabel.indexOf(senteMark);
                    if (prevMarkPos < 0) {
                        prevMarkPos = prevMoveLabel.indexOf(goteMark);
                    }
                    
                    if (prevMarkPos >= 0) {
                        // ▲/△の後の文字が「同」でなければ移動先を抽出
                        QString afterMark = prevMoveLabel.mid(prevMarkPos + 1);
                        if (!afterMark.startsWith(QStringLiteral("同"))) {
                            // 「２二」のような2文字の移動先を抽出
                            if (afterMark.length() >= 2) {
                                prevDestination = afterMark.left(2);
                            }
                        }
                    }
                    
                    // 候補手から移動先を抽出
                    QString candDestination;
                    qsizetype candMarkPos = candidateMove.indexOf(senteMark);
                    if (candMarkPos < 0) {
                        candMarkPos = candidateMove.indexOf(goteMark);
                    }
                    
                    if (candMarkPos >= 0) {
                        QString afterMark = candidateMove.mid(candMarkPos + 1);
                        if (!afterMark.startsWith(QStringLiteral("同"))) {
                            if (afterMark.length() >= 2) {
                                candDestination = afterMark.left(2);
                            }
                        }
                    }
                    
                    qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] prevDestination=" << prevDestination << "candDestination=" << candDestination;
                    
                    // 移動先が同じなら「同」表記に変換
                    if (!prevDestination.isEmpty() && !candDestination.isEmpty() &&
                        prevDestination == candDestination) {
                        // 「▲８八銀(79)」→「▲同　銀(79)」
                        // candMarkPosの位置から変換
                        QString prefix = candidateMove.left(candMarkPos + 1);  // "▲" or "△"まで
                        QString suffix = candidateMove.mid(candMarkPos + 3);    // 駒種以降（銀(79)など）
                        candidateMove = prefix + QStringLiteral("同　") + suffix;
                        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] converted to 同 notation:" << candidateMove;
                    }
                }
                
                // 候補手に「同」が含まれている場合、「同　」（同＋全角空白）に統一
                // 「△同銀(31)」→「△同　銀(31)」
                if (candidateMove.contains(QStringLiteral("同"))) {
                    // 「同」の後に全角空白がない場合は追加
                    // ただし「同　」は既にあるのでスキップ
                    if (!candidateMove.contains(QStringLiteral("同　"))) {
                        candidateMove.replace(QStringLiteral("同"), QStringLiteral("同　"));
                        qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] added space after 同:" << candidateMove;
                    }
                }
                
                resultItem->setCandidateMove(candidateMove);
                qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] setCandidateMove:" << candidateMove;
            }
        }
    }
    
    m_analysisModel->appendItem(resultItem);
    
    // ★ GUI更新用に結果を保存（次のonPositionPrepared_でシグナルを発行）
    m_lastCommittedPly = ply;
    m_lastCommittedScoreCp = curVal;
}

void AnalysisFlowController::onAnalysisFinished_(AnalysisCoordinator::Mode /*mode*/)
{
    qDebug().noquote() << "[AnalysisFlowController::onAnalysisFinished_] called, m_stoppedByUser=" << m_stoppedByUser;
    
    // 最後の結果をGUIに反映
    if (m_lastCommittedPly >= 0) {
        qDebug().noquote() << "[AnalysisFlowController::onAnalysisFinished_] emitting analysisProgressReported for final ply=" 
                           << m_lastCommittedPly << "scoreCp=" << m_lastCommittedScoreCp;
        Q_EMIT analysisProgressReported(m_lastCommittedPly, m_lastCommittedScoreCp);
        m_lastCommittedPly = -1;
    }
    
    // 解析中フラグをリセット
    m_running = false;
    
    // 中止ボタンを無効化
    if (m_presenter) {
        m_presenter->setStopButtonEnabled(false);
    }
    
    // ユーザーによる中止でなければ（正常完了なら）完了メッセージを表示
    if (!m_stoppedByUser && m_presenter && m_analysisModel) {
        int totalMoves = m_analysisModel->rowCount();
        m_presenter->showAnalysisComplete(totalMoves);
    }
    
    Q_EMIT analysisStopped();
}

void AnalysisFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    qDebug().noquote() << "[AnalysisFlowController::runWithDialog] START";
    qDebug().noquote() << "[AnalysisFlowController::runWithDialog] d.gameController=" << d.gameController;
    qDebug().noquote() << "[AnalysisFlowController::runWithDialog] d.usi=" << d.usi;
    
    // 依存の必須チェック（usi以外）
    if (!d.sfenRecord || d.sfenRecord->isEmpty()) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: sfenRecord が未準備です。棋譜読み込み後に実行してください。"));
        return;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: 解析モデルが未準備です。"));
        return;
    }

    // ダイアログを生成してユーザに選択してもらう
    KifuAnalysisDialog dlg(parent);
    
    // 最大手数を設定
    // 注: sfenRecordには終局指し手（投了、中断など）は含まれないため、
    //     sfenSize - 1 が最後の指し手の局面インデックスとなる
    int maxPly = static_cast<int>(d.sfenRecord->size()) - 1;
    dlg.setMaxPly(qMax(0, maxPly));
    
    const int result = dlg.exec();
    if (result != QDialog::Accepted) return;

    // GameControllerを保持
    m_gameController = d.gameController;
    qDebug().noquote() << "[AnalysisFlowController::runWithDialog] m_gameController=" << m_gameController;
    if (m_gameController) {
        qDebug().noquote() << "[AnalysisFlowController::runWithDialog] m_gameController->board()=" << m_gameController->board();
    }

    // Usiが渡されていない場合は内部で生成
    Deps actualDeps = d;
    if (!actualDeps.usi) {
        qDebug().noquote() << "[AnalysisFlowController::runWithDialog] Creating internal Usi instance...";
        
        // ログモデル: 渡されたものがあればそれを使用、なければ生成
        UsiCommLogModel* logModelToUse = d.logModel;
        if (!logModelToUse) {
            if (!m_ownedLogModel) {
                m_ownedLogModel = new UsiCommLogModel(this);
            }
            logModelToUse = m_ownedLogModel;
        }
        
        // ThinkingModel: 渡されたものがあればそれを使用、なければ生成
        ShogiEngineThinkingModel* thinkingModelToUse = d.thinkingModel;
        if (!thinkingModelToUse) {
            if (!m_ownedThinkingModel) {
                m_ownedThinkingModel = new ShogiEngineThinkingModel(this);
            }
            thinkingModelToUse = m_ownedThinkingModel;
        }

        // Usiインスタンスを生成（GameControllerを渡して盤面情報を取得可能にする）
        m_usi = new Usi(logModelToUse, thinkingModelToUse, m_gameController, m_playModeForAnalysis, this);
        m_ownsUsi = true;

        actualDeps.usi = m_usi;
        actualDeps.logModel = logModelToUse;
        actualDeps.thinkingModel = thinkingModelToUse;
        qDebug().noquote() << "[AnalysisFlowController::runWithDialog] Internal Usi created:" << m_usi;
        qDebug().noquote() << "[AnalysisFlowController::runWithDialog] Using logModel:" << logModelToUse;
        qDebug().noquote() << "[AnalysisFlowController::runWithDialog] Using thinkingModel:" << thinkingModelToUse;
    }

    // 以降は既存の start(...) に委譲（Presenter への表示や接続も start 側で実施）
    start(actualDeps, &dlg);
}

void AnalysisFlowController::onResultRowDoubleClicked_(int row)
{
    qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] row=" << row;
    
    if (!m_analysisModel) {
        qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] m_analysisModel is null";
        return;
    }
    
    if (row < 0 || row >= m_analysisModel->rowCount()) {
        qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] row out of range";
        return;
    }
    
    // KifuAnalysisResultsDisplayから読み筋を取得
    KifuAnalysisResultsDisplay* item = m_analysisModel->item(row);
    if (!item) {
        qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] item is null";
        return;
    }
    
    QString kanjiPv = item->principalVariation();
    qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] kanjiPv=" << kanjiPv.left(50);
    
    if (kanjiPv.isEmpty()) {
        qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] kanjiPv is empty";
        return;
    }
    
    // 局面SFENを取得（itemから）
    QString baseSfen = item->sfen();
    if (baseSfen.isEmpty()) {
        // フォールバック: m_sfenRecordから取得
        if (m_sfenRecord && row < m_sfenRecord->size()) {
            baseSfen = m_sfenRecord->at(row);
            // "position sfen ..."形式の場合は除去
            if (baseSfen.startsWith(QStringLiteral("position sfen "))) {
                baseSfen = baseSfen.mid(14);
            } else if (baseSfen.startsWith(QStringLiteral("position "))) {
                baseSfen = baseSfen.mid(9);
            }
        }
    }
    if (baseSfen.isEmpty()) {
        baseSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] baseSfen=" << baseSfen.left(50);
    
    // USI形式の読み筋を取得（itemから）
    QString usiPvStr = item->usiPv();
    QStringList usiMoves;
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(' ', Qt::SkipEmptyParts);
        qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] usiMoves from item:" << usiMoves;
    }
    
    // PvBoardDialogを表示
    QWidget* parentWidget = nullptr;
    if (m_presenter && m_presenter->dialog()) {
        parentWidget = m_presenter->dialog();
    }
    
    PvBoardDialog* dlg = new PvBoardDialog(baseSfen, usiMoves, parentWidget);
    dlg->setKanjiPv(kanjiPv);
    
    // 対局者名を設定（Depsから取得した名前を使用）
    QString blackName = m_blackPlayerName.isEmpty() ? tr("先手") : m_blackPlayerName;
    QString whiteName = m_whitePlayerName.isEmpty() ? tr("後手") : m_whitePlayerName;
    dlg->setPlayerNames(blackName, whiteName);
    
    // 最後の指し手を設定（初期局面のハイライト用）
    QString lastMove = item->lastUsiMove();
    if (lastMove.isEmpty()) {
        // フォールバック: 棋譜表記からUSI形式の指し手を抽出
        // rowはitem->moveNum()と同じはずだが、念のためrowを使用
        int ply = row;  // 解析結果のrow番号は手数(ply)と一致
        if (m_recordModel && ply > 0 && ply < m_recordModel->rowCount()) {
            KifuDisplay* moveDisp = m_recordModel->item(ply);
            if (moveDisp) {
                QString moveLabel = moveDisp->currentMove();
                lastMove = extractUsiMoveFromKanji_(moveLabel);
                qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] lastMove from kanji:" << moveLabel << "->" << lastMove;
            }
        }
    }
    if (!lastMove.isEmpty()) {
        qDebug().noquote() << "[AnalysisFlowController::onResultRowDoubleClicked_] setting lastMove:" << lastMove;
        dlg->setLastMove(lastMove);
    }
    
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

QString AnalysisFlowController::extractUsiMoveFromKanji_(const QString& kanjiMove) const
{
    // 漢字表記からUSI形式の指し手を抽出する
    // 形式例:
    //   「▲７六歩(77)」 → "7g7f" （通常移動）
    //   「△同　銀(31)」 → 同表記は前回の移動先が必要なのでスキップ
    //   「▲５五角打」   → "B*5e" （駒打ち）
    //   「▲３三歩成(34)」 → "3d3c+" （成り）

    if (kanjiMove.isEmpty()) {
        return QString();
    }

    // 先手・後手マークを探す
    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");

    qsizetype markPos = kanjiMove.indexOf(senteMark);
    if (markPos < 0) {
        markPos = kanjiMove.indexOf(goteMark);
    }
    if (markPos < 0 || kanjiMove.length() <= markPos + 2) {
        return QString();
    }

    QString afterMark = kanjiMove.mid(markPos + 1);

    // 「同」表記の場合は前回の移動先が必要なのでスキップ
    if (afterMark.startsWith(QStringLiteral("同"))) {
        qDebug().noquote() << "[extractUsiMoveFromKanji_] 同 notation, cannot extract USI move";
        return QString();
    }

    // 駒打ち判定（「打」を含む場合）
    bool isDrop = afterMark.contains(QStringLiteral("打"));

    // 成り判定（「成」を含む場合）
    bool isPromotion = afterMark.contains(QStringLiteral("成")) && !afterMark.contains(QStringLiteral("不成"));

    // 移動先座標を取得（全角数字 + 漢数字）
    QChar fileChar = afterMark.at(0);  // 全角数字 '１'〜'９'
    QChar rankChar = afterMark.at(1);  // 漢数字 '一'〜'九'

    // 全角数字を整数に変換
    int fileTo = 0;
    if (fileChar >= QChar(0xFF11) && fileChar <= QChar(0xFF19)) {
        fileTo = fileChar.unicode() - 0xFF11 + 1;
    }

    // 漢数字を整数に変換
    int rankTo = 0;
    static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
    qsizetype rankIdx = kanjiRanks.indexOf(rankChar);
    if (rankIdx >= 0) {
        rankTo = static_cast<int>(rankIdx) + 1;
    }

    if (fileTo < 1 || fileTo > 9 || rankTo < 1 || rankTo > 9) {
        qDebug().noquote() << "[extractUsiMoveFromKanji_] invalid destination:" << fileTo << rankTo;
        return QString();
    }

    QChar toRankAlpha = QChar('a' + rankTo - 1);

    if (isDrop) {
        // 駒打ちの場合: "P*5e" 形式
        // 駒の種類を抽出（歩/香/桂/銀/金/角/飛）
        static const QString pieceChars = QStringLiteral("歩香桂銀金角飛");
        static const QString usiPieces = QStringLiteral("PLNSGBR");

        QChar pieceUsi;
        for (qsizetype i = 0; i < pieceChars.size(); ++i) {
            if (afterMark.contains(pieceChars.at(i))) {
                pieceUsi = usiPieces.at(i);
                break;
            }
        }

        if (pieceUsi.isNull()) {
            qDebug().noquote() << "[extractUsiMoveFromKanji_] could not identify piece for drop";
            return QString();
        }

        return QString("%1*%2%3").arg(pieceUsi).arg(fileTo).arg(toRankAlpha);
    } else {
        // 通常移動の場合: 括弧内の元位置を取得 "(77)" → file=7, rank=7
        qsizetype parenStart = afterMark.indexOf('(');
        qsizetype parenEnd = afterMark.indexOf(')');

        if (parenStart < 0 || parenEnd < 0 || parenEnd <= parenStart + 1) {
            qDebug().noquote() << "[extractUsiMoveFromKanji_] could not find source position in parentheses";
            return QString();
        }

        QString srcStr = afterMark.mid(parenStart + 1, parenEnd - parenStart - 1);
        if (srcStr.length() != 2) {
            qDebug().noquote() << "[extractUsiMoveFromKanji_] invalid source string:" << srcStr;
            return QString();
        }

        int fileFrom = srcStr.at(0).digitValue();
        int rankFrom = srcStr.at(1).digitValue();

        if (fileFrom < 1 || fileFrom > 9 || rankFrom < 1 || rankFrom > 9) {
            qDebug().noquote() << "[extractUsiMoveFromKanji_] invalid source coordinates:" << fileFrom << rankFrom;
            return QString();
        }

        QChar fromRankAlpha = QChar('a' + rankFrom - 1);

        QString usiMove = QString("%1%2%3%4").arg(fileFrom).arg(fromRankAlpha).arg(fileTo).arg(toRankAlpha);
        if (isPromotion) {
            usiMove += '+';
        }

        return usiMove;
    }
}
