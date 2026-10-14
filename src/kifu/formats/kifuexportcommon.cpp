/// @file kifuexportcommon.cpp
/// @brief 棋譜エクスポータ共通の文字列処理の実装

#include "kifuexportcommon.h"
#include "parsecommon.h"

#include <QRegularExpression>
#include <array>

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

bool isTerminalMove(const QString& move)
{
    // 読み込みと同じ終局語に、CSA V3 の終局（引き分け・最大手数・エラー）と反則全般を足す
    static const std::array<QString, 4> extra = {{
        QStringLiteral("引き分け"), QStringLiteral("最大手数"), QStringLiteral("反則"), QStringLiteral("エラー")
    }};
    const QString stripped = removeTurnMarker(move);
    if (KifuParseCommon::containsAnyTerminal(stripped)) return true;
    for (const QString& word : extra) {
        if (stripped.contains(word)) return true;
    }
    return false;
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

QString buildEndingLine(int lastActualMoveNo, const QString& terminalMove, bool initialBlackToMove,
                        bool handicapNames)
{
    const QString stripped = removeTurnMarker(terminalMove);
    const QString n = QString::number(lastActualMoveNo);

    // 最後の指し手を指した側（奇数手なら開始局面の手番側）
    const bool lastMoveBySente = ((lastActualMoveNo % 2 != 0) == initialBlackToMove);
    // 駒落ちは柿木形式と同じく下手（先手側）・上手（後手側）と書く
    const QString senteStr = handicapNames ? QStringLiteral("下手") : QStringLiteral("先手");
    const QString goteStr = handicapNames ? QStringLiteral("上手") : QStringLiteral("後手");

    if (stripped.contains(QStringLiteral("投了"))) {
        // 直前の指し手側が勝ち
        return QStringLiteral("まで%1手で%2の勝ち").arg(n, lastMoveBySente ? senteStr : goteStr);
    }
    if (stripped.contains(QStringLiteral("不詰"))) {
        return QStringLiteral("まで%1手で不詰").arg(n);
    }
    if (stripped.contains(QStringLiteral("詰み"))) {
        return QStringLiteral("まで%1手で詰み").arg(n);
    }
    // 終局語の手番記号は、その終局を宣言した側（＝指す番の側）を表す。記号がなければ次に指す側
    const bool declaredBySente = terminalMove.startsWith(QStringLiteral("▲"))
        || (!terminalMove.startsWith(QStringLiteral("△")) && !lastMoveBySente);
    const QString declarer = declaredBySente ? senteStr : goteStr;
    const QString opponent = declaredBySente ? goteStr : senteStr;
    if (stripped.contains(QStringLiteral("切れ負け")) || stripped.contains(QStringLiteral("時間切れ"))) {
        // 手番側の時間切れ
        return QStringLiteral("まで%1手で時間切れにより%2の勝ち").arg(n, opponent);
    }
    if (stripped.contains(QStringLiteral("反則勝ち"))) {
        return QStringLiteral("まで%1手で%2の反則勝ち").arg(n, declarer);
    }
    if (stripped.contains(QStringLiteral("反則負け"))) {
        return QStringLiteral("まで%1手で%2の反則負け").arg(n, declarer);
    }
    if (stripped.contains(QStringLiteral("入玉勝ち")) || stripped.contains(QStringLiteral("宣言勝ち"))) {
        return QStringLiteral("まで%1手で%2の入玉勝ち").arg(n, declarer);
    }
    if (stripped.contains(QStringLiteral("不戦勝"))) {
        return QStringLiteral("まで%1手で%2の不戦勝").arg(n, declarer);
    }
    if (stripped.contains(QStringLiteral("不戦敗"))) {
        return QStringLiteral("まで%1手で%2の不戦敗").arg(n, declarer);
    }
    if (stripped.contains(QStringLiteral("千日手"))) {
        return QStringLiteral("まで%1手で千日手").arg(n);
    }
    if (stripped.contains(QStringLiteral("持将棋"))) {
        return QStringLiteral("まで%1手で持将棋").arg(n);
    }
    if (stripped.contains(QStringLiteral("中断"))) {
        return QStringLiteral("まで%1手で中断").arg(n);
    }
    return QStringLiteral("まで%1手").arg(n);
}

} // namespace KifuExportCommon
