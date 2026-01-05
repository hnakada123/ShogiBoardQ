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
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"

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
    m_err           = d.displayError;
    m_prevEvalCp    = 0; // 差分用の前回値をリセット
    m_pendingPly    = -1; // 一時結果をリセット

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
    }
    m_presenter->showWithModel(m_analysisModel);

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
    
    // AnalysisCoordinatorを停止
    if (m_coord) {
        m_coord->stop();
    }
    
    // Usiにquitコマンドを送信してエンジンを停止
    if (m_usi) {
        m_usi->sendQuitCommand();
    }
    
    // 停止シグナルを発行
    Q_EMIT analysisStopped();
    
    qDebug().noquote() << "[AnalysisFlowController::stop] analysis stopped";
}

void AnalysisFlowController::applyDialogOptions_(KifuAnalysisDialog* dlg)
{
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = dlg->byoyomiSec() * 1000;

    const int startRaw = dlg->initPosition() ? 0 : qMax(0, m_activePly);
    const int sfenSize = static_cast<int>(m_sfenRecord->size());
    opt.startPly = qBound(0, startRaw, sfenSize - 1);
    opt.endPly   = sfenSize - 1;

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
    
    // 一時保存した結果を確定
    commitPendingResult_();
    
    if (!m_coord) return;
    m_coord->onEngineBestmoveReceived(QString());
}

void AnalysisFlowController::onInfoLineReceived_(const QString& line)
{
    qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] line=" << line.left(80);
    if (!m_coord) {
        qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] m_coord is null!";
        return;
    }
    
    // info行から漢字PVを生成（ThinkingInfoPresenterのバッファリングを回避）
    if (line.startsWith(QStringLiteral("info ")) && line.contains(QStringLiteral(" pv "))) {
        // 現在解析中のplyのSFENから盤面データを生成
        const int currentPly = m_coord->currentPly();
        
        if (m_sfenRecord && currentPly >= 0 && currentPly < m_sfenRecord->size()) {
            QString sfen = m_sfenRecord->at(currentPly);
            qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] ply=" << currentPly << "sfen from record=" << sfen.left(60);
            
            // "position sfen ..."形式の場合は除去
            if (sfen.startsWith(QStringLiteral("position sfen "))) {
                sfen = sfen.mid(14);
            } else if (sfen.startsWith(QStringLiteral("position "))) {
                sfen = sfen.mid(9);
            }
            
            // SFENから手番を取得（" b " = 先手、" w " = 後手）
            ShogiGameController::Player sideToMove = ShogiGameController::Player1;  // デフォルトは先手
            if (sfen.contains(QStringLiteral(" w "))) {
                sideToMove = ShogiGameController::Player2;
            }
            
            // SFENから盤面データを生成
            ShogiBoard tempBoard;
            tempBoard.setSfen(sfen);
            QVector<QChar> boardData = tempBoard.boardData();
            
            // デバッグ: 盤面データを表示（9段分）
            QString boardDebug;
            for (int r = 0; r < 9; ++r) {
                boardDebug += QString("[rank%1: ").arg(r + 1);
                for (int f = 0; f < 9; ++f) {
                    int idx = r * 9 + f;
                    if (idx < boardData.size()) {
                        QChar c = boardData[idx];
                        boardDebug += (c == QChar(' ')) ? QChar('.') : c;
                    }
                }
                boardDebug += "] ";
            }
            qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] ply=" << currentPly << "board=" << boardDebug;
            
            if (boardData.size() == 81) {
                ShogiEngineInfoParser parser;
                parser.setThinkingStartPlayer(sideToMove);  // 手番を設定
                QString lineCopy = line;
                parser.parseEngineOutputAndUpdateState(
                    lineCopy,
                    nullptr,  // gameControllerは使わない
                    boardData,
                    false  // ponder disabled
                );
                QString kanjiPv = parser.pvKanjiStr();
                QString usiPv = parser.pvUsiStr();
                
                if (!kanjiPv.isEmpty()) {
                    m_pendingPvKanji = kanjiPv;
                    qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] ply=" << currentPly 
                                       << "side=" << (sideToMove == ShogiGameController::Player1 ? "b" : "w")
                                       << "kanjiPv=" << kanjiPv.left(50);
                } else {
                    qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] ply=" << currentPly 
                                       << "kanjiPv is EMPTY, usiPv=" << usiPv.left(30);
                }
            } else {
                qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] ply=" << currentPly << "boardData size=" << boardData.size() << "(invalid)";
            }
        } else {
            qDebug().noquote() << "[AnalysisFlowController::onInfoLineReceived_] sfenRecord not available or ply out of range: ply=" << currentPly;
        }
    }
    
    m_coord->onEngineInfoLine(line);
}

void AnalysisFlowController::onThinkingInfoUpdated_(const QString& /*time*/, const QString& /*depth*/,
                                                    const QString& /*nodes*/, const QString& /*score*/,
                                                    const QString& pvKanjiStr, const QString& /*usiPv*/,
                                                    const QString& /*baseSfen*/)
{
    // デバッグログのみ出力
    // 注意: onInfoLineReceived_で正しい手番の漢字PVを生成しているため、
    //       ThinkingInfoPresenterからの漢字PVは使用しない（バッファリング遅延で手番がずれる可能性があるため）
    qDebug().noquote() << "[AnalysisFlowController::onThinkingInfoUpdated_] pvKanjiStr=" << pvKanjiStr.left(50);
    // m_pendingPvKanjiはonInfoLineReceived_で設定済みなので、ここでは上書きしない
}

void AnalysisFlowController::onPositionPrepared_(int ply, const QString& sfen)
{
    // 各局面の解析開始時のログ
    // 実際の盤面データはonInfoLineReceived_でSFENから都度生成する
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
    QString evalStr;
    int curVal = 0;
    if (isBook) {
        evalStr = QStringLiteral("-");
        curVal = m_prevEvalCp;  // 差分0
    } else if (mate != 0) {
        evalStr = QStringLiteral("mate %1").arg(mate);
        curVal  = m_prevEvalCp; // 詰みは差分0扱い（前回値維持）
    } else if (scoreCp != std::numeric_limits<int>::min()) {
        evalStr = QString::number(scoreCp);
        curVal  = scoreCp;
    } else {
        evalStr = QStringLiteral("0");
        curVal  = 0;
    }
    const QString diff = isBook ? QStringLiteral("-") : QString::number(curVal - m_prevEvalCp);
    m_prevEvalCp = curVal;

    qDebug().noquote() << "[AnalysisFlowController::commitPendingResult_] ply=" << ply << "moveLabel=" << moveLabel << "evalStr=" << evalStr << "pv=" << pv.left(30);
    
    // ★ KifuAnalysisResultsDisplay は (Move, Eval, Diff, PV) の4引数
    m_analysisModel->appendItem(new KifuAnalysisResultsDisplay(
        moveLabel,
        evalStr,
        diff,
        pv
        ));
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
