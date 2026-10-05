/// @file analysissessionhandler.cpp
/// @brief 検討・詰み探索セッション管理ハンドラクラスの実装

#include "analysissessionhandler.h"
#include "usi.h"
#include "shogienginethinkingmodel.h"
#include "logcategories.h"

#include <QTimer>
#include <QSignalBlocker>

AnalysisSessionHandler::AnalysisSessionHandler(QObject* parent)
    : QObject(parent)
{
}

void AnalysisSessionHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// 検討フルライフサイクル
// ============================================================

bool AnalysisSessionHandler::startFullAnalysis(const MatchCoordinator::AnalysisOptions& opt)
{
    qCDebug(lcGame).noquote() << "startFullAnalysis ENTER:"
                       << "mode=" << static_cast<int>(opt.mode)
                       << "byoyomiMs=" << opt.byoyomiMs
                       << "multiPV=" << opt.multiPV
                       << "inConsideration=" << m_inConsiderationMode;

    if (m_starting || (m_hooks.isShutdownInProgress && m_hooks.isShutdownInProgress())) {
        qCDebug(lcGame).noquote() << "startFullAnalysis: engine shutdown in progress";
        return false;
    }

    m_starting = true;
    m_startCancelled = false;
    m_startupError.clear();
    if (m_hooks.setPlayMode) m_hooks.setPlayMode(opt.mode);

    qCDebug(lcGame).noquote() << "startFullAnalysis: destroying old engines:" << opt.enginePath;
    if (m_hooks.destroyEnginesAll) m_hooks.destroyEnginesAll();

    Usi* usi = m_hooks.createAnalysisEngine ? m_hooks.createAnalysisEngine(opt) : nullptr;
    if (!usi) {
        m_starting = false;
        return false;
    }

    // 初期化前に状態とモデルを準備する。待機中の中止・エラーでも安全に復帰でき、
    // 前回の読み筋を新しい探索結果と誤認させない。
    setupModeSpecificWiring(usi, opt);
    const bool startupAccepted = m_hooks.initAndStartEngine
        && m_hooks.initAndStartEngine(1, opt.enginePath, opt.engineName);
    m_starting = false;
    if (m_startCancelled) {
        stopFullAnalysis();
        return false;
    }
    if (!startupAccepted) {
        handleEngineError(m_startupError.isEmpty() ? opt.enginePath : m_startupError);
        return false;
    }
    if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName, QString());

    if (opt.mode == PlayMode::ConsiderationMode) {
        // 初期化待機中の局面・候補手変更も反映する。
        auto currentOptions = opt;
        currentOptions.positionStr = m_positionStr;
        currentOptions.multiPV = m_multiPV;
        usi->setPreviousFileTo(m_previousFileTo);
        usi->setPreviousRankTo(m_previousRankTo);
        usi->setLastUsiMove(m_lastUsiMove);
        startCommunication(usi, currentOptions);
    } else {
        usi->setLastUsiMove(opt.lastUsiMove);
        startCommunication(usi, opt);
    }

    qCDebug(lcGame).noquote() << "startFullAnalysis EXIT";
    return true;
}

void AnalysisSessionHandler::stopFullAnalysis()
{
    qCDebug(lcGame).noquote() << "stopFullAnalysis called";

    if (m_starting) {
        m_startCancelled = true;
        if (m_engine) m_engine->cancelCurrentOperation();
        return;
    }
    if (m_hooks.isShutdownInProgress && m_hooks.isShutdownInProgress()) return;
    if (m_hooks.setShutdownInProgress) m_hooks.setShutdownInProgress(true);

    const bool wasConsideration = m_inConsiderationMode;
    const bool wasTsumeSearch = m_inTsumeSearchMode;
    {
        // 終了待ちのイベントループで再開要求を受けないよう状態を先に停止する。
        // UIへの終了通知はエンジン破棄後に行い、開始可能な時点でボタンを戻す。
        const QSignalBlocker blocker(this);
        handleStop();
    }

    if (m_hooks.destroyEnginesKeepModels) m_hooks.destroyEnginesKeepModels();

    if (m_hooks.setShutdownInProgress) m_hooks.setShutdownInProgress(false);
    if (wasConsideration) emit considerationModeEnded();
    if (wasTsumeSearch) emit tsumeSearchModeEnded();
}

// ============================================================
// モード固有の配線と状態保存
// ============================================================

