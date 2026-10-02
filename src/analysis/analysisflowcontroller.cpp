/// @file analysisflowcontroller.cpp
/// @brief 棋譜解析フローコントローラクラスの実装

#include "analysisflowcontroller.h"

#include "analysiscoordinator.h"
#include "analysisresulthandler.h"
#include "analysisresultspresenter.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifurecordlistmodel.h"
#include "usi.h"
#include "usicommlogmodel.h"

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

void AnalysisFlowController::emitAnalysisStoppedOnce()
{
    if (m_analysisStoppedEmitted) {
        return;
    }
    m_analysisStoppedEmitted = true;
    Q_EMIT analysisStopped();
}

void AnalysisFlowController::start(const Deps& d, KifuAnalysisDialog* dlg)
{
    m_lastStartSucceeded = false;
    m_analysisStoppedEmitted = false;

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
    // 解析中に表示する分岐を切り替えても、対象局面と指し手を変えない。
    m_sfenSnapshot = *d.sfenRecord;
    m_sfenHistory = &m_sfenSnapshot;
    m_usiMovesSnapshot = d.usiMoves ? *d.usiMoves : QStringList();
    m_recordSnapshot = std::make_unique<KifuRecordListModel>();
    if (d.recordModel) {
        for (int row = 0; row < d.recordModel->rowCount(); ++row) {
            const auto* item = d.recordModel->item(row);
            m_recordSnapshot->appendItem(new KifuDisplay(item ? item->currentMove() : QString(), QString()));
        }
    }
    m_recordModel = m_recordSnapshot.get();
    m_analysisLineIndex = d.lineIndex;
    m_analysisModel = d.analysisModel;
    m_usi           = d.usi;
    m_logModel      = d.logModel;
    m_blackPlayerName = d.blackPlayerName;
    m_whitePlayerName = d.whitePlayerName;
    m_usiMoves      = d.usiMoves ? &m_usiMovesSnapshot : nullptr;
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
    if (m_connCoordAnalysisProgress) {
        QObject::disconnect(m_connCoordAnalysisProgress);
    }
    m_connCoordAnalysisProgress = QObject::connect(
        m_coord, &AnalysisCoordinator::analysisProgress,
        this,    &AnalysisFlowController::onAnalysisProgress);

    // (C-2) position準備時に盤面データを更新
    if (m_connCoordPositionPrepared) {
        QObject::disconnect(m_connCoordPositionPrepared);
    }
    m_connCoordPositionPrepared = QObject::connect(
        m_coord, &AnalysisCoordinator::positionPrepared,
        this,    &AnalysisFlowController::onPositionPrepared);

    // (C-3) 解析完了時に最後の結果を発行
    if (m_connCoordAnalysisFinished) {
        QObject::disconnect(m_connCoordAnalysisFinished);
    }
    m_connCoordAnalysisFinished = QObject::connect(
        m_coord, &AnalysisCoordinator::analysisFinished,
        this,    &AnalysisFlowController::onAnalysisFinished);

    // (A) AC → エンジンへ USI 文字列を橋渡し（毎回再接続）
    if (m_connCoordRequestSendUsi) {
        QObject::disconnect(m_connCoordRequestSendUsi);
    }
    m_connCoordRequestSendUsi = QObject::connect(
        m_coord, &AnalysisCoordinator::requestSendUsiCommand,
        m_usi,   &Usi::sendRaw);

    // (B-1) Usi::bestMoveReceived → AC へ直接通知
    if (m_connUsiBestMove) {
        QObject::disconnect(m_connUsiBestMove);
    }
    m_connUsiBestMove = QObject::connect(
        m_usi, &Usi::bestMoveReceived,
        this,  &AnalysisFlowController::onBestMoveReceived);

    // (B-2) Usi::infoLineReceived → AC へ直接通知
    if (m_connUsiInfoLine) {
        QObject::disconnect(m_connUsiInfoLine);
    }
    m_connUsiInfoLine = QObject::connect(
        m_usi, &Usi::infoLineReceived,
        this,  &AnalysisFlowController::onInfoLineReceived);

    // (B-3) Usi::thinkingInfoUpdated → 漢字PV取得用
    if (m_connUsiThinkingInfo) {
        QObject::disconnect(m_connUsiThinkingInfo);
    }
    m_connUsiThinkingInfo = QObject::connect(
        m_usi, &Usi::thinkingInfoUpdated,
        this,  &AnalysisFlowController::onThinkingInfoUpdated);

    // (B-4) Usi::errorOccurred → エンジンクラッシュ時に解析を停止
    if (m_connUsiError) {
        QObject::disconnect(m_connUsiError);
    }
    m_connUsiError = QObject::connect(
        m_usi, &Usi::errorOccurred,
        this,  &AnalysisFlowController::onEngineError);

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
            this,        &AnalysisFlowController::onResultRowSelected,
            Qt::UniqueConnection
            );
    }
    m_presenter->showWithModel(m_analysisModel);

    // ダイアログ設定を AC オプションへ反映
    applyDialogOptions(dlg);

    // エンジン起動
    const int  engineIdx = dlg->engineNumber();
    const auto engines   = dlg->engineList();
    if (engineIdx < 0 || engineIdx >= engines.size()) {
        m_presenter->setStopButtonEnabled(false);
        if (m_err) m_err(tr("エンジン選択が不正です。"));
        return;
    }
    const QString enginePath = engines.at(engineIdx).path;
    const QString engineName = dlg->engineName();
    emit analysisEngineNameChanged(engineName);
    emit analysisStarted(m_analysisLineIndex);

    // 思考タブのエンジン名を設定
    if (m_logModel) {
        m_logModel->setEngineName(engineName);
    }

    // USI通信ログのエンジン識別子を設定（"E?" ではなく "E1" と表示されるようにする）
    m_usi->setLogIdentity(QStringLiteral("[E1]"), QString(), engineName);

    if (!m_usi->startAndInitializeEngineAsync(enginePath, engineName)) { // usi→usiok/setoption/isready→readyok
        if (m_presenter) {
            m_presenter->setStopButtonEnabled(false);
        }
        if (m_err) {
            m_err(tr("エンジン初期化に失敗しました。エンジン設定を確認してください。"));
        }
        return;
    }

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
    m_lastStartSucceeded = true;
    m_running = true;
    connect(m_usi, &Usi::engineInitialized, this, &AnalysisFlowController::onEngineInitialized, Qt::UniqueConnection);
    if (!m_usi->isInitializing()) onEngineInitialized();
}

