#ifndef KIFPARSETYPES_H
#define KIFPARSETYPES_H

/// @file kifparsetypes.h
/// @brief KIF棋譜パース用データ型の定義

#include <QString>
#include <QStringList>
#include <QList>
#include <optional>

#include "recordentry.h"
#include "kifdisplayitem.h"

/// KIFの「(キーワード)：(内容)」行を表すペア
///
struct KifGameInfoItem {
    QString key;    ///< キーワード（例: "開始日時"）
    QString value;  ///< 内容（例: "2025/03/02 09:00"）
};

// --- 分岐用データ構造 ---

/// 1本の手順（本譜または変化）。取り込んだ行は着手・終局・局面・注釈を一緒に持つ
struct KifLine {
    int startPly = 1;
    QString baseSfen;
    std::optional<KifDisplayItem> opening;
    QList<RecordEntry> entries;

    bool endsWithTerminal() const { return !entries.isEmpty() && entries.last().isTerminal(); }
    int moveCount() const { return static_cast<int>(entries.size()) - (endsWithTerminal() ? 1 : 0); }
    QString sfenAt(int offset) const
    {
        return offset == 0 ? baseSfen : (offset > 0 && offset <= moveCount() ? entries.at(offset - 1).sfen : QString());
    }
    QStringList usiMoves() const
    {
        QStringList out;
        for (const auto& entry : entries)
            if (!entry.isTerminal()) out.append(entry.move.move.toString());
        return out;
    }
    QStringList sfenList() const
    {
        QStringList out{baseSfen};
        for (const auto& entry : entries)
            if (!entry.isTerminal()) out.append(entry.sfen);
        return out;
    }
    QList<KifDisplayItem> displayItems() const
    {
        QList<KifDisplayItem> out;
        if (opening) out.append(*opening);
        QString before = baseSfen;
        int ply = startPly;
        for (const auto& record : entries) {
            KifDisplayItem item(record.displayText, record.time.toText(), record.comment, ply++);
            item.bookmark = record.bookmark;
            item.beforeSfen = before;
            item.terminal = record.isTerminal();
            item.terminalType = record.terminal;
            item.terminalSide = record.mover;
            if (!item.terminal) item.usiMove = record.move.move.toString();
            out.append(item);
            before = record.sfen;
        }
        return out;
    }
};

/**
 * @brief 変化（どの手数から分岐するか）を表す構造体
 *
 */
struct KifVariation {
    int startPly = 1;  ///< 変化開始手数（例: 「変化：65手」なら 65）
    KifLine line;      ///< 変化の手順
    int parentVariation = -1; ///< 分岐元の手順。-1 は本譜、それ以外は variations の先行要素
};

/**
 * @brief KIFパース結果（本譜＋変化）を格納する構造体
 *
 */
struct KifParseResult {
    KifLine mainline;                  ///< 本譜
    QList<KifVariation> variations;  ///< 変化リスト
};

#endif // KIFPARSETYPES_H
