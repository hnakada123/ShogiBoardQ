/// @file csaenginecontroller.cpp
/// @brief CSA通信対局用エンジンコントローラの実装

#include "csaenginecontroller.h"
#include "usi.h"
#include "usitimingparams.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "settingscommon.h"
#include "logcategories.h"

#include <QSettings>
#include <utility>

CsaEngineController::CsaEngineController(QObject* parent)
    : QObject(parent)
{
}

CsaEngineController::~CsaEngineController()
{
    cleanup();
}

void CsaEngineController::initialize(const InitParams& params)
{
    cleanup();

    m_gameController = params.gameController;
    m_engineName = params.engineName;

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginReadArray("Engines");
    settings.setArrayIndex(params.engineNumber);
    QString enginePath = settings.value("path").toString();
    settings.endArray();

    if (enginePath.isEmpty()) {
        enginePath = params.enginePath;
    }

    if (enginePath.isEmpty()) {
        onEngineError(tr("エンジンパスが指定されていません"));
        return;
    }

    // USI通信ログモデル：外部注入を優先し、内部生成時は親子所有に統一する
    if (params.commLog) {
        if (m_engineCommLog && m_engineCommLog != params.commLog
            && m_engineCommLog->parent() == this) {
            m_engineCommLog->deleteLater();
        }
        m_engineCommLog = params.commLog;
        m_engineCommLog->clear();
    } else if (!m_engineCommLog || m_engineCommLog->parent() != this) {
        m_engineCommLog = new UsiCommLogModel(this);
    } else {
        m_engineCommLog->clear();
    }

    // 思考モデルも同様に、外部注入または QObject 親子所有に寄せる
    if (params.thinkingModel) {
        if (m_engineThinking && m_engineThinking != params.thinkingModel
            && m_engineThinking->parent() == this) {
            m_engineThinking->deleteLater();
        }
        m_engineThinking = params.thinkingModel;
        m_engineThinking->clearAllItems();
    } else if (!m_engineThinking || m_engineThinking->parent() != this) {
        m_engineThinking = new ShogiEngineThinkingModel(this);
    } else {
        m_engineThinking->clearAllItems();
    }

    m_engine = new Usi(m_engineCommLog, m_engineThinking,
                       m_gameController.data(), this);

    connect(m_engine, &Usi::matchMoveReady, this, &CsaEngineController::onMatchMoveReady);
    connect(m_engine, &Usi::engineInitialized, this, &CsaEngineController::onEngineInitialized);
    connect(m_engine, &Usi::errorOccurred, this, &CsaEngineController::onEngineError);
    connect(m_engine, &Usi::bestMoveResignReceived,
            this, &CsaEngineController::onEngineResign);

    m_engine->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("CSA"), params.engineName);
    (void)m_engine->startAndInitializeEngineAsync(enginePath, params.engineName);
}

void CsaEngineController::onEngineInitialized()
{
    emit logMessage(tr("エンジン %1 を起動しました").arg(m_engineName));
    emit initialized();
}

void CsaEngineController::onEngineError(const QString& message)
{
    cleanup();
    emit logMessage(message, true);
    emit engineError(message);
}

void CsaEngineController::thinkAsync(const ThinkingParams& params)
{
    if (!m_engine || !m_gameController) return;
    m_gameController->setPromote(false);
    const UsiTimingParams timing{params.byoyomiMs, params.btimeStr, params.wtimeStr,
                                 params.bincMs, params.wincMs, params.useByoyomi};
    m_engine->requestMatchMove(params.positionCmd, m_ponderPosition, timing);
}

void CsaEngineController::onMatchMoveReady(const QPoint& from, const QPoint& to,
                                          const QString&, const QString& ponder)
{
    if (!m_engine || sender() != m_engine || !m_gameController) return;
    m_ponderPosition = ponder;
    ThinkingResult result;
    result.from = from;
    result.to = to;
    result.promote = m_gameController->promote();
    result.valid = to.x() >= 1 && to.x() <= 9 && to.y() >= 1 && to.y() <= 9;
    result.scoreCp = m_engine->lastScoreCp();
    emit thinkingFinished(result);
}

void CsaEngineController::sendGameOver(bool win)
{
    m_ponderPosition.clear();
    if (!m_engine) return;
    if (win) {
        m_engine->sendGameOverWinAndQuitCommands();
    } else {
        m_engine->sendGameOverLoseAndQuitCommands();
    }
}

void CsaEngineController::sendQuit()
{
    m_ponderPosition.clear();
    if (m_engine) {
        m_engine->sendQuitCommand();
    }
}

void CsaEngineController::cleanup()
{
    m_ponderPosition.clear();
    if (Usi* engine = std::exchange(m_engine, nullptr)) {
        disconnect(engine, nullptr, this, nullptr);
        // QObjectの子破棄中にプロセス待ちのイベントループへ入らないよう、先に停止する。
        engine->cleanupEngineProcessAndThread(false);
        engine->deleteLater();
    }
}

void CsaEngineController::onEngineResign()
{
    emit resignRequested();
}