void AnalysisSessionHandler::setupModeSpecificWiring(Usi* engine,
                                                     const MatchCoordinator::AnalysisOptions& opt)
{
    m_engine = engine;

    // --- 詰み探索の配線（TsumiSearchMode のときのみ） ---
    m_inTsumeSearchMode = (opt.mode == PlayMode::TsumiSearchMode);
    if (m_inTsumeSearchMode && engine) {
        connect(engine, &Usi::checkmateSolved,
                this,   &AnalysisSessionHandler::onCheckmateSolved,
                Qt::UniqueConnection);
        connect(engine, &Usi::checkmateNoMate,
                this,   &AnalysisSessionHandler::onCheckmateNoMate,
                Qt::UniqueConnection);
        connect(engine, &Usi::checkmateNotImplemented,
                this,   &AnalysisSessionHandler::onCheckmateNotImplemented,
                Qt::UniqueConnection);
        connect(engine, &Usi::checkmateUnknown,
                this,   &AnalysisSessionHandler::onCheckmateUnknown,
                Qt::UniqueConnection);
        // bestmove 受信時も通知（checkmate 非対応エンジン用）
        connect(engine, &Usi::bestMoveReceived,
                this,   &AnalysisSessionHandler::onTsumeBestMoveReceived,
                Qt::UniqueConnection);
    }

    // --- 検討タブ用モデルを設定 ---
    if (opt.considerationModel && opt.mode == PlayMode::ConsiderationMode) {
        engine->setConsiderationModel(opt.considerationModel, opt.multiPV);
    }

    // --- 前回の移動先を設定（「同」表記のため） ---
    qCDebug(lcGame).noquote() << "setupModeSpecificWiring: opt.previousFileTo=" << opt.previousFileTo
                       << "opt.previousRankTo=" << opt.previousRankTo;
    // 開始局面では0を設定し、前局面の「同」判定を残さない。
    engine->setPreviousFileTo(opt.previousFileTo);
    engine->setPreviousRankTo(opt.previousRankTo);

    // --- 検討モードの場合、フラグを設定し bestmove を接続 ---
    if (opt.mode == PlayMode::ConsiderationMode) {
        m_inConsiderationMode = true;
        // 検討の状態を保存（MultiPV変更時・ポジション変更時の再開用）
        m_positionStr = opt.positionStr;
        m_byoyomiMs = opt.byoyomiMs;
        m_multiPV = opt.multiPV;
        m_modelPtr = opt.considerationModel;
        m_restartPending = false;
        m_waiting = false;  // 待機フラグをリセット
        m_restartInProgress = false;
        m_previousFileTo = opt.previousFileTo;
        m_previousRankTo = opt.previousRankTo;
        m_lastUsiMove = opt.lastUsiMove;

        connect(engine, &Usi::bestMoveReceived,
                this,   &AnalysisSessionHandler::onConsiderationBestMoveReceived,
                Qt::UniqueConnection);
    }
}

void AnalysisSessionHandler::startCommunication(Usi* engine,
                                                const MatchCoordinator::AnalysisOptions& opt)
{
    QString pos = opt.positionStr; // "position sfen <...>"
    qCDebug(lcGame).noquote() << "startCommunication: about to start, byoyomiMs=" << opt.byoyomiMs;

    if (opt.mode == PlayMode::TsumiSearchMode) {
        engine->executeTsumeCommunication(pos, opt.byoyomiMs);
        qCDebug(lcGame).noquote() << "startCommunication EXIT (executeTsumeCommunication)";
        return;
    }

    if (opt.mode == PlayMode::ConsiderationMode) {
        // 検討は非ブロッキングで開始し、UIフリーズを避ける
        engine->sendAnalysisCommands(pos, opt.byoyomiMs, opt.multiPV);
        qCDebug(lcGame).noquote() << "startCommunication EXIT (sendAnalysisCommands)";
        return;
    }

    // 解析モードも非ブロッキングで開始する（bestmove待機はシグナルで処理）。
    engine->sendAnalysisCommands(pos, opt.byoyomiMs, opt.multiPV);
    qCDebug(lcGame).noquote() << "startCommunication EXIT (sendAnalysisCommands)";
}

// ============================================================
// 停止処理
// ============================================================

void AnalysisSessionHandler::handleStop()
{
    // 検討モード中なら終了シグナルを発火
    if (m_inConsiderationMode) {
        m_inConsiderationMode = false;
        m_restartInProgress = false;
        m_restartPending = false;
        m_waiting = false;
        emit considerationModeEnded();
    }

    const bool wasTsumeSearch = m_inTsumeSearchMode;
    m_inTsumeSearchMode = false;
    if (wasTsumeSearch) {
        emit tsumeSearchModeEnded();
    }
}

// ============================================================
// MultiPV 変更
// ============================================================

