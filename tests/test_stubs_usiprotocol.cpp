/// @file test_stubs_usiprotocol.cpp
/// @brief UsiProtocolHandler テスト用スタブ
///
/// usiprotocolhandler.cpp が参照する外部シンボルのスタブ実装を提供する。
/// テストではエンジンプロセスを起動せず、プロトコル解析ロジックのみを検証する。

#include <QProcess>
#include <QSettings>

#include "engineprocessmanager.h"
#include "shogiboard.h"
#include "thinkinginfopresenter.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "settingscommon.h"

// === EngineProcessManager スタブ ===
EngineProcessManager::EngineProcessManager(QObject* parent) : QObject(parent) {}
EngineProcessManager::~EngineProcessManager() = default;
bool EngineProcessManager::startProcess(const QString&) { return false; }
void EngineProcessManager::stopProcess() {}
bool EngineProcessManager::isRunning() const { return false; }
QProcess::ProcessState EngineProcessManager::state() const { return QProcess::NotRunning; }
void EngineProcessManager::sendCommand(const QString&) {}
void EngineProcessManager::closeWriteChannel() {}
void EngineProcessManager::setShutdownState(ShutdownState) {}
void EngineProcessManager::setPostQuitInfoStringLinesLeft(int) {}
void EngineProcessManager::decrementPostQuitLines() {}
void EngineProcessManager::discardStdout() {}
void EngineProcessManager::discardStderr() {}
bool EngineProcessManager::canReadLine() const { return false; }
QByteArray EngineProcessManager::readLine() { return {}; }
QByteArray EngineProcessManager::readAllStderr() { return {}; }
void EngineProcessManager::setLogIdentity(const QString&, const QString&, const QString&) {}
QString EngineProcessManager::logPrefix() const { return {}; }
void EngineProcessManager::onReadyReadStdout() {}
void EngineProcessManager::onReadyReadStderr() {}
void EngineProcessManager::onProcessError(QProcess::ProcessError) {}
void EngineProcessManager::onProcessFinished(int, QProcess::ExitStatus) {}
void EngineProcessManager::scheduleMoreReading() {}

// === ThinkingInfoPresenter スタブ ===
ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent) : QObject(parent) {}
ThinkingInfoPresenter::~ThinkingInfoPresenter() = default;
void ThinkingInfoPresenter::setGameController(ShogiGameController*) {}
void ThinkingInfoPresenter::setAnalysisMode(bool) {}
void ThinkingInfoPresenter::setPreviousMove(int, int) {}
void ThinkingInfoPresenter::setClonedBoardData(const QList<QChar>&) {}
void ThinkingInfoPresenter::setPonderEnabled(bool) {}
void ThinkingInfoPresenter::setBaseSfen(const QString&) {}
QString ThinkingInfoPresenter::baseSfen() const { return {}; }
void ThinkingInfoPresenter::processInfoLine(const QString&) {}
void ThinkingInfoPresenter::requestClearThinkingInfo() {}
void ThinkingInfoPresenter::flushInfoBuffer() {}
void ThinkingInfoPresenter::logSentCommand(const QString&, const QString&) {}
void ThinkingInfoPresenter::logReceivedData(const QString&, const QString&) {}
void ThinkingInfoPresenter::logStderrData(const QString&, const QString&) {}
void ThinkingInfoPresenter::onInfoReceived(const QString&) {}

// === ShogiEngineInfoParser スタブ ===
ShogiEngineInfoParser::ShogiEngineInfoParser() {}

// === ShogiGameController スタブ ===
ShogiGameController::ShogiGameController(QObject* parent) : QObject(parent) {}
ShogiGameController::~ShogiGameController() = default;
void ShogiGameController::setPromote(bool) {}
void ShogiGameController::newGame(QString&) {}

// === SettingsCommon スタブ ===
namespace SettingsCommon {
QString settingsFilePath() { return QStringLiteral("/tmp/tst_usiprotocol_stub.ini"); }
QSettings& openSettings() { static QSettings s(settingsFilePath(), QSettings::IniFormat); return s; }
}

// === MOC出力のインクルード ===
// AUTOMOC が依存ヘッダーの MOC を自動生成するよう、明示的にインクルードする
#include "moc_engineprocessmanager.cpp"
#include "moc_thinkinginfopresenter.cpp"
#include "moc_shogiengineinfoparser.cpp"
#include "moc_shogigamecontroller.cpp"
