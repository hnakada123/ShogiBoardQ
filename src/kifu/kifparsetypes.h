#ifndef KIFPARSETYPES_H
#define KIFPARSETYPES_H

/// @file kifparsetypes.h
/// @brief 棋譜の読み込み結果（本譜・変化・対局情報）と、読めない手の扱い

#include <QString>
#include <QStringList>
#include <QList>
#include <optional>

#include "recordline.h"
#include "shogiposition.h"
#include "usimove.h"

/// KIFの「(キーワード)：(内容)」行を表すペア
///
struct KifGameInfoItem {
    QString key;    ///< キーワード（例: "開始日時"）
    QString value;  ///< 内容（例: "2025/03/02 09:00"）

    friend bool operator==(const KifGameInfoItem& a, const KifGameInfoItem& b)
    {
        return a.key == b.key && a.value == b.value;
    }
    friend bool operator!=(const KifGameInfoItem& a, const KifGameInfoItem& b) { return !(a == b); }
};

// --- 分岐用データ構造 ---

/**
 * @brief 変化（どの手数から分岐するか）を表す構造体
 *
 */
struct KifVariation {
    int startPly = 1;  ///< 変化開始手数（例: 「変化：65手」なら 65）
    RecordLine line;   ///< 変化の手順（line.startPly == startPly）
    int parentVariation = -1; ///< 分岐元の手順。-1 は本譜、それ以外は variations の先行要素
};

/// 変化の分岐元（どの手順の、どの局面から分かれるか）
struct KifBranchPoint {
    int parentVariation = -1;  ///< 分岐元の手順。-1 は本譜
    int startPly = 1;          ///< 変化の最初の手の手数
    std::optional<Position> position;  ///< 変化の最初の手を指す前の局面（手順の局面が読めなければ空）
    std::optional<UsiMove> previousMove;  ///< その局面に至った手（「同」の解決用。開始局面なら nullopt）
};

/**
 * @brief 取り込んだ棋譜（本譜＋変化）
 *
 * 変化は addVariation で分岐元（KifBranchPoint）と一緒に足す。分岐元はいつも先に書かれた手順なので、
 * variations の並びのまま木にできる。
 */
struct KifParseResult {
    RecordLine mainline;             ///< 本譜
    QList<KifVariation> variations;  ///< 変化リスト（書かれた順）

    /// 手順（-1 は本譜）
    const RecordLine& lineAt(int variation) const
    {
        return variation < 0 ? mainline : variations.at(variation).line;
    }

    /// 手順 parent の startPly 手目の手の前で分かれる分岐元（手順がその手の前まで届かなければ nullopt）
    std::optional<KifBranchPoint> branchPoint(int parent, int startPly) const
    {
        if (parent < -1 || parent >= variations.size() || startPly < 1) return std::nullopt;
        const RecordLine& line = lineAt(parent);
        const int offset = startPly - firstPlyOf(parent);  // 手順の中で、分岐の前に指した手の数
        if (offset < 0 || offset > line.moveCount()) return std::nullopt;
        KifBranchPoint point;
        point.parentVariation = parent;
        point.startPly = startPly;
        point.position = Position::fromSfen(line.sfenAt(offset));
        if (offset > 0) {
            point.previousMove = line.entries.at(offset - 1).move.move;
        } else if (parent >= 0) {
            // 手順の最初の手の前で分かれるなら、直前の手は分岐元の手順にある
            const KifVariation& variation = variations.at(parent);
            if (const auto up = branchPoint(variation.parentVariation, variation.startPly))
                point.previousMove = up->previousMove;
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
    void addVariation(const KifBranchPoint& from, RecordLine line)
    {
        KifVariation variation;
        variation.startPly = from.startPly;
        variation.parentVariation = from.parentVariation;
        line.startPly = from.startPly;
        variation.line = std::move(line);
        variations.append(std::move(variation));
    }
};

/**
 * @brief 読めない手・指せない手の扱い（全形式で同じ。ここだけで決める）
 *
 * 読めない手（表記が解釈できない・局面に合わない）・指せない手・パスがあったときに、
 * その手順をその手の手前で打ち切って読み込みを続けるか、棋譜全体を読めなかったことにするか。
 */
struct ParseOptions {
    enum class UnreadableMove {
        TruncateLine,  ///< その手の手前で手順を打ち切り、警告に「何手目まで読んだか」を書いて続ける
        FailRecord,    ///< 棋譜全体を読めなかったことにする
    };
    UnreadableMove unreadableMove = UnreadableMove::TruncateLine;
};

/**
 * @brief 読み込み中の警告と失敗の記録（ParseOptions の方針で、読めない手をどう扱うかを決める）
 */
class ParseReport
{
public:
    explicit ParseReport(const ParseOptions& options = {}) : m_options(options) {}

    const ParseOptions& options() const { return m_options; }
    /// 警告を1行足す（末尾の改行は付ける）
    void warn(const QString& message)
    {
        m_warnings += message;
        if (!message.endsWith(QLatin1Char('\n'))) m_warnings += QLatin1Char('\n');
    }
    /// 棋譜全体を読めなかったことにする
    void fail(const QString& message)
    {
        warn(message);
        m_failed = true;
    }
    /**
     * @brief ply 手目が読めない・指せない（reason は "KIF: cannot parse move 5: ..." のような1行）
     * @return 手順を打ち切って読み込みを続けるなら true、棋譜全体を読めなかったことにしたなら false
     */
    bool rejectMove(int ply, const QString& reason)
    {
        if (m_options.unreadableMove == ParseOptions::UnreadableMove::FailRecord) {
            fail(reason);
            return false;
        }
        warn(reason);
        m_warnings += truncationNotice(ply);
        return true;
    }
    /// ply 手目のパス（未対応の手として rejectMove と同じに扱う）
    bool rejectPass(const QString& format, int ply)
    {
        return rejectMove(ply, QStringLiteral("%1: Move %2 is a pass, which is not supported.").arg(format).arg(ply));
    }

    bool failed() const { return m_failed; }
    const QString& warnings() const { return m_warnings; }

    /// 手順を ply 手目の手前で打ち切ったときの警告
    static QString truncationNotice(int ply)
    {
        return ply > 1 ? QStringLiteral("The record was read up to move %1.\n").arg(ply - 1)
                       : QStringLiteral("No moves were read from this line.\n");
    }

private:
    ParseOptions m_options;
    QString m_warnings;
    bool m_failed = false;
};

/// 棋譜の開始局面の書き方
enum class StartKind {
    Default,    ///< 書かれていない（平手とみなす）
    Handicap,   ///< 手合割の名前（平手・香落ちなど。USI の startpos・JKF の preset を含む）
    Diagram,    ///< 局面図（KIF・KI2 の BOD と「配置」、JKF の data）
    Specified,  ///< 局面の値（SFEN・USEN の局面、CSA の平手以外の局面）
};

/**
 * @brief 1回の解析で得る棋譜のすべて（形式ごとの parse() が返す）
 *
 * 棋譜（本譜・変化）・開始局面の書き方・対局情報・警告を同じ解析で作る。
 * 開始局面は本譜の開始局面（record.mainline.startSfen）だけに置く。
 */
struct KifuParseOutcome {
    bool success = false;
    KifParseResult record;
    StartKind startKind = StartKind::Default;
    bool positionOnly = false;         ///< 指し手のない局面だけの棋譜（USI・SFEN・BOD）
    QList<KifGameInfoItem> gameInfo;   ///< 対局情報（書かれた順）
    QString warning;                   ///< 警告（失敗したときは理由）
};

#endif // KIFPARSETYPES_H
