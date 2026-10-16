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

/// 変化の分岐元（どの手順の、どの局面から分かれるか）
struct KifBranchPoint {
    int parentVariation = -1;  ///< 分岐元の手順。-1 は本譜
    int startPly = 1;          ///< 変化の最初の手の手数
    QString sfen;              ///< 変化の最初の手を指す前の局面
    QString previousUsi;       ///< その局面に至った手（「同」の解決用。開始局面なら空）
};

/**
 * @brief 取り込んだ棋譜（本譜＋変化）
 *
 * 変化は addVariation で分岐元（KifBranchPoint）と一緒に足す。分岐元はいつも先に書かれた手順なので、
 * variations の並びのまま木にできる。
 */
struct KifParseResult {
    KifLine mainline;                  ///< 本譜
    QList<KifVariation> variations;  ///< 変化リスト（書かれた順）

    /// 手順（-1 は本譜）
    const KifLine& lineAt(int variation) const
    {
        return variation < 0 ? mainline : variations.at(variation).line;
    }

    /// 手順 parent の startPly 手目の手の前で分かれる分岐元（手順がその手の前まで届かなければ nullopt）
    std::optional<KifBranchPoint> branchPoint(int parent, int startPly) const
    {
        if (parent < -1 || parent >= variations.size() || startPly < 1) return std::nullopt;
        const KifLine& line = lineAt(parent);
        const int offset = startPly - firstPlyOf(parent);  // 手順の中で、分岐の前に指した手の数
        if (offset < 0 || offset > line.moveCount()) return std::nullopt;
        KifBranchPoint point;
        point.parentVariation = parent;
        point.startPly = startPly;
        point.sfen = line.sfenAt(offset);
        if (offset > 0) {
            point.previousUsi = line.entries.at(offset - 1).move.move.toString();
        } else if (parent >= 0) {
            // 手順の最初の手の前で分かれるなら、直前の手は分岐元の手順にある
            const KifVariation& variation = variations.at(parent);
            if (const auto up = branchPoint(variation.parentVariation, variation.startPly)) point.previousUsi = up->previousUsi;
        }
        return point;
    }

    /**
     * @brief 「変化：N手」の分岐元（KIF・KI2・USEN のように、変化を分岐元の後に並べる形式）
     *
     * N-1 手目の指し手を持つ手順のうち最後に書かれたものから分かれる（同じ手数から始まる兄弟の変化は
     * 分岐元にならない）。N=1 なら本譜の開始局面から。どの手順も N-1 手目まで届かなければ nullopt
     */
    std::optional<KifBranchPoint> latestBranchPoint(int startPly) const
    {
        if (startPly < 1) return std::nullopt;
        for (int i = static_cast<int>(variations.size()) - 1; i >= 0; --i) {
            const KifVariation& variation = variations.at(i);
            if (variation.startPly < startPly && startPly - variation.startPly <= variation.line.moveCount())
                return branchPoint(i, startPly);
        }
        return branchPoint(-1, startPly);
    }

    /// 手順の最初の手の手数（-1 は本譜）
    int firstPlyOf(int variation) const
    {
        return variation < 0 ? mainline.startPly : variations.at(variation).startPly;
    }

    /// 変化を足す（line は from.sfen から始まる手順）
    void addVariation(const KifBranchPoint& from, KifLine line)
    {
        KifVariation variation;
        variation.startPly = from.startPly;
        variation.parentVariation = from.parentVariation;
        variation.line = std::move(line);
        variations.append(std::move(variation));
    }
};

#endif // KIFPARSETYPES_H