void AnalysisSessionHandler::updateMultiPV(Usi* engine, int multiPV)
{
    qCDebug(lcGame).noquote() << "updateMultiPV called: multiPV=" << multiPV;

    // 検討モード中でない場合は無視
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "updateMultiPV: not in consideration mode, ignoring";
        return;
    }

    // 値が同じなら何もしない
    if (m_multiPV == multiPV) {
        qCDebug(lcGame).noquote() << "updateMultiPV: same value, ignoring";
        return;
    }

    m_multiPV = qBound(1, multiPV, 10);
    requestConsiderationRestart(engine);
}

void AnalysisSessionHandler::requestConsiderationRestart(Usi* engine)
{
    if (m_modelPtr) m_modelPtr->clearAllItems();
    if (!engine || m_starting || m_restartPending || m_restartInProgress) return;

    if (engine->isInitializing()) {
        engine->sendStopCommand();
        m_restartInProgress = true;
        QTimer::singleShot(0, this, &AnalysisSessionHandler::restartConsiderationDeferred);
    } else if (m_waiting) {
        // 時間切れ後はstopに応答するbestmoveが来ないため、そのまま再開する。
        m_restartInProgress = true;
        QTimer::singleShot(0, this, &AnalysisSessionHandler::restartConsiderationDeferred);
    } else {
        m_restartPending = true;
        engine->sendStopCommand();
    }
}

// ============================================================
// ポジション変更
// ============================================================

bool AnalysisSessionHandler::updatePosition(Usi* engine, const QString& newPositionStr,
                                            int previousFileTo, int previousRankTo,
                                            const QString& lastUsiMove)
{
    qCDebug(lcGame).noquote() << "updatePosition called:"
                       << "m_inConsiderationMode=" << m_inConsiderationMode
                       << "m_waiting=" << m_waiting
                       << "m_restartPending=" << m_restartPending
                       << "engine=" << (engine ? "valid" : "null")
                       << "previousFileTo=" << previousFileTo
                       << "previousRankTo=" << previousRankTo
                       << "lastUsiMove=" << lastUsiMove;

    // 検討モード中でない場合は無視
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "updatePosition: not in consideration mode, ignoring";
        return false;
    }

    // 同じポジションなら何もしない
    if (m_positionStr == newPositionStr) {
        qCDebug(lcGame).noquote() << "updatePosition: same position, ignoring";
        return false;
    }

    // 新しいポジションと前回の移動先を保存
    m_positionStr = newPositionStr;
    m_previousFileTo = previousFileTo;
    m_previousRankTo = previousRankTo;
    m_lastUsiMove = lastUsiMove;

    requestConsiderationRestart(engine);

    return true;
}

// ============================================================
// エラー処理
// ============================================================

bool AnalysisSessionHandler::handleEngineError(const QString& errorMsg)
{
    if (m_starting) {
        // USI初期化のコールスタック上でエンジンを破棄してはならない。
        m_startupError = errorMsg;
        return true;
    }
    // 詰み探索中にエンジンがクラッシュした場合の復旧処理
    if (m_inTsumeSearchMode) {
        m_inTsumeSearchMode = false;
        emit tsumeSearchModeEnded();
        if (m_hooks.showGameOverDialog) {
            m_hooks.showGameOverDialog(tr("詰み探索"), tr("エンジンエラー: %1").arg(errorMsg));
        }
        if (m_hooks.destroyEnginesKeepModels) {
            m_hooks.destroyEnginesKeepModels();
        }
        return true;
    }

    // 検討モード中にエンジンがクラッシュした場合の復旧処理
    if (m_inConsiderationMode) {
        m_inConsiderationMode = false;
        m_restartInProgress = false;
        m_restartPending = false;
        m_waiting = false;
        emit considerationModeEnded();
        if (m_hooks.showGameOverDialog) {
            m_hooks.showGameOverDialog(tr("検討"), tr("エンジンエラー: %1").arg(errorMsg));
        }
        if (m_hooks.destroyEnginesKeepModels) {
            m_hooks.destroyEnginesKeepModels();
        }
        return true;
    }

    return false;
}

void AnalysisSessionHandler::resetOnDestroyEngines()
{
    if (m_inTsumeSearchMode) {
        m_inTsumeSearchMode = false;
        emit tsumeSearchModeEnded();
    }

    if (m_inConsiderationMode) {
        m_inConsiderationMode = false;
        m_restartInProgress = false;
        m_restartPending = false;
        m_waiting = false;
        emit considerationModeEnded();
    }
}

// ============================================================
// 詰み探索スロット
// ============================================================

void AnalysisSessionHandler::finalizeTsumeSearch(const QString& resultMessage)
{
    m_inTsumeSearchMode = false;
    emit tsumeSearchModeEnded();
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), resultMessage);
    }
    if (m_hooks.destroyEnginesKeepModels) {
        m_hooks.destroyEnginesKeepModels();
    }
}

