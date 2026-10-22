/// @file csaenginecontroller.cpp
/// @brief CSA通信対局用エンジンコントローラの実装

#include "csaenginecontroller.h"
#include "enginelifecyclemanager.h"
#include "enginelistsettings.h"
#include "usi.h"
#include "usitimingparams.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <utility>

CsaEngineController::CsaEngineController(EngineLifecycleManager* engines, QObject* parent)
    : QObject(parent)
    , m_engines(engines)
{
    connect(m_engines, &EngineLifecycleManager::engineRevoked, this, &CsaEngineController::onEngineRevoked);
}

CsaEngineController::~CsaEngineController()
{
    cleanup();
}

Usi* CsaEngineController::engine() const
{
    return m_engine;
}

void CsaEngineController::initialize(const InitParams& params)
{
    cleanup();

    m_engineName = params.engineName;

    // ダイアログで選んだエンジンのパスを優先し、無ければ名前で登録一覧から引く
    // （番号は登録一覧が変わるとずれるため使わない）
    QString enginePath = params.enginePath;
    if (enginePath.isEmpty()) {
        enginePath = EngineListSettings::pathForName(params.engineName);
    }

    if (enginePath.isEmpty()) {
        onEngineError(tr("エンジンパスが指定されていません"));
        return;
    }

    m_lease = m_engines->acquire(this, 1, std::nullopt, params.engineName);
    m_engine = m_lease.engine();
    m_engine->setLogIdentity(QStringLiteral("E1"), QStringLiteral("CSA"), params.engineName);
    m_engine->setMatchTimeSource(params.timeSource);

    connect(m_engine, &Usi::matchMoveReady, this, &CsaEngineController::onMatchMoveReady);
    connect(m_engine, &Usi::engineInitialized, this, &CsaEngineController::onEngineInitialized);
    connect(m_engine, &Usi::errorOccurred, this, &CsaEngineController::onEngineError);
    connect(m_engine, &Usi::resigned, this, &CsaEngineController::onEngineResign);
    connect(m_engine, &Usi::declaredWin, this, &CsaEngineController::onEngineWinDeclaration);
    m_engine->startEngine(enginePath, params.engineName);
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

void CsaEngineController::thinkAsync(const UsiPosition& position)
{
    if (!m_engine) return;
    m_engine->requestMatchMove(position);
}

void CsaEngineController::onMatchMoveReady(const UsiMatchMove& move)
{
    if (!m_engine) return;
    ThinkingResult result;
    result.move = move.move;
    result.valid = move.move.isValid();
    result.score = move.score;
    emit thinkingFinished(result);
}

void CsaEngineController::stopThinking()
{
    if (m_engine) m_engine->stop();
}

bool CsaEngineController::isStarting() const
{
    return m_engine && m_engine->isInitializing();
}

bool CsaEngineController::isReady() const
{
    return m_engine && m_engine->isRunning() && !m_engine->isInitializing();
}

void CsaEngineController::sendGameOver(GameOverResult result)
{
    if (!m_engine) return;
    // 終局後の bestmove で着手させない（取り消した探索の結果は届かない）。先読み中なら stop で止める
    m_engine->cancelCurrentOperation();
    m_engine->stop();
    m_engine->sendGameOverCommand(result);
    // 枠を空けて quit する。次の対局は起動し直す
    cleanup();
}

void CsaEngineController::sendQuit()
{
    cleanup();
}

void CsaEngineController::cleanup()
{
    if (Usi* engine = std::exchange(m_engine, nullptr)) disconnect(engine, nullptr, this, nullptr);
    // 枠を空ける（quit を送り、後で破棄する。思考情報・通信ログの表示は残す）
    m_lease.reset();
}

void CsaEngineController::onEngineRevoked(QObject* owner)
{
    if (owner != this || !m_engine) return;
    // 通信対局の最中にほかの処理が枠を取った。エンジンの異常と同じく知らせる
    onEngineError(tr("エンジンを別の処理が使い始めたため、通信対局のエンジンを止めました"));
}

void CsaEngineController::onEngineResign()
{
    emit resignRequested();
}

void CsaEngineController::onEngineWinDeclaration()
{
    emit winDeclarationRequested();
}
