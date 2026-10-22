/// @file josekimovepicker.cpp
/// @brief 対局で指す定跡手を選ぶ処理の実装

#include "josekimovepicker.h"

#include "enginemovevalidator.h"
#include "josekibook.h"
#include "shogiposition.h"
#include "usimove.h"

#include <QRandomGenerator>

#include <algorithm>

namespace {

/// 出現頻度の合計（負の値は0とみなす）
qint64 frequencyTotal(const QList<JosekiMove> &moves)
{
    qint64 total = 0;
    for (const JosekiMove &move : moves) total += qMax(move.frequency, 0);
    return total;
}

double ratioPercent(const JosekiMove &move, qint64 total)
{
    return total > 0 ? 100.0 * qMax(move.frequency, 0) / static_cast<double>(total) : 0.0;
}

/// 出現頻度に比例した確率で選ぶ（出現頻度がすべて0なら等確率）
qsizetype pickByFrequency(const QList<JosekiMove> &moves, QRandomGenerator &random)
{
    const qint64 total = frequencyTotal(moves);
    if (total <= 0) return random.bounded(static_cast<int>(moves.size()));
    double remaining = random.generateDouble() * static_cast<double>(total);
    for (qsizetype i = 0; i < moves.size(); ++i) {
        remaining -= qMax(moves.at(i).frequency, 0);
        if (remaining < 0) return i;
    }
    return moves.size() - 1;
}

/// 最善の評価値から diff 以内の手を等確率で選ぶ
qsizetype pickByValue(const QList<JosekiMove> &moves, int diff, QRandomGenerator &random)
{
    const auto best = std::max_element(moves.cbegin(), moves.cend(), [](const JosekiMove &a, const JosekiMove &b) {
        return a.value < b.value;
    });
    QList<qsizetype> pool;
    for (qsizetype i = 0; i < moves.size(); ++i) {
        if (static_cast<qint64>(moves.at(i).value) >= static_cast<qint64>(best->value) - diff) pool.append(i);
    }
    return pool.at(random.bounded(static_cast<int>(pool.size())));
}

} // namespace

namespace JosekiMovePicker {

QList<JosekiMove> candidates(const QList<JosekiMove> &moves, const QString &sfen, const JosekiPlayOptions &options)
{
    QList<JosekiMove> result;
    const std::optional<Position> position = Position::fromSfenLike(sfen);
    if (!position) return result;
    const JosekiPlayOptions conditions = options.normalized();
    const EngineMoveValidator::Context context = EngineMoveValidator::load(*position);
    const qint64 total = frequencyTotal(moves);
    for (const JosekiMove &move : moves) {
        // 定跡ファイルの誤りで指せない手を指すと反則負けになるので、局面で指せる手だけを使う
        const std::optional<UsiMove> usi = UsiMove::parse(move.move);
        if (!usi || !EngineMoveValidator::isLegal(context, *usi)) continue;
        if (move.depth < conditions.minDepth || move.frequency < conditions.minFrequency) continue;
        if (conditions.useMinValue && move.value < conditions.minValue) continue;
        // 表の比率（小数1桁）と同じ値が下限ちょうどでも使えるように、わずかな誤差は許す
        if (ratioPercent(move, total) + 1e-9 < conditions.minRatioPercent) continue;
        result.append(move);
    }
    return result;
}

std::optional<JosekiPick> pick(const JosekiBook &book, const QString &sfen, const JosekiPlayOptions &options,
                               QRandomGenerator &random)
{
    const JosekiPlayOptions conditions = options.normalized();
    if (!conditions.enabled) return std::nullopt;
    const std::optional<Position> position = Position::fromSfenLike(sfen);
    if (!position || position->moveNumber() > conditions.maxPly) return std::nullopt;
    const QList<JosekiMove> moves = book.movesAt(sfen);
    const QList<JosekiMove> found = candidates(moves, sfen, conditions);
    if (found.isEmpty()) return std::nullopt;

    qsizetype index = 0;
    switch (conditions.method) {
    case JosekiPlayOptions::FirstMove: index = 0; break;
    case JosekiPlayOptions::ByValue: index = pickByValue(found, conditions.valueDiff, random); break;
    default: index = pickByFrequency(found, random); break;
    }
    JosekiPick picked;
    picked.move = found.at(index);
    picked.ratioPercent = ratioPercent(picked.move, frequencyTotal(moves));
    return picked;
}

} // namespace JosekiMovePicker
