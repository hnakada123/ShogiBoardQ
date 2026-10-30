#ifndef AUTOMATIONCOMMANDS_H
#define AUTOMATIONCOMMANDS_H

/// @file automationcommands.h
/// @brief 自動化 API の JSON-RPC メソッド群の登録

#include "automationcontext.h"
#include "recordline.h"

class AutomationDispatcher;
class KifuFileController;

/// メソッド群の登録関数。メソッド `x.y` の実装は automationcommands_x.cpp に置く（名前空間ごとに1ファイル）
namespace AutomationCommands {

void registerAll(AutomationDispatcher& dispatcher, const AutomationContext& context);

void registerAppCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);        ///< app.*
void registerPositionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);   ///< position.*
void registerKifuCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);       ///< kifu.*
void registerBoardCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);      ///< board.*
void registerBranchCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);     ///< branch.*
void registerMenuCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);       ///< menu.*
void registerActionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);     ///< action.*
void registerDialogCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);     ///< dialog.*
void registerWidgetCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);     ///< widget.*
void registerDockCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);       ///< dock.*
void registerScreenshotCommands(AutomationDispatcher& dispatcher, const AutomationContext& context); ///< screenshot.*
void registerClipboardCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);  ///< clipboard.*
void registerUiCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);         ///< ui.*

/// 現在表示中の局面の SFEN（SFEN 記録があればその値、無ければ盤から組み立てる）
QString currentSfen(const AutomationContext& context);
/// 閲覧中の分岐の局面列（終局行を含む）。
QStringList currentLinePositions(const AutomationContext& context);
/// 表示中の手順（分岐ツリー）。終局行は含まない。currentPly は表示中の手数
RecordLine currentRecordLine(const AutomationContext& context);
/// 棋譜を置き換える操作（kifu.load・position.set）の前の検査。対局・検討・解析・局面編集の間と、
/// discardUnsaved でない未保存の変更があるときは AutomationError を投げる
KifuFileController* requireReplaceableRecord(const AutomationContext& context, bool discardUnsaved);
/// メインウィンドウのドック name を見せて前面に出す操作を予約する（dock.configure の operation=show と同じ処理と検査。
/// widget.showDock が使う）
QJsonObject showDock(const AutomationContext& context, const QString& name);

} // namespace AutomationCommands

#endif // AUTOMATIONCOMMANDS_H
