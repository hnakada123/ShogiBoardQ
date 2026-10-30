/// @file automationmethodspecs.cpp
/// @brief 自動化 API のメソッドごとの引数の表

#include "automationmethodspecs.h"
#include "kifuformat.h"

using namespace ParamSpecs;

namespace {
// 対象のウィンドウと部品（多くのメソッドで共通）
ParamSpec target()
{
    return string(QStringLiteral("target"), QStringLiteral("'main', or a dialog object name, selector or title substring."))
        .withDefault(QStringLiteral("main"));
}
ParamSpec dialogTarget()
{
    return string(QStringLiteral("target"), QStringLiteral("Dialog object name, selector or title substring."))
        .length(1, 1000).require();
}
ParamSpec widget()
{
    return string(QStringLiteral("widget"), QStringLiteral("Widget object name or selector from widget.text.")).length(1, 1000);
}
ParamSpec discardUnsaved()
{
    return boolean(QStringLiteral("discard_unsaved"), QStringLiteral("Discard unsaved edits of the current record."));
}
ParamSpec coordinate(const QString& name)
{
    return integer(name, QString(), -100000, 100000);
}
} // namespace

const QList<OperationSpec>& AutomationMethodSpecs::all()
{
    static const QList<OperationSpec> table = {
        {QStringLiteral("app.ping"), QStringLiteral("Check that the application responds."), {}},
        {QStringLiteral("app.version"), QStringLiteral("Application version and method names."), {}},
        {QStringLiteral("app.state"), QStringLiteral("UI state, play mode, plies, file, dialogs, engines and CSA state."), {}},
        {QStringLiteral("app.quit"), QStringLiteral("Quit without the unsaved-changes prompt."), {}},
        {QStringLiteral("position.get"), QStringLiteral("Current position, start position and moves."), {}},
        {QStringLiteral("position.set"), QStringLiteral("Replace the position (the record is cleared)."),
         {string(QStringLiteral("sfen"),
                 QStringLiteral("Position in SFEN (board, side to move b/w, hands, move number), or 'startpos' for the "
                                "initial position. A leading 'position sfen ' is accepted."))
              .atLeast(1).require(),
          discardUnsaved()}},
        {QStringLiteral("kifu.load"), QStringLiteral("Load a record from a file or text."),
         {string(QStringLiteral("path"), QStringLiteral("Kifu file (absolute path).")),
          string(QStringLiteral("text"), QStringLiteral("Kifu text.")).length(1, 2000000), discardUnsaved()}},
        {QStringLiteral("kifu.save"), QStringLiteral("Save the record (format by extension)."),
         {string(QStringLiteral("path"), QStringLiteral("Destination file (absolute path).")).atLeast(1).require(),
          boolean(QStringLiteral("overwrite"), QStringLiteral("Allow replacing an existing file."))}},
        {QStringLiteral("kifu.get"), QStringLiteral("The record as moves of a line, or as exported text."),
         {choice(QStringLiteral("format"), QString(), QStringList{QStringLiteral("moves")} + KifuFormats::savableNames())
              .withDefault(QStringLiteral("moves")),
          choice(QStringLiteral("line"), QStringLiteral("Line for format=moves."), {QStringLiteral("current"), QStringLiteral("main")})
              .withDefault(QStringLiteral("current")),
          integer(QStringLiteral("from_ply"), QString(), 1, 100000).withDefault(1),
          integer(QStringLiteral("max_moves"), QString(), 1, 100000).withDefault(200)}},
        {QStringLiteral("kifu.goto"), QStringLiteral("Go to a ply of the current line (0 = start position)."),
         {integer(QStringLiteral("ply"), QString(), 0, 100000).require()}},
        {QStringLiteral("board.click"), QStringLiteral("Click a board square (file 10/11 are the hands)."),
         {target(), integer(QStringLiteral("file"), QString(), 1, 11).require(),
          integer(QStringLiteral("rank"), QString(), 1, 9).require(),
          choice(QStringLiteral("button"), QString(), {QStringLiteral("left"), QStringLiteral("right")})
              .withDefault(QStringLiteral("left"))}},
        {QStringLiteral("dialog.list"), QStringLiteral("Open top-level windows and dialogs."), {}},
        {QStringLiteral("dialog.close"), QStringLiteral("Close (reject) an open dialog."), {dialogTarget()}},
        {QStringLiteral("dialog.clickButton"), QStringLiteral("Click a button of an open dialog by widget or text."),
         {dialogTarget(), widget(), string(QStringLiteral("text"), QStringLiteral("Exact button text.")).length(1, 1000),
          integer(QStringLiteral("index"), QStringLiteral("Zero-based occurrence among matches."), 0, 1000).withDefault(0)}},
        {QStringLiteral("action.list"), QStringLiteral("Menu actions that action.trigger may run."), {}},
        {QStringLiteral("action.trigger"), QStringLiteral("Trigger an allow-listed menu action."),
         {string(QStringLiteral("name"), QStringLiteral("QAction object name (see action.list)."))
              .length(1, 1000).matching(QStringLiteral("^action[A-Za-z0-9_]+$")).require()}},
        {QStringLiteral("screenshot.capture"), QStringLiteral("Save a PNG of the main window or a dialog."),
         {target(), string(QStringLiteral("output_dir"), QStringLiteral("Directory for the PNG."))}},
        {QStringLiteral("widget.text"), QStringLiteral("Describe the widgets of a window."),
         {target(),
          string(QStringLiteral("widget"),
                 QStringLiteral("Object name or selector of a widget to read. Returned selectors also identify unnamed widgets "
                                "in widget.click and widget.setValue; reacquire them after reopening a dialog."))
              .length(1, 1000),
          integer(QStringLiteral("max_rows"), QStringLiteral("Row limit for tables and lists."), 1, 1000).withDefault(50),
          integer(QStringLiteral("max_widgets"), QStringLiteral("Widget limit."), 1, 3000).withDefault(300),
          boolean(QStringLiteral("include_children"),
                  QStringLiteral("Also describe descendants of the selected widget, e.g. a dock panel."))}},
        {QStringLiteral("widget.click"), QStringLiteral("Click a button."), {target(), widget().require()}},
        {QStringLiteral("widget.setValue"), QStringLiteral("Change a combo box, spin box, check box, list, tab, color or text field."),
         {target(), widget().require(), any(QStringLiteral("value"), QString()).require(),
          boolean(QStringLiteral("submit"), QStringLiteral("Press Enter on a line edit after the text."))}},
        {QStringLiteral("widget.clickCell"), QStringLiteral("Click a table cell."),
         {target(), widget().require(), integer(QStringLiteral("row"), QString(), 0, 1000000000).require(),
          integer(QStringLiteral("column"), QString(), 0, 1000000000).require()}},
        {QStringLiteral("widget.editCell"), QStringLiteral("Edit a table cell through its line editor."),
         {target(), widget().require(), integer(QStringLiteral("row"), QString(), 0, 1000000000).require(),
          integer(QStringLiteral("column"), QString(), 0, 1000000000).require(),
          string(QStringLiteral("text"), QString()).length(0, 32767).require(),
          boolean(QStringLiteral("commit"), QStringLiteral("Press Enter to commit the edit."), true)}},
        {QStringLiteral("widget.showDock"), QStringLiteral("Show and raise a dock."), {widget().require()}},
        {QStringLiteral("clipboard.get"), QStringLiteral("Clipboard text and image size."),
         {integer(QStringLiteral("max_chars"), QString(), 1, 1000000).withDefault(30000)}},
        {QStringLiteral("dock.list"), QStringLiteral("All main-window docks."), {}},
        {QStringLiteral("dock.configure"), QStringLiteral("Show, hide, float, dock, tabify or drag a dock."),
         {widget().require(),
          choice(QStringLiteral("operation"), QString(), {QStringLiteral("show"), QStringLiteral("hide"), QStringLiteral("float"),
                                                          QStringLiteral("dock"), QStringLiteral("tabify"), QStringLiteral("drag")})
              .require(),
          choice(QStringLiteral("area"), QStringLiteral("For operation=dock."),
                 {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("top"), QStringLiteral("bottom")}),
          string(QStringLiteral("relative_to"), QStringLiteral("For operation=tabify.")).length(1, 1000),
          coordinate(QStringLiteral("x")), coordinate(QStringLiteral("y")),
          object(QStringLiteral("geometry"), QStringLiteral("For operation=float: global client rectangle."),
                 {coordinate(QStringLiteral("x")).require(), coordinate(QStringLiteral("y")).require(),
                  integer(QStringLiteral("width"), QString(), 1, 16384).require(),
                  integer(QStringLiteral("height"), QString(), 1, 16384).require()})}},
        {QStringLiteral("menu.items"), QStringLiteral("Items of a menu or menu button."), {target(), widget().require()}},
        {QStringLiteral("menu.select"), QStringLiteral("Trigger a menu item by a path of indices."),
         {target(), widget().require(),
          integerArray(QStringLiteral("path"), QStringLiteral("Indices from menu.items."), 0).length(1, 10).require()}},
        {QStringLiteral("menu.favorites"), QStringLiteral("Read, or replace with actions, the menu favorites."),
         {stringArray(QStringLiteral("actions"), QStringLiteral("Ordered favorite action names.")).atMost(200)}},
        {QStringLiteral("branch.click"), QStringLiteral("Click a branch tree node."),
         {target(), widget().withDefault(QStringLiteral("branchTreeView")),
          integer(QStringLiteral("id"), QStringLiteral("Node id from widget.text."), 1, 1000000000).require()}},
        {QStringLiteral("ui.interaction"),
         QStringLiteral("State of a queued UI operation: pending, running, done, failed (with error) or cancelled."),
         {integer(QStringLiteral("id"), QStringLiteral("The interaction number returned by the queued operation."), 1,
                  2147483647).require()}},
    };
    return table;
}

const OperationSpec* AutomationMethodSpecs::find(const QString& name)
{
    for (const OperationSpec& spec : all())
        if (spec.name == name) return &spec;
    return nullptr;
}
