/// @file analysisflowcontroller.cpp
/// @brief 棋譜解析フローコントローラクラスの実装

#include "analysisflowcontroller.h"

#include "analysiscoordinator.h"
#include "analysisresulthandler.h"
#include "analysisresultspresenter.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"

#include <QString>
#include <QObject>
#include <QtGlobal>
#include <QPointer>

#include "logcategories.h"

AnalysisFlowController::AnalysisFlowController(QObject* parent)
    : QObject(parent)
    , m_resultHandler(std::make_unique<AnalysisResultHandler>())
{
}

AnalysisFlowController::~AnalysisFlowController()
{
    // 解析中の場合はリソースリーク防止のため停止処理を行う
    if (m_running) {
        stop();
    }
    // 注意：m_ownedLogModel, m_ownedThinkingModel, m_usi, m_coord は
    //       すべて this を親として作成されているため、Qtの親子関係により自動破棄される
}

void AnalysisFlowController::start(const Deps& d, KifuAnalysisDialog* dlg)
{
    if (!d.sfenRecord || d.sfenRecord->isEmpty()) {
        if (d.displayError) d.displayError(tr("内部エラー: sfenRecord が未準備です。棋譜読み込み後に実行してください。"));
        return;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(tr("内部エラー: 解析モデルが未準備です。"));
        return;
    }
    if (!d.usi) {
        if (d.displayError) d.displayError(tr("内部エラー: Usi インスタンスが未初期化です。"));
        return;
    }
    if (!dlg) return;

    // Cache deps
    m_sfenHistory    = d.sfenRecord;
    m_moveRecords   = d.moveRecords;
    m_recordModel   = d.recordModel;
    m_analysisModel = d.analysisModel;
    m_usi           = d.usi;
    m_logModel      = d.logModel;
    m_activePly     = d.activePly;
    m_blackPlayerName = d.blackPlayerName;
    m_whitePlayerName = d.whitePlayerName;
    m_usiMoves      = d.usiMoves;
    m_boardFlipped  = d.boardFlipped;
    m_err           = d.displayError;

    // 前回の解析結果をクリア
    if (m_analysisModel) {
        m_analysisModel->clearAllItems();
    }

    // sfenRecordとusiMovesの整合性をチェック（デバッグ用）
    const qsizetype sfenSize = m_sfenHistory ? m_sfenHistory->size() : 0;
    const qsizetype usiSize = m_usiMoves ? m_usiMoves->size() : 0;
    qCDebug(lcAnalysis).noquote() << "sfenRecord.size=" << sfenSize
                                  << "usiMoves.size=" << usiSize
                                  << "(expected: sfenSize == usiSize + 1)";
    if (m_usiMoves && sfenSize != usiSize + 1) {
        qCWarning(lcAnalysis).noquote() << "sfenRecord and usiMoves size mismatch!"
                                        << "Will use recordModel fallback for lastUsiMove extraction.";
        // 注意: m_usiMovesをnullptrにしない。境界チェックで対応し、範囲外の場合は棋譜表記から抽出する
    }

    m_resultHandler->reset(); // 一時結果・確定結果・差分用前回値をリセット
    m_stoppedByUser = false; // 中止フラグをリセット

    // Coordinator（初回のみ作成、シグナル接続は毎回）
    AnalysisCoordinator::Deps cd;
    cd.sfenRecord = m_sfenHistory;
    if (!m_coord) {
        m_coord = new AnalysisCoordinator(cd, this);
    } else {
        // 2回目以降の解析では依存関係を更新（sfenRecordが変わっている可能性あり）
        m_coord->setDeps(cd);
    }

    // (C) 進捗を結果モデルへ投入（毎回再接続）
    QObject::disconnect(m_coord, &AnalysisCoordinator::analysisProgress, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::analysisProgress,
        this,    &AnalysisFlowController::onAnalysisProgress,
        Qt::UniqueConnection
        );

    // (C-2) position準備時に盤面データを更新
    QObject::disconnect(m_coord, &AnalysisCoordinator::positionPrepared, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::positionPrepared,
        this,    &AnalysisFlowController::onPositionPrepared,
        Qt::UniqueConnection
        );

    // (C-3) 解析完了時に最後の結果を発行
    QObject::disconnect(m_coord, &AnalysisCoordinator::analysisFinished, nullptr, nullptr);
    QObject::connect(
        m_coord, &AnalysisCoordinator::analysisFinished,
        this,    &AnalysisFlowController::onAnalysisFinished,
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
        this,  &AnalysisFlowController::onBestMoveReceived,
        Qt::UniqueConnection
        );

    // (B-2) Usi::infoLineReceived → AC へ直接通知
    QObject::disconnect(m_usi, &Usi::infoLineReceived, nullptr, nullptr);
    QObject::connect(
        m_usi, &Usi::infoLineReceived,
        this,  &AnalysisFlowController::onInfoLineReceived,
        Qt::UniqueConnection
        );

    // (B-3) Usi::thinkingInfoUpdated → 漢字PV取得用
    QObject::disconnect(m_usi, &Usi::thinkingInfoUpdated, nullptr, nullptr);
    QObject::connect(
        m_usi, &Usi::thinkingInfoUpdated,
        this,  &AnalysisFlowController::onThinkingInfoUpdated,
        Qt::UniqueConnection
        );

    // (B-4) Usi::errorOccurred → エンジンクラッシュ時に解析を停止
    QObject::connect(
        m_usi, &Usi::errorOccurred,
        this,  &AnalysisFlowController::onEngineError,
        Qt::UniqueConnection
        );

    // 結果ビュー（Presenter）— 外部から渡された場合はそれを使用
    if (!m_presenter) {
        if (d.presenter) {
            m_presenter = d.presenter;
        } else {
            m_presenter = new AnalysisResultsPresenter(this);
        }
        // 中止ボタンのシグナルを接続
        QObject::connect(
            m_presenter, &AnalysisResultsPresenter::stopRequested,
            this,        &AnalysisFlowController::stop,
            Qt::UniqueConnection
            );
        // 行ダブルクリックのシグナルを接続（読み筋表示用）
        QObject::connect(
            m_presenter, &AnalysisResultsPresenter::rowDoubleClicked,
            this,        &AnalysisFlowController::onResultRowDoubleClicked,
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
    applyDialogOptions(dlg);

    // エンジン起動
    const int  engineIdx = dlg->engineNumber();
    const auto engines   = dlg->engineList();
    if (engineIdx < 0 || engineIdx >= engines.size()) {
        if (m_err) m_err(tr("エンジン選択が不正です。"));
        return;
    }
    const QString enginePath = engines.at(engineIdx).path;
    const QString engineName = dlg->engineName();

    // 思考タブのエンジン名を設定
    if (m_logModel) {
        m_logModel->setEngineName(engineName);
    }

    // USI通信ログのエンジン識別子を設定（"E?" ではなく "E1" と表示されるようにする）
    m_usi->setLogIdentity(QStringLiteral("[E1]"), QString(), engineName);

    m_usi->startAndInitializeEngine(enginePath, engineName); // usi→usiok/setoption/isready→readyok

    // 解析用の盤面データを初期化（info行のPV解析に必要）
    m_usi->prepareBoardDataForAnalysis();

    // 解析結果ハンドラの外部参照を更新
    {
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = m_analysisModel;
        refs.sfenHistory = m_sfenHistory;
        refs.recordModel = m_recordModel;
        refs.usiMoves = m_usiMoves;
        refs.coord = m_coord;
        refs.presenter = m_presenter;
        refs.blackPlayerName = m_blackPlayerName;
        refs.whitePlayerName = m_whitePlayerName;
        refs.boardFlipped = m_boardFlipped;
        m_resultHandler->setRefs(refs);
    }

    // 解析開始
    m_running = true;
    m_coord->startAnalyzeRange();
}

void AnalysisFlowController::stop()
{
    qCDebug(lcAnalysis).noquote() << "stop called, m_running=" << m_running;

    if (!m_running) {
        return;
    }

    m_running = false;
    m_stoppedByUser = true;  // ユーザーによる中止を記録

    if (m_coord) {
        m_coord->stop();
    }

    if (m_usi) {
        m_usi->sendQuitCommand();
    }

    if (m_presenter) {
        m_presenter->setStopButtonEnabled(false);
    }

    Q_EMIT analysisStopped();

    qCDebug(lcAnalysis).noquote() << "analysis stopped";
}

void AnalysisFlowController::applyDialogOptions(KifuAnalysisDialog* dlg)
{
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = dlg->byoyomiSec() * 1000;

    const int sfenSize = static_cast<int>(m_sfenHistory->size());

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

    qCDebug(lcAnalysis).noquote() << "applyDialogOptions: startPly=" << opt.startPly
                                  << "endPly=" << opt.endPly << "maxEndPly=" << maxEndPly;

    opt.multiPV    = 1;    // ダイアログ未対応なら 1 固定
    opt.centerTree = true;

    m_coord->setOptions(opt);
}

// ======================
//  スロット実装（非ラムダ）
// ======================

void AnalysisFlowController::onUsiCommLogChanged()
{
    if (!m_coord || !m_logModel) return;

    const QString line = m_logModel->usiCommLog().trimmed();
    if (line.startsWith(QStringLiteral("info "))) {
        m_coord->onEngineInfoLine(line);
    }
    // bestmoveはonBestMoveReceived_で処理するため、ここでは処理しない
}

void AnalysisFlowController::onBestMoveReceived()
{
    qCDebug(lcAnalysis).noquote() << "onBestMoveReceived";

    // ThinkingInfoPresenterのバッファをフラッシュして、最新の漢字PVを取得
    if (m_usi) {
        m_usi->flushThinkingInfoBuffer();
    }

    // 一時保存した結果を確定
    m_resultHandler->commitPendingResult();

    if (!m_coord) return;
    m_coord->onEngineBestmoveReceived(QString());
}

void AnalysisFlowController::onInfoLineReceived(const QString& line)
{
    // info行を受け取り、AnalysisCoordinatorに転送
    // 漢字PV変換はThinkingInfoPresenterが行い、onThinkingInfoUpdated_で受け取る
    qCDebug(lcAnalysis).noquote() << "onInfoLineReceived: line=" << line.left(80);
    if (!m_coord) {
        qCDebug(lcAnalysis).noquote() << "onInfoLineReceived: m_coord is null!";
        return;
    }

    m_coord->onEngineInfoLine(line);
}

void AnalysisFlowController::onThinkingInfoUpdated(const QString& /*time*/, const QString& /*depth*/,
                                                    const QString& /*nodes*/, const QString& /*score*/,
                                                    const QString& pvKanjiStr, const QString& /*usiPv*/,
                                                    const QString& /*baseSfen*/, int /*multipv*/, int /*scoreCp*/)
{
    if (!pvKanjiStr.isEmpty()) {
        m_resultHandler->updatePendingPvKanji(pvKanjiStr);
    }
}

// onPositionPrepared() は analysisflowcontroller_position.cpp に実装

void AnalysisFlowController::onAnalysisProgress(int ply, int /*depth*/, int /*seldepth*/,
                                                 int scoreCp, int mate,
                                                 const QString& pv, const QString& /*raw*/)
{
    m_resultHandler->updatePending(ply, scoreCp, mate, pv);
}


void AnalysisFlowController::onAnalysisFinished(AnalysisCoordinator::Mode /*mode*/)
{
    qCDebug(lcAnalysis).noquote() << "analysis finished, m_stoppedByUser=" << m_stoppedByUser;

    // 最後の結果をGUIに反映
    if (m_resultHandler->lastCommittedPly() >= 0) {
        qCDebug(lcAnalysis).noquote() << "emitting final analysisProgressReported: ply="
                                      << m_resultHandler->lastCommittedPly() << "scoreCp=" << m_resultHandler->lastCommittedScoreCp();
        Q_EMIT analysisProgressReported(m_resultHandler->lastCommittedPly(), m_resultHandler->lastCommittedScoreCp());
        m_resultHandler->resetLastCommitted();
    }

    m_running = false;

    // エンジンプロセスを終了させる
    if (m_usi) {
        m_usi->sendQuitCommand();
    }

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

bool AnalysisFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    qCDebug(lcAnalysis).noquote() << "runWithDialog START";
    qCDebug(lcAnalysis).noquote() << "d.gameController=" << d.gameController;
    qCDebug(lcAnalysis).noquote() << "d.usi=" << d.usi;

    // 依存の必須チェック（usi以外）
    if (!d.sfenRecord || d.sfenRecord->isEmpty()) {
        if (d.displayError) d.displayError(tr("内部エラー: sfenRecord が未準備です。棋譜読み込み後に実行してください。"));
        return false;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(tr("内部エラー: 解析モデルが未準備です。"));
        return false;
    }

    // ダイアログを生成してユーザに選択してもらう
    KifuAnalysisDialog dlg(parent);

    // 最大手数を設定
    // 注: sfenRecordには終局指し手（投了、中断など）は含まれないため、
    //     sfenSize - 1 が最後の指し手の局面インデックスとなる
    int maxPly = static_cast<int>(d.sfenRecord->size()) - 1;
    dlg.setMaxPly(qMax(0, maxPly));

    const int result = dlg.exec();
    if (result != QDialog::Accepted) return false;

    // GameControllerを保持
    m_gameController = d.gameController;
    qCDebug(lcAnalysis).noquote() << "m_gameController=" << m_gameController;
    if (m_gameController) {
        qCDebug(lcAnalysis).noquote() << "m_gameController->board()=" << m_gameController->board();
    }

    // Usiが渡されていない場合は内部で生成
    Deps actualDeps = d;
    if (!actualDeps.usi) {
        qCDebug(lcAnalysis).noquote() << "Creating internal Usi instance...";

        // 既存の内部Usiを破棄（メモリリーク防止）
        if (m_ownsUsi && m_usi) {
            m_usi->disconnect();
            m_usi->deleteLater();
            m_usi = nullptr;
            m_ownsUsi = false;
        }

        // ログモデル: 渡されたものがあればそれを使用、なければ生成
        UsiCommLogModel* logModelToUse = d.logModel;
        if (!logModelToUse) {
            if (!m_ownedLogModel) {
                m_ownedLogModel = new UsiCommLogModel(this);
            } else {
                m_ownedLogModel->clear();
            }
            logModelToUse = m_ownedLogModel;
        }

        // ThinkingModel: 渡されたものがあればそれを使用、なければ生成
        ShogiEngineThinkingModel* thinkingModelToUse = d.thinkingModel;
        if (!thinkingModelToUse) {
            if (!m_ownedThinkingModel) {
                m_ownedThinkingModel = new ShogiEngineThinkingModel(this);
            } else {
                m_ownedThinkingModel->clearAllItems();
            }
            thinkingModelToUse = m_ownedThinkingModel;
        }

        // Usiインスタンスを生成（GameControllerを渡して盤面情報を取得可能にする）
        m_usi = new Usi(logModelToUse, thinkingModelToUse, m_gameController, m_playModeForAnalysis, this);
        m_ownsUsi = true;

        actualDeps.usi = m_usi;
        actualDeps.logModel = logModelToUse;
        actualDeps.thinkingModel = thinkingModelToUse;
        qCDebug(lcAnalysis).noquote() << "Internal Usi created:" << m_usi;
        qCDebug(lcAnalysis).noquote() << "Using logModel:" << logModelToUse;
        qCDebug(lcAnalysis).noquote() << "Using thinkingModel:" << thinkingModelToUse;
    }

    // 以降は既存の start(...) に委譲（Presenter への表示や接続も start 側で実施）
    start(actualDeps, &dlg);
    return true;
}

void AnalysisFlowController::onResultRowDoubleClicked(int row)
{
    m_resultHandler->showPvBoardDialog(row);
}


void AnalysisFlowController::onEngineError(const QString& msg)
{
    if (!m_running) return;

    qCWarning(lcAnalysis).noquote() << "Engine error during analysis:" << msg;

    // エラーメッセージを表示
    if (m_err) {
        m_err(tr("エンジンエラー: %1").arg(msg));
    }

    // 解析を停止（stop()内でanalysisStopped()が発行される）
    stop();
}
