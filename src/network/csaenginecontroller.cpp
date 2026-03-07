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

    connect(m_engine, &Usi::bestMoveReceived,
            this, &CsaEngineController::onBestMoveReceived);
    connect(m_engine, &Usi::bestMoveResignReceived,
            this, &CsaEngineController::onEngineResign);

    m_engine->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("CSA"), params.engineName);
    if (!m_engine->startAndInitializeEngine(enginePath, params.engineName)) {
        emit logMessage(tr("エンジン %1 の初期化に失敗しました").arg(params.engineName), true);
        m_engine->deleteLater();
        m_engine = nullptr;
        return;
    }

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

    const UsiTimingParams timing{params.byoyomiMs, params.btimeStr, params.wtimeStr,
                                 params.bincMs, params.wincMs, params.useByoyomi};
    m_engine->handleEngineVsHumanOrEngineMatchCommunication(
        positionCmd, ponderStr, result.from, result.to, timing);

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
}

void CsaEngineController::onBestMoveReceived()
{
    qCDebug(lcNetwork) << "onEngineBestMoveReceived called (handled in think)";
}

void CsaEngineController::onEngineResign()
{
    emit resignRequested();
}
