/// @file analysissessionhandler.cpp
/// @brief 検討・詰み探索セッション管理ハンドラクラスの実装

#include "analysissessionhandler.h"
#include "usi.h"
#include "shogienginethinkingmodel.h"
#include "logcategories.h"

#include <QTimer>

AnalysisSessionHandler::AnalysisSessionHandler(QObject* parent)
    : QObject(parent)
{
}

void AnalysisSessionHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
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
    if (opt.previousFileTo > 0 && opt.previousRankTo > 0) {
        engine->setPreviousFileTo(opt.previousFileTo);
        engine->setPreviousRankTo(opt.previousRankTo);
        qCDebug(lcGame).noquote() << "setupModeSpecificWiring: setPreviousFileTo/RankTo:"
                           << opt.previousFileTo << "/" << opt.previousRankTo;
    } else {
        qCWarning(lcGame).noquote() << "setupModeSpecificWiring: previousFileTo/RankTo not set (values are 0)";
    }

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
        // 注意: m_restartInProgress はここではリセットしない
        // （restartConsiderationDeferred から呼ばれた場合は呼び出し元でリセットする）
        m_enginePath = opt.enginePath;
        m_engineName = opt.engineName;
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

    engine->executeAnalysisCommunication(pos, opt.byoyomiMs, opt.multiPV);
    qCDebug(lcGame).noquote() << "startCommunication EXIT (executeAnalysisCommunication)";
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

    // 新しいMultiPV値を保存し、再開フラグを設定
    m_multiPV = multiPV;
    m_restartPending = true;

    // 検討タブ用モデルをクリア
    if (m_modelPtr) {
        m_modelPtr->clearAllItems();
    }

    // エンジンを停止（bestmove を受信後に再開する）
    if (engine) {
        qCDebug(lcGame).noquote() << "updateMultiPV: sending stop to restart with new MultiPV";
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

    // 検討タブ用モデルをクリア
    if (m_modelPtr) {
        m_modelPtr->clearAllItems();
    }

    // エンジンが待機状態の場合、直接新しい局面で検討を再開（stop不要）
    if (m_waiting && engine) {
        qCDebug(lcGame).noquote() << "updatePosition: engine is waiting, resuming with new position";
        m_waiting = false;  // 待機状態を解除

        // 前回の移動先を設定（「同」表記のため）
        if (previousFileTo > 0 && previousRankTo > 0) {
            engine->setPreviousFileTo(previousFileTo);
            engine->setPreviousRankTo(previousRankTo);
        }

        // 最後の指し手を設定（読み筋表示ウィンドウのハイライト用）
        if (!lastUsiMove.isEmpty()) {
            engine->setLastUsiMove(lastUsiMove);
        }

        // 既存エンジンに直接コマンドを送信（非ブロッキング）
        engine->sendAnalysisCommands(newPositionStr, m_byoyomiMs, m_multiPV);
        return true;
    }

    // エンジンが稼働中の場合、停止して再開フラグを設定
    m_restartPending = true;
    qCDebug(lcGame).noquote() << "updatePosition: set m_restartPending=true";

    // エンジンを停止（bestmove を受信後に再開する）
    if (engine) {
        qCDebug(lcGame).noquote() << "updatePosition: sending stop to restart with new position";
        engine->sendStopCommand();
    }

    return true;
}

// ============================================================
// エラー処理
// ============================================================

bool AnalysisSessionHandler::handleEngineError(const QString& errorMsg)
{
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
    finalizeTsumeSearch(tr("不明（解析不能）"));
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
        qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: scheduling restart (restart was pending)";

        // 再開処理を次のイベントループに遅延
        QTimer::singleShot(0, this, &AnalysisSessionHandler::restartConsiderationDeferred);
        return;
    }

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
    if (!m_inConsiderationMode) {
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

    // 前回の移動先を設定（「同」表記のため）
    if (m_previousFileTo > 0 && m_previousRankTo > 0) {
        m_engine->setPreviousFileTo(m_previousFileTo);
        m_engine->setPreviousRankTo(m_previousRankTo);
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: setPreviousFileTo/RankTo:"
                           << m_previousFileTo << "/" << m_previousRankTo;
    }

    // 最後の指し手を設定（読み筋表示ウィンドウのハイライト用）
    if (!m_lastUsiMove.isEmpty()) {
        m_engine->setLastUsiMove(m_lastUsiMove);
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: setLastUsiMove:"
                           << m_lastUsiMove;
    }

    // 既存エンジンにコマンドを送信
    m_engine->sendAnalysisCommands(m_positionStr, m_byoyomiMs, m_multiPV);

    qCDebug(lcGame).noquote() << "restartConsiderationDeferred EXIT";
}
