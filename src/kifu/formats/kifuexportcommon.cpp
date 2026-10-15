/// @file kifuexportcommon.cpp
/// @brief 棋譜エクスポータ共通の文字列処理の実装

#include "kifuexportcommon.h"

#include <QRegularExpression>

namespace KifuExportCommon {

QString fwColonLine(const QString& key, const QString& value)
{
    return QStringLiteral("%1：%2").arg(key, value);
}

QString removeTurnMarker(const QString& move)
{
    if (move.startsWith(QStringLiteral("▲")) || move.startsWith(QStringLiteral("△"))) return move.mid(1);
    return move;
}

void appendKifBookmarks(const QString& bookmark, QStringList& out)
{
    if (bookmark.trimmed().isEmpty()) return;

    const QStringList lines = bookmark.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& name : lines) {
        const QString t = name.trimmed();
        if (!t.isEmpty()) out << (QStringLiteral("&") + t);
    }
}

void appendKifComments(const QString& comment, QStringList& out)
{
    if (comment.trimmed().isEmpty()) return;

    static const QRegularExpression newlineRe(QStringLiteral("\r?\n"));
    const QStringList lines = comment.split(newlineRe, Qt::KeepEmptyParts);
    for (const QString& raw : lines) {
        const QString t = raw.trimmed();
        // 後方互換: コメント中の【しおり】マーカーを & 行として出力
        if (t.startsWith(QStringLiteral("【しおり】"))) {
            const QString name = t.mid(5).trimmed(); // "【しおり】" は5文字
            if (!name.isEmpty()) out << (QStringLiteral("&") + name);
            continue;
        }
        // モデルのコメントは接頭辞を除いた本文。本文先頭の * も保持する。
        out << (QStringLiteral("*") + t);
    }
}

QString buildEndingLine(int lastActualMoveNo, TerminalType terminal, Turn side, bool handicapNames)
{
    const QString n = QString::number(lastActualMoveNo);
    const QString black = handicapNames ? QStringLiteral("下手") : QStringLiteral("先手");
    const QString white = handicapNames ? QStringLiteral("上手") : QStringLiteral("後手");
    const QString declarer = side == Turn::Black ? black : white;
    const QString opponent = side == Turn::Black ? white : black;
    switch (terminal) {
    case TerminalType::Resign: return QStringLiteral("まで%1手で%2の勝ち").arg(n, opponent);
    case TerminalType::NoCheckmate: return QStringLiteral("まで%1手で不詰").arg(n);
    case TerminalType::Checkmate: return QStringLiteral("まで%1手で詰み").arg(n);
    case TerminalType::Timeout: return QStringLiteral("まで%1手で時間切れにより%2の勝ち").arg(n, opponent);
    case TerminalType::IllegalWin: return QStringLiteral("まで%1手で%2の反則勝ち").arg(n, declarer);
    case TerminalType::IllegalLoss: return QStringLiteral("まで%1手で%2の反則負け").arg(n, declarer);
    case TerminalType::DeclarationWin: return QStringLiteral("まで%1手で%2の入玉勝ち").arg(n, declarer);
    case TerminalType::ForfeitWin: return QStringLiteral("まで%1手で%2の不戦勝").arg(n, declarer);
    case TerminalType::Forfeit: return QStringLiteral("まで%1手で%2の不戦敗").arg(n, declarer);
    case TerminalType::PerpetualCheck:
    case TerminalType::Repetition: return QStringLiteral("まで%1手で千日手").arg(n);
    case TerminalType::Impasse: return QStringLiteral("まで%1手で持将棋").arg(n);
    case TerminalType::Interrupt: return QStringLiteral("まで%1手で中断").arg(n);
    default: return QStringLiteral("まで%1手").arg(n);
    }
}

} // namespace KifuExportCommon
