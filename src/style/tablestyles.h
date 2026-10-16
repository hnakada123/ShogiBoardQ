/// @file tablestyles.h
/// @brief 棋譜・エンジン情報・思考一覧で共有する表の配色

#ifndef TABLESTYLES_H
#define TABLESTYLES_H

#include <QColor>
#include <QString>

namespace TableStyles {

inline QColor selectionBackground() { return QColor(0xe7, 0xf0, 0xf8); }
inline QColor selectionText() { return QColor(0x24, 0x3b, 0x53); }
inline QColor selectionAccent() { return QColor(0x52, 0x79, 0x9c); }

inline QString header(int fontSize = 0)
{
    const QString fontRule = fontSize > 0
        ? QStringLiteral("font-size: %1pt;").arg(fontSize) : QString();
    return QStringLiteral(
        "QHeaderView::section {"
        "  background-color: #f1f3f5; color: #46515c;"
        "  font-weight: normal; %1"
        "  padding: 2px 6px; border: none;"
        "  border-bottom: 1px solid #d8dee5;"
        "}").arg(fontRule);
}

inline QString selection()
{
    return QStringLiteral(
        "QTableView { selection-background-color: %1; selection-color: %2; }"
        "QTableView::item:selected:active, QTableView::item:selected:!active {"
        "  background-color: %1; color: %2;"
        "}").arg(selectionBackground().name(), selectionText().name());
}

inline QString thinking(int fontSize = 0)
{
    return header(fontSize) + selection() + QStringLiteral(
        "QTableView { background-color: #ffffff; color: #303841;"
        "  border: 1px solid #d8dee5; }"
        "QTableView::item { border: none; border-bottom: 1px solid #e6eaee; }");
}

} // namespace TableStyles

#endif // TABLESTYLES_H