void AnalysisSessionHandler::onCheckmateSolved(const QStringList& pv)
{
    finalizeTsumeSearch(tr("詰みあり（手順 %1 手）").arg(pv.size()));
}

void AnalysisSessionHandler::onCheckmateNoMate()
{
    finalizeTsumeSearch(tr("詰みなし"));
}

void AnalysisSessionHandler::onCheckmateNotImplemented()
{
    finalizeTsumeSearch(tr("（エンジン側）未実装"));
}

void AnalysisSessionHandler::onCheckmateUnknown()
{
    // USI では探索時間を使い切ったときに "checkmate timeout" が返る（結果不明もここに来る）
    finalizeTsumeSearch(tr("時間内に詰みを判定できませんでした"));
}

void AnalysisSessionHandler::onTsumeBestMoveReceived()
{
    // 詰み探索モード中でない場合は無視
    if (!m_inTsumeSearchMode) return;

    finalizeTsumeSearch(tr("探索が完了しました"));
}

// ============================================================
// 検討モードスロット
// ============================================================

void AnalysisSessionHandler::onConsiderationBestMoveReceived()
{
    qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived ENTER:"
                       << "m_inConsiderationMode=" << m_inConsiderationMode
                       << "m_restartPending=" << m_restartPending
                       << "m_waiting=" << m_waiting;

    // 検討モード中でない場合は無視
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: not in consideration mode, ignoring";
        return;
    }

    // 再開フラグがセットされている場合は、新しい設定で検討を再開
    // 注意: executeAnalysisCommunication はブロッキング処理なので、
    // シグナルハンドラ内から直接呼び出すとGUIがフリーズする。
    // QTimer::singleShot で次のイベントループに遅延させる。
    if (m_restartPending) {
        m_restartPending = false;
        m_waiting = true;
        m_restartInProgress = true;
        qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: scheduling restart (restart was pending)";

        // 再開処理を次のイベントループに遅延
        QTimer::singleShot(0, this, &AnalysisSessionHandler::restartConsiderationDeferred);
        return;
    }

    if (m_restartInProgress) return;

    // 検討時間が経過した場合、エンジンを待機状態にして次の局面選択を待つ
    // エンジンを終了せず、検討モードも維持する
    qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: entering waiting state (engine idle)";
    m_waiting = true;  // 待機状態に移行
    m_restartInProgress = false;  // 再入防止フラグをリセット
    // 検討モードは維持（m_inConsiderationMode = true のまま）
    // エンジンは終了しない（destroyEngines を呼ばない）
    // considerationModeEnded も発火しない（ボタンは「検討中止」のまま）
    emit considerationWaitingStarted();  // UIに待機開始を通知（経過タイマー停止用）
    qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived EXIT (waiting state)";
}

void AnalysisSessionHandler::restartConsiderationDeferred()
{
    qCDebug(lcGame).noquote() << "restartConsiderationDeferred ENTER:"
                       << "m_inConsiderationMode=" << m_inConsiderationMode
                       << "m_restartPending=" << m_restartPending
                       << "m_restartInProgress=" << m_restartInProgress
                       << "m_engine=" << (m_engine ? "valid" : "null");

    // 検討モード中でなければ何もしない
    if (!m_inConsiderationMode || !m_restartInProgress) {
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: not in consideration mode, ignoring";
        return;
    }

    // エンジンがなければ何もしない
    if (!m_engine) {
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: no engine, ignoring";
        return;
    }

    // 既存エンジンに直接コマンドを送信（非ブロッキング）
    // エンジンは bestmove 送信後アイドル状態なので、そのまま新しいコマンドを送れる
    qCDebug(lcGame).noquote() << "restartConsiderationDeferred: sending commands to existing engine"
                       << "position=" << m_positionStr
                       << "byoyomiMs=" << m_byoyomiMs
                       << "multiPV=" << m_multiPV;

    // モデルをクリア
    if (m_modelPtr) {
        m_modelPtr->clearAllItems();
    }

    // 待機状態を解除
    m_waiting = false;

    m_engine->setPreviousFileTo(m_previousFileTo);
    m_engine->setPreviousRankTo(m_previousRankTo);
    m_engine->setLastUsiMove(m_lastUsiMove);
    m_restartInProgress = false;

    // 既存エンジンにコマンドを送信
    m_engine->sendAnalysisCommands(m_positionStr, m_byoyomiMs, m_multiPV);

    qCDebug(lcGame).noquote() << "restartConsiderationDeferred EXIT";
}
