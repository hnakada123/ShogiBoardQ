/// @file automationcommands_app.cpp
/// @brief 自動化 API の app.* メソッドの実装

#include "automationcommands.h"
#include "analysissession.h"
#include "automationdispatcher.h"
#include "automationinteractions.h"
#include "automationwidgets.h"
#include "csagamecoordinator.h"
#include "kifudocument.h"
#include "gameinfo.h"
#include "kifufilecontroller.h"
#include "sfenutils.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QMainWindow>
#include <QMetaEnum>

namespace {

QString uiStateName(AppActivityState::Activity state)
{
    switch (state) {
    case AppActivityState::Activity::Idle: return QStringLiteral("idle");
    case AppActivityState::Activity::DuringGame: return QStringLiteral("game");
    case AppActivityState::Activity::DuringAnalysis: return QStringLiteral("analysis");
    case AppActivityState::Activity::DuringCsaGame: return QStringLiteral("csa_game");
    case AppActivityState::Activity::DuringTsumeSearch: return QStringLiteral("tsume_search");
    case AppActivityState::Activity::DuringConsideration: return QStringLiteral("consideration");
    case AppActivityState::Activity::DuringPositionEdit: return QStringLiteral("position_edit");
    }
    return QStringLiteral("unknown");
}

/// 自動化 API の play_mode（動作状態と、対局中なら対局者の組。駒落ちの対局は handicap_ を付ける）
QString playModeName(const AppActivityState& state, const MatchPlayers& players, const QString& startSfen)
{
    using Activity = AppActivityState::Activity;
    switch (state.activity()) {
    case Activity::Idle:
    case Activity::DuringPositionEdit: return QStringLiteral("not_started");
    case Activity::DuringAnalysis: return QStringLiteral("analysis");
    case Activity::DuringConsideration: return QStringLiteral("consideration");
    case Activity::DuringTsumeSearch: return QStringLiteral("tsume_search");
    case Activity::DuringCsaGame: return QStringLiteral("csa_network");
    case Activity::DuringGame: break;
    }
    if (players.humanVsHuman()) return QStringLiteral("human_vs_human");
    const QString seats = players.engineVsEngine() ? QStringLiteral("engine_vs_engine")
                        : players.isEngine(Turn::Black)  ? QStringLiteral("engine_vs_human")
                                                 : QStringLiteral("human_vs_engine");
    return GameInfo::isHandicap(startSfen) ? QStringLiteral("handicap_") + seats : seats;
}

} // namespace

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
        const int ply = context.currentPly();
        const QStringList record = currentLinePositions(context);
        const QString startSfen = record.isEmpty() ? SfenUtils::hirateSfen() : record.first();
        const MatchPlayers players = context.document ? context.document->players() : MatchPlayers();
        result[QStringLiteral("play_mode")] = context.activity ? playModeName(*context.activity, players, startSfen)
                                                            : QStringLiteral("unknown");
        result[QStringLiteral("current_ply")] = ply;
        result[QStringLiteral("total_plies")] = qMax(0, static_cast<int>(record.size()) - 1);
        result[QStringLiteral("kifu_file")] = context.document ? context.document->filePath() : QString();
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
            engines[QStringLiteral("black")] = players.engineName(Turn::Black);
            engines[QStringLiteral("white")] = players.engineName(Turn::White);
        }
        result[QStringLiteral("engines")] = engines;
        // 通信対局を一度も始めていなければ（司令塔が待機のまま）出さない
        const CsaGameCoordinator* csa = context.csaGame;
        if (csa && csa->gameState() != CsaGameCoordinator::GameState::Idle) {
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
            throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Quit is not available"));
        }
        // 応答を送ってから終了処理を始める
        AutomationDeferredCall::schedule(context.quitApplication, context.mainWindow);
        QJsonObject result;
        result[QStringLiteral("ok")] = true;
        return result;
    });
}