void AnalysisFlowController::onEngineInitialized()
{
    if (m_running && m_coord) m_coord->startAnalyzeRange();
}

void AnalysisFlowController::stop()
{
    qCDebug(lcAnalysis).noquote() << "stop called, m_running=" << m_running;

    if (!m_running) {
        return;
    }

    m_stoppedByUser = true;  // ユーザーによる中止を記録

    if (m_coord) {
        m_coord->stop();
        if (!m_running) {
            qCDebug(lcAnalysis).noquote() << "analysis stopped";
            return;
        }
    }

    // フォールバック: AnalysisCoordinator::analysisFinished が返らない場合
    m_running = false;
    if (m_usi) {
        m_usi->sendQuitCommand();
    }

    if (m_presenter) {
        m_presenter->setStopButtonEnabled(false);
    }

    emitAnalysisStoppedOnce();

    qCDebug(lcAnalysis).noquote() << "analysis stopped";
}

// ======================
//  スロット実装（非ラムダ）
// ======================

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
                                                 const QString& pv, const QString& raw)
{
    const QStringList tokens = raw.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const qsizetype scoreIndex = tokens.indexOf(QStringLiteral("score"));
    QString rawMate;
    if (scoreIndex >= 0 && scoreIndex + 2 < tokens.size()
        && tokens.at(scoreIndex + 1) == QStringLiteral("mate")) {
        rawMate = tokens.at(scoreIndex + 2);
    }
    m_resultHandler->updatePending(ply, scoreCp, mate, pv, rawMate);
}


void AnalysisFlowController::onAnalysisFinished(AnalysisCoordinator::Mode /*mode*/)
{
    qCDebug(lcAnalysis).noquote() << "analysis finished, m_stoppedByUser=" << m_stoppedByUser;

    // 最後の結果をGUIに反映
    if (m_resultHandler->lastCommittedPly() >= 0) {
        qCDebug(lcAnalysis).noquote() << "emitting final analysisProgressReported: ply="
                                      << m_resultHandler->lastCommittedPly() << "scoreCp=" << m_resultHandler->lastCommittedScoreCp();
        Q_EMIT analysisProgressReported(m_resultHandler->lastCommittedPly(), m_resultHandler->lastCommittedScoreCp(),
                                       m_resultHandler->lastCommittedMate());
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

    emitAnalysisStoppedOnce();
}

void AnalysisFlowController::onResultRowDoubleClicked(int row)
{
    m_resultHandler->showPvBoardDialog(row);
}

void AnalysisFlowController::onResultRowSelected(int row)
{
    if (!m_running && m_analysisModel && row >= 0 && row < m_analysisModel->rowCount()) {
        emit analysisResultRowSelected(m_analysisStartPly + row);
    }
}


void AnalysisFlowController::onEngineError(const QString& msg)
{
    if (!m_running) return;

    qCWarning(lcAnalysis).noquote() << "Engine error during analysis:" << msg;

    // エラーメッセージを表示
    if (m_err) {
        m_err(tr("エンジンエラー: %1").arg(msg));
    }

    // 解析を停止（analysisStopped は一度だけ通知される）
    stop();
}
