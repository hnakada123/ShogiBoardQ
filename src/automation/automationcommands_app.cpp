/// @file automationcommands_app.cpp
/// @brief 自動化 API の app.* メソッドと共通ヘルパの実装

#include "automationcommands.h"
#include "analysissession.h"
#include "automationdispatcher.h"
#include "automationwidgets.h"
#include "gamerecordmodel.h"
#include "kifufilecontroller.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "kifunavigationstate.h"
#include "kifuexportmetadata.h"
#include <QMetaEnum>

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
    // ボタン操作でモーダル画面を閉じると、呼び出し元のボタンや親が破棄されることがある。
    // 実行前の親破棄では予約を取り消し、実行開始後はコールバックの終了まで自身を保つ。
    setParent(nullptr);
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

/// 自動化 API の play_mode（動作状態と、対局中なら対局者の組。駒落ちの対局は handicap_ を付ける）
QString playModeName(const AppState& state)
{
    using Activity = AppActivityState::Activity;
    switch (state.activity.activity()) {
    case Activity::Idle:
    case Activity::DuringPositionEdit: return QStringLiteral("not_started");
    case Activity::DuringAnalysis: return QStringLiteral("analysis");
    case Activity::DuringConsideration: return QStringLiteral("consideration");
    case Activity::DuringTsumeSearch: return QStringLiteral("tsume_search");
    case Activity::DuringCsaGame: return QStringLiteral("csa_network");
    case Activity::DuringGame: break;
    }
    const MatchPlayers& players = state.players;
    if (players.humanVsHuman()) return QStringLiteral("human_vs_human");
    const QString seats = players.engineVsEngine() ? QStringLiteral("engine_vs_engine")
                        : players.blackIsEngine  ? QStringLiteral("engine_vs_human")
                                                 : QStringLiteral("human_vs_engine");
    const bool handicap = KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(state.startSfenStr));
    return handicap ? QStringLiteral("handicap_") + seats : seats;
}

} // namespace

QStringList AutomationCommands::currentLinePositions(const AutomationContext& context)
{
    auto* model = context.gameRecordModel;
    if (model && model->branchTree() && !model->branchTree()->isEmpty()) {
        const auto lines = model->branchTree()->allLines();
        const int line = model->activeRow();
        if (line >= 0 && line < lines.size()) {
            QStringList positions;
            for (const auto* node : lines[line].nodes) positions.append(node->sfen());
            return positions;
        }
    }
    return {};
}

RecordLine AutomationCommands::currentRecordLine(const AutomationContext& context)
{
    auto* model = context.gameRecordModel;
    if (!model || !model->branchTree() || model->branchTree()->isEmpty()) return {};
    const auto lines = model->branchTree()->allLines();
    const int line = model->activeRow();
    if (line < 0 || line >= lines.size()) return {};
    const int ply = context.currentPly();
    return KifuBranchTree::toRecordLine(lines.at(line).nodes, ply);
}

int AutomationContext::currentPly() const
{
    return navState ? navState->currentPly() : 0;
}

QString AutomationCommands::currentSfen(const AutomationContext& context)
{
    const QStringList record = currentLinePositions(context);
    const int index = context.currentPly();
    if (index >= 0 && index < record.size()) {
        return record.at(index);
    }
    if (context.gameController && context.gameController->board()) {
        ShogiBoard* board = context.gameController->board();
        return board->convertBoardToSfen() + QLatin1Char(' ') + turnToSfen(board->currentPlayer())
               + QLatin1Char(' ') + board->convertStandToSfen() + QStringLiteral(" 1");
    }
    return context.state ? context.state->currentSfenStr : QString();
}

void AutomationCommands::registerAll(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    registerAppCommands(dispatcher, context);
    registerPositionCommands(dispatcher, context);
    registerKifuCommands(dispatcher, context);
    registerUiCommands(dispatcher, context);
    registerWidgetCommands(dispatcher, context);
    registerDockCommands(dispatcher, context);
    registerNavigationCommands(dispatcher, context);
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
        UiStatePolicyManager* policy = context.uiStatePolicy;
        result[QStringLiteral("ui_state")] = policy ? uiStateName(policy->currentState()) : QStringLiteral("unknown");
        result[QStringLiteral("play_mode")] = context.state ? playModeName(*context.state) : QStringLiteral("unknown");
        const int ply = context.currentPly();
        result[QStringLiteral("current_ply")] = ply;
        const QStringList record = currentLinePositions(context);
        result[QStringLiteral("total_plies")] = qMax(0, static_cast<int>(record.size()) - 1);
        result[QStringLiteral("kifu_file")] = context.state ? context.state->saveFileName : QString();
        auto* fileController = context.kifuFileController;
        result[QStringLiteral("dirty")] = fileController && fileController->hasUnsavedChanges();
        result[QStringLiteral("board_flipped")] = context.shogiView && context.shogiView->flipMode();
        result[QStringLiteral("sfen")] = currentSfen(context);
        QJsonObject engines;
        // 検討・詰み探索中はそのエンジン、それ以外は直近の対局のエンジン
        const QString analysisEngine = context.analysis ? context.analysis->engineName() : QString();
        if (!analysisEngine.isEmpty()) {
            engines[QStringLiteral("black")] = analysisEngine;
            engines[QStringLiteral("white")] = QString();
        } else {
            engines[QStringLiteral("black")] = context.state ? context.state->players.blackEngineName : QString();
            engines[QStringLiteral("white")] = context.state ? context.state->players.whiteEngineName : QString();
        }
        result[QStringLiteral("engines")] = engines;
        if (auto* csa = context.csaGameWiring ? context.csaGameWiring->coordinator() : nullptr) {
            result[QStringLiteral("csa")] = QJsonObject{
                {QStringLiteral("state"), QString::fromLatin1(QMetaEnum::fromType<CsaGameCoordinator::GameState>()
                    .valueToKey(static_cast<quint64>(csa->gameState())))},
                {QStringLiteral("is_my_turn"), csa->isMyTurn()},
                {QStringLiteral("is_black"), csa->isBlackSide()},
                {QStringLiteral("is_human"), csa->isHumanPlayer()},
                {QStringLiteral("black_remaining_ms"), csa->blackRemainingMs()},
                {QStringLiteral("white_remaining_ms"), csa->whiteRemainingMs()},
                {QStringLiteral("black_consumed_ms"), csa->blackTotalTimeMs()},
                {QStringLiteral("white_consumed_ms"), csa->whiteTotalTimeMs()}};
        }
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
