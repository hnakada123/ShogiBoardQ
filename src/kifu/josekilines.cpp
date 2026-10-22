/// @file josekilines.cpp
/// @brief 定跡と棋譜の手順の処理の実装

#include "josekilines.h"

#include "enginemovevalidator.h"
#include "josekibook.h"
#include "kiflinebuilder.h"
#include "shogiposition.h"

#include <QSet>

namespace JosekiLines {

namespace {

/// 深さ優先でたどり、行き止まりごとに手順を作る
class Expander
{
public:
    Expander(const JosekiBook &book, const Position &start, int startPly, std::optional<UsiMove> previous,
             const ExpandOptions &options)
        : m_book(book)
        , m_start(start)
        , m_startPly(startPly)
        , m_previous(previous)
        , m_options(options)
    {
        m_visited.insert(JosekiBook::positionKey(start.toSfen()));
    }

    ExpandResult run()
    {
        visit(m_start, 0);
        return std::move(m_result);
    }

private:
    /// 展開する定跡手（表の順。指せない手・比率の低い手を除き、上限の数まで）
    QList<UsiMove> movesAt(const Position &position) const
    {
        QList<UsiMove> chosen;
        const QList<JosekiMove> moves = m_book.movesAt(position.toSfen());
        qint64 total = 0;
        for (const JosekiMove &move : moves) total += qMax(move.frequency, 0);
        const EngineMoveValidator::Context context = EngineMoveValidator::load(position);
        for (const JosekiMove &move : moves) {
            if (chosen.size() >= m_options.maxMovesPerPosition) break;
            const std::optional<UsiMove> usi = UsiMove::parse(move.move);
            if (!usi || !EngineMoveValidator::isLegal(context, *usi)) continue;
            // 表の比率（小数1桁）と同じ値が下限ちょうどでも使えるように、わずかな誤差は許す
            const double ratio = total > 0 ? 100.0 * qMax(move.frequency, 0) / static_cast<double>(total) : 100.0;
            if (ratio + 1e-9 < m_options.minRatioPercent) continue;
            chosen.append(*usi);
        }
        return chosen;
    }

    void visit(const Position &position, int depth)
    {
        const QList<UsiMove> moves = depth < m_options.maxPlies ? movesAt(position) : QList<UsiMove>();
        bool extended = false;
        for (const UsiMove &move : moves) {
            if (m_result.moveCount >= m_options.maxMoves) {
                m_result.truncated = true;
                break;
            }
            Position next = position;
            if (!next.apply(move)) continue;
            extended = true;
            ++m_result.moveCount;
            m_path.append(move);
            const QString key = JosekiBook::positionKey(next.toSfen());
            if (m_visited.contains(key)) {
                // 別の手順で着いた局面から先は、先に着いた手順で展開している
                ++m_result.transpositions;
                addLine();
            } else {
                m_visited.insert(key);
                visit(next, depth + 1);
            }
            m_path.removeLast();
        }
        if (!extended && !m_path.isEmpty()) addLine();
    }

    void addLine()
    {
        KifLineBuilder builder(m_start, m_startPly, m_previous, KifLineBuilder::LineKind::Variation);
        for (const UsiMove &move : std::as_const(m_path)) {
            if (!builder.addMove(move)) break;
        }
        m_result.lines.append(builder.take());
    }

    const JosekiBook &m_book;
    Position m_start;
    int m_startPly;
    std::optional<UsiMove> m_previous;
    ExpandOptions m_options;
    QSet<QString> m_visited;   ///< たどった局面（手数を除いた SFEN）
    QList<UsiMove> m_path;     ///< 開始局面から今の局面までの手
    ExpandResult m_result;
};

} // namespace

ExpandResult expand(const JosekiBook &book, const QString &startSfen, int startPly, std::optional<UsiMove> previous,
                    const ExpandOptions &options)
{
    const std::optional<Position> start = Position::fromSfenLike(startSfen);
    if (!start) return {};
    return Expander(book, *start, startPly, previous, options).run();
}

std::optional<int> firstDeviation(const JosekiBook &book, const QStringList &sfens, const QStringList &usiMoves)
{
    for (int i = 0; i < usiMoves.size() && i < sfens.size(); ++i) {
        if (!book.hasMove(sfens.at(i), usiMoves.at(i))) return i;
    }
    return std::nullopt;
}

} // namespace JosekiLines
