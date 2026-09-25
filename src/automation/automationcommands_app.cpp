/// @file automationcommands_app.cpp
/// @brief 自動化 API の app.* メソッドと共通ヘルパの実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationwidgets.h"
#include "gamerecordmodel.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QMainWindow>
#include <QTimer>

AutomationDeferredCall::AutomationDeferredCall(std::function<void()> callback, QObject* parent)
    : QObject(parent)
    , m_callback(std::move(callback))
{
}

void AutomationDeferredCall::schedule(std::function<void()> callback, QObject* parent)
{
    auto* call = new AutomationDeferredCall(std::move(callback), parent);
    QTimer::singleShot(0, call, &AutomationDeferredCall::run);
}

void AutomationDeferredCall::run()
{
    if (m_callback) m_callback();
    deleteLater();
}

namespace {

QString uiStateName(UiStatePolicyManager::AppState state)
{
    switch (state) {
    case UiStatePolicyManager::AppState::Idle: return QStringLiteral("idle");
    case UiStatePolicyManager::AppState::DuringGame: return QStringLiteral("game");
    case UiStatePolicyManager::AppState::DuringAnalysis: return QStringLiteral("analysis");
    case UiStatePolicyManager::AppState::DuringCsaGame: return QStringLiteral("csa_game");
    case UiStatePolicyManager::AppState::DuringTsumeSearch: return QStringLiteral("tsume_search");
    case UiStatePolicyManager::AppState::DuringConsideration: return QStringLiteral("consideration");
    case UiStatePolicyManager::AppState::DuringPositionEdit: return QStringLiteral("position_edit");
    }
    return QStringLiteral("unknown");
}

QString playModeName(PlayMode mode)
{
    switch (mode) {
    case PlayMode::NotStarted: return QStringLiteral("not_started");
    case PlayMode::HumanVsHuman: return QStringLiteral("human_vs_human");
    case PlayMode::EvenHumanVsEngine: return QStringLiteral("human_vs_engine");
    case PlayMode::EvenEngineVsHuman: return QStringLiteral("engine_vs_human");
    case PlayMode::EvenEngineVsEngine: return QStringLiteral("engine_vs_engine");
    case PlayMode::HandicapEngineVsHuman: return QStringLiteral("handicap_engine_vs_human");
    case PlayMode::HandicapHumanVsEngine: return QStringLiteral("handicap_human_vs_engine");
    case PlayMode::HandicapEngineVsEngine: return QStringLiteral("handicap_engine_vs_engine");
    case PlayMode::AnalysisMode: return QStringLiteral("analysis");
    case PlayMode::ConsiderationMode: return QStringLiteral("consideration");
    case PlayMode::TsumiSearchMode: return QStringLiteral("tsume_search");
    case PlayMode::CsaNetworkMode: return QStringLiteral("csa_network");
    case PlayMode::PlayModeError: return QStringLiteral("error");
    }
    return QStringLiteral("unknown");
}

} // namespace

QString AutomationCommands::currentSfen(const AutomationContext& context)
{
    const QStringList* record = context.sfenRecord ? context.sfenRecord() : nullptr;
    const int index = context.currentMoveIndex ? *context.currentMoveIndex : 0;
    if (record && index >= 0 && index < record->size()) {
        return record->at(index);
    }
    if (context.gameController && context.gameController->board()) {
        ShogiBoard* board = context.gameController->board();
        return board->convertBoardToSfen() + QLatin1Char(' ') + turnToSfen(board->currentPlayer())
               + QLatin1Char(' ') + board->convertStandToSfen() + QStringLiteral(" 1");
    }
    return context.currentSfenStr ? *context.currentSfenStr : QString();
}

void AutomationCommands::registerAll(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    registerAppCommands(dispatcher, context);
    registerPositionCommands(dispatcher, context);
    registerKifuCommands(dispatcher, context);
    registerUiCommands(dispatcher, context);
}

void AutomationCommands::registerAppCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("app.ping"), [](const QJsonObject&) {
        QJsonObject result;
        result[QStringLiteral("pong")] = true;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("app.version"), [&dispatcher](const QJsonObject&) {
        QJsonObject result;
        result[QStringLiteral("version")] = QCoreApplication::applicationVersion();
        result[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
        result[QStringLiteral("api")] = 1;
        result[QStringLiteral("methods")] = QJsonArray::fromStringList(dispatcher.methodNames());
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("app.state"), [context](const QJsonObject&) {
        QJsonObject result;
        UiStatePolicyManager* policy = context.uiStatePolicy ? context.uiStatePolicy() : nullptr;
        result[QStringLiteral("ui_state")] = policy ? uiStateName(policy->currentState()) : QStringLiteral("unknown");
        result[QStringLiteral("play_mode")] = context.playMode ? playModeName(*context.playMode) : QStringLiteral("unknown");
        const int ply = context.currentMoveIndex ? *context.currentMoveIndex : 0;
        result[QStringLiteral("current_ply")] = ply;
        const QStringList* record = context.sfenRecord ? context.sfenRecord() : nullptr;
        result[QStringLiteral("total_plies")] = record && !record->isEmpty() ? static_cast<int>(record->size()) - 1 : 0;
        result[QStringLiteral("kifu_file")] = context.saveFileName ? *context.saveFileName : QString();
        GameRecordModel* model = context.gameRecordModel ? context.gameRecordModel() : nullptr;
        result[QStringLiteral("dirty")] = model && model->isDirty();
        result[QStringLiteral("board_flipped")] = context.shogiView && context.shogiView->flipMode();
        result[QStringLiteral("sfen")] = currentSfen(context);
        QJsonObject engines;
        engines[QStringLiteral("black")] = context.engineName1 ? *context.engineName1 : QString();
        engines[QStringLiteral("white")] = context.engineName2 ? *context.engineName2 : QString();
        result[QStringLiteral("engines")] = engines;
        QJsonArray dialogs;
        const QList<QWidget*> windows = AutomationWidgets::visibleWindows();
        for (QWidget* w : windows) {
            if (w == context.mainWindow) continue;
            dialogs.append(AutomationWidgets::describeWindow(w, false));
        }
        result[QStringLiteral("dialogs")] = dialogs;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("app.quit"), [context](const QJsonObject&) {
        if (!context.quitApplication) {
            throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Quit is not available"));
        }
        // 応答を送ってから終了処理を始める
        AutomationDeferredCall::schedule(context.quitApplication, context.mainWindow);
        QJsonObject result;
        result[QStringLiteral("ok")] = true;
        return result;
    });
}
