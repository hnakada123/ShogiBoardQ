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

/**
 * @brief 1本の手順（分岐ライン）を表す構造体
 *
 */
/// 取り込んだ1行。着手・終局・局面・注釈が常に一緒に移動する。
struct ImportedRecordEntry {
    RecordEntry record;
    QString comment;
    QString bookmark;
};

struct KifLine {
    int startPly = 1;
    QString baseSfen;
    std::optional<KifDisplayItem> opening;
    QList<ImportedRecordEntry> entries;

    bool endsWithTerminal() const { return !entries.isEmpty() && entries.last().record.isTerminal(); }
    int moveCount() const { return static_cast<int>(entries.size()) - (endsWithTerminal() ? 1 : 0); }
    QString sfenAt(int offset) const
    {
        return offset == 0 ? baseSfen : (offset > 0 && offset <= moveCount() ? entries.at(offset - 1).record.sfen : QString());
    }
    QStringList usiMoves() const
    {
        QStringList out;
        for (const auto& entry : entries)
            if (!entry.record.isTerminal()) out.append(entry.record.move.toUsiMove().toString());
        return out;
    }
    QStringList sfenList() const
    {
        QStringList out{baseSfen};
        for (const auto& entry : entries)
            if (!entry.record.isTerminal()) out.append(entry.record.sfen);
        return out;
    }
    QList<KifDisplayItem> displayItems() const
    {
        QList<KifDisplayItem> out;
        if (opening) out.append(*opening);
        QString before = baseSfen;
        int ply = startPly;
        for (const auto& entry : entries) {
            const auto& record = entry.record;
            KifDisplayItem item(record.displayText, record.time.toText(), entry.comment, ply++);
            item.bookmark = entry.bookmark;
            item.beforeSfen = before;
            item.terminal = record.isTerminal();
            item.terminalType = record.terminal;
            item.terminalSide = record.mover;
            if (!item.terminal) item.usiMove = record.move.toUsiMove().toString();
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
