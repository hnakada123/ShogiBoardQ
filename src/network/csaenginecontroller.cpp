/// @file csaenginecontroller.cpp
/// @brief CSA通信対局用エンジンコントローラの実装

#include "csaenginecontroller.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "settingscommon.h"
#include "playmode.h"
#include "logcategories.h"

#include <QSettings>

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
    if (m_engine) {
        m_engine->sendQuitCommand();
        m_engine->deleteLater();
        m_engine = nullptr;
    }

    m_gameController = params.gameController;

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginReadArray("Engines");
    settings.setArrayIndex(params.engineNumber);
    QString enginePath = settings.value("path").toString();
    settings.endArray();

    if (enginePath.isEmpty()) {
        enginePath = params.enginePath;
    }

    if (enginePath.isEmpty()) {
        emit logMessage(tr("エンジンパスが指定されていません"), true);
        return;
    }

    // USI通信ログモデル：外部から渡されていなければ内部で作成
    if (!m_engineCommLog) {
        if (params.commLog) {
            m_engineCommLog = params.commLog;
            m_ownsCommLog = false;
        } else {
            m_engineCommLog = new UsiCommLogModel(this);
            m_ownsCommLog = true;
        }
    } else {
        m_engineCommLog->clear();
    }

    // エンジン思考モデル：外部から渡されていなければ内部で作成
    if (!m_engineThinking) {
        if (params.thinkingModel) {
            m_engineThinking = params.thinkingModel;
            m_ownsThinking = false;
        } else {
            m_engineThinking = new ShogiEngineThinkingModel(this);
            m_ownsThinking = true;
        }
    } else {
        m_engineThinking->clearAllItems();
    }

    static PlayMode dummyPlayMode = PlayMode::CsaNetworkMode;

    m_engine = new Usi(m_engineCommLog, m_engineThinking,
                       m_gameController.data(), dummyPlayMode, this);

    connect(m_engine, &Usi::bestMoveReceived,
            this, &CsaEngineController::onBestMoveReceived);
    connect(m_engine, &Usi::bestMoveResignReceived,
            this, &CsaEngineController::onEngineResign);

    m_engine->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("CSA"), params.engineName);
    m_engine->startAndInitializeEngine(enginePath, params.engineName);

    emit logMessage(tr("エンジン %1 を起動しました").arg(params.engineName));
}

CsaEngineController::ThinkingResult CsaEngineController::think(const ThinkingParams& params)
{
    ThinkingResult result;
    if (!m_engine || !m_gameController) {
        return result;
    }

    m_gameController->setPromote(false);

    QString positionCmd = params.positionCmd;
    QString ponderStr;

    m_engine->handleEngineVsHumanOrEngineMatchCommunication(
        positionCmd,
        ponderStr,
        result.from, result.to,
        params.byoyomiMs,
        params.btimeStr,
        params.wtimeStr,
        params.bincMs,
        params.wincMs,
        params.useByoyomi
    );

    result.resign = m_engine->isResignMove();
    result.promote = m_gameController->promote();
    result.valid = (result.to.x() >= 1 && result.to.x() <= 9 &&
                    result.to.y() >= 1 && result.to.y() <= 9);
    result.scoreCp = m_engine->lastScoreCp();

    return result;
}

void CsaEngineController::sendGameOver(bool win)
{
    if (!m_engine) return;
    if (win) {
        m_engine->sendGameOverWinAndQuitCommands();
    } else {
        m_engine->sendGameOverLoseAndQuitCommands();
    }
}

void CsaEngineController::sendQuit()
{
    if (m_engine) {
        m_engine->sendQuitCommand();
    }
}

void CsaEngineController::cleanup()
{
    if (m_engine) {
        m_engine->sendQuitCommand();
        m_engine->deleteLater();
        m_engine = nullptr;
    }

    if (m_engineCommLog && m_ownsCommLog) {
        m_engineCommLog->deleteLater();
        m_engineCommLog = nullptr;
    }

    if (m_engineThinking && m_ownsThinking) {
        m_engineThinking->deleteLater();
        m_engineThinking = nullptr;
    }
}

void CsaEngineController::onBestMoveReceived()
{
    qCDebug(lcNetwork) << "onEngineBestMoveReceived called (handled in think)";
}

void CsaEngineController::onEngineResign()
{
    emit resignRequested();
}
