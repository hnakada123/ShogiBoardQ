/// @file automationcommands.cpp
/// @brief 自動化 API のメソッド群の登録と、メソッドが共有する表示中の局面・手順の読み取り

#include "automationcommands.h"

#include "appactivitystate.h"
#include "automationdispatcher.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "shogiboard.h"

QStringList AutomationCommands::currentLinePositions(const AutomationContext& context)
{
    QStringList positions;
    if (!context.navState) return positions;
    const QList<KifuBranchNode*> path = context.navState->selectedPath();
    for (const KifuBranchNode* node : path) positions.append(node->sfen());
    return positions;
}

RecordLine AutomationCommands::currentRecordLine(const AutomationContext& context)
{
    return context.navState ? context.navState->currentRecordLine() : RecordLine();
}

int AutomationContext::currentPly() const
{
    return navState ? navState->currentPly() : 0;
}

QString AutomationCommands::currentSfen(const AutomationContext& context)
{
    // 局面編集中は編集している盤の局面（表示中の手のノードは編集の前の局面のまま）
    if (context.board && context.activity && context.activity->is(AppActivityState::Activity::DuringPositionEdit))
        return context.board->toSfen();
    const QString displayed = context.navState ? context.navState->currentSfen() : QString();
    if (!displayed.isEmpty()) return displayed;
    return context.board ? context.board->toSfen() : QString();
}

void AutomationCommands::registerAll(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    registerAppCommands(dispatcher, context);
    registerPositionCommands(dispatcher, context);
    registerKifuCommands(dispatcher, context);
    registerBoardCommands(dispatcher, context);
    registerBranchCommands(dispatcher, context);
    registerMenuCommands(dispatcher, context);
    registerActionCommands(dispatcher, context);
    registerDialogCommands(dispatcher, context);
    registerWidgetCommands(dispatcher, context);
    registerDockCommands(dispatcher, context);
    registerScreenshotCommands(dispatcher, context);
    registerClipboardCommands(dispatcher, context);
    registerUiCommands(dispatcher, context);
}
