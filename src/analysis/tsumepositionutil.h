#ifndef TSUMEPOSITIONUTIL_H
#define TSUMEPOSITIONUTIL_H

/// @file tsumepositionutil.h
/// @brief 詰み探索に送る局面コマンドの組み立て

#include <QString>
#include <QStringList>

#include "usiposition.h"

namespace TsumePositionUtil {

/**
 * @brief 詰み探索に送る局面コマンドを1つの局面から作る
 *
 * 玉の有無から攻方を推定し、手番を攻方に、手数を 1 にする（玉が片方だけなら、その玉のない側が攻方）。
 * sfenLike は素の SFEN のほか "position sfen ..."（moves 以降は無視）も受け付ける。空なら空文字列。
 * position の文字列は UsiPosition::commandFor で作る。
 */
inline QString buildPositionForMate(const QString& sfenLike)
{
    QString baseSfen = sfenLike.trimmed();
    if (baseSfen.startsWith(QStringLiteral("position sfen "))) {
        baseSfen = baseSfen.mid(14).trimmed();
        const qsizetype m = baseSfen.indexOf(QStringLiteral(" moves "));
        if (m >= 0) baseSfen = baseSfen.left(m).trimmed();
    }
    QStringList tokens = baseSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) return QString();

    const QString board = tokens.at(0);
    const bool blackKing = board.contains(QLatin1Char('K'));
    const bool whiteKing = board.contains(QLatin1Char('k'));
    QString turn = tokens.size() >= 2 && tokens.at(1) == QLatin1String("w") ? QStringLiteral("w") : QStringLiteral("b");
    if (blackKing != whiteKing) turn = whiteKing ? QStringLiteral("b") : QStringLiteral("w");

    if (tokens.size() < 2) tokens << turn << QStringLiteral("-") << QStringLiteral("1");
    tokens[1] = turn;
    if (tokens.size() >= 4) tokens[3] = QStringLiteral("1");
    return UsiPosition::commandFor(tokens.join(QLatin1Char(' ')), {});
}

} // namespace TsumePositionUtil

#endif // TSUMEPOSITIONUTIL_H
