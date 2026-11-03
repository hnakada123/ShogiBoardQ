/// @file plaintooltip.cpp
/// @brief 外から来た文字列を、文字のまま出すツールチップの実装

#include "plaintooltip.h"

#include <QTextDocument>

QString plainToolTip(const QString& text)
{
    return plainToolTip(QStringList{text});
}

QString plainToolTip(const QStringList& sections)
{
    QStringList escaped;
    for (const QString& section : sections) {
        if (!section.isEmpty()) escaped.append(section.toHtmlEscaped());
    }
    if (escaped.isEmpty()) return {};
    // <qt> で必ず HTML として読ませ（タグの無い文字はそのまま出る）、空白と改行は pre-wrap で残す
    return QStringLiteral("<qt style='white-space:pre-wrap'>%1</qt>").arg(escaped.join(QStringLiteral("<hr/>")));
}

QString shownToolTipText(const QString& toolTip)
{
    if (!Qt::mightBeRichText(toolTip)) return toolTip;
    QTextDocument document;
    document.setHtml(toolTip);
    return document.toPlainText();
}
