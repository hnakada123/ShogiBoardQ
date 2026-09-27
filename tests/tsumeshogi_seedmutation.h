#pragma once

// 問題集補充用の種局面の変更。出力は未検証候補であり、採択には別途実エンジン検査が必要。
#include "tsumeshogipositiongenerator.h"
#include <QFile>
#include <QList>
#include <QRandomGenerator>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <cstdlib>

namespace seed_mutation {
struct Position {
    int board[81] = {};
    int hand[7] = {};
    int matePlies = 0;
};

inline QList<Position> loadContents(const QString& contents, QSet<QString>* known = nullptr)
{
    QList<Position> result;
    const QString pieces = QStringLiteral("PLNSGBR");
    for (const QString& line : contents.split(QLatin1Char('\n'))) {
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 4) continue;
        Position position;
        if (fields.size() > 5 && fields.at(4) == QLatin1String("moves"))
            position.matePlies = static_cast<int>(fields.size()) - 5;
        int square = 0;
        bool promoted = false;
        for (const QChar ch : fields.at(0)) {
            if (ch == QLatin1Char('/')) continue;
            if (ch == QLatin1Char('+')) { promoted = true; continue; }
            if (ch.isDigit()) { square += ch.digitValue(); continue; }
            int type = ch == QLatin1Char('k') ? 8 : static_cast<int>(pieces.indexOf(ch.toUpper())) + 1;
            if (promoted) type += 10;
            if (square < 81) position.board[square++] = ch.isUpper() ? type : -type;
            promoted = false;
        }
        int count = 0;
        for (const QChar ch : fields.at(2)) {
            if (ch.isDigit()) { count = count * 10 + ch.digitValue(); continue; }
            const int index = static_cast<int>(pieces.indexOf(ch));
            if (index >= 0) position.hand[index] += std::max(1, count);
            count = 0;
        }
        result.append(position);
        if (known) known->insert(fields.mid(0, 4).join(QLatin1Char(' ')));
    }
    return result;
}

inline QList<Position> load(const QString& path, QSet<QString>* known = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return loadContents(QString::fromUtf8(file.readAll()), known);
}

inline QString sfen(Position position, const TsumeshogiPositionGenerator::Settings& settings)
{
    const int maximum[7] = {18, 4, 4, 4, 4, 2, 2};
    const QString pieces = QStringLiteral("PLNSGBR");
    int remaining[7];
    int attackCount = 0;
    int defendCount = 0;
    int kings = 0;
    bool pawns[2][9] = {};
    for (int i = 0; i < 7; ++i) {
        if (position.hand[i] < 0) return {};
        remaining[i] = maximum[i] - position.hand[i];
        attackCount += position.hand[i];
    }
    for (int square = 0; square < 81; ++square) {
        int& piece = position.board[square];
        if (piece == 0) continue;
        if (piece == -8) { ++kings; continue; }
        const int type = std::abs(piece) % 10;
        if (type < 1 || type > 7) return {};
        if (--remaining[type - 1] < 0) return {};
        const bool black = piece > 0;
        if (black) ++attackCount; else ++defendCount;
        const int rank = black ? square / 9 : 8 - square / 9;
        // Never place an unpromoted pawn/lance/knight on a dead rank.
        if (std::abs(piece) < 10 && ((rank == 0 && (type == 1 || type == 2)) || (rank <= 1 && type == 3)))
            piece += black ? 10 : -10;
        if (std::abs(piece) == 1) {
            const int color = black ? 0 : 1;
            if (pawns[color][square % 9]) return {};
            pawns[color][square % 9] = true;
        }
    }
    if (kings != 1 || attackCount < 1 || attackCount > settings.maxAttackPieces || defendCount > settings.maxDefendPieces) return {};
    for (const int count : remaining) if (count < 0) return {};
    QString board;
    for (int rank = 0; rank < 9; ++rank) {
        if (rank > 0) board += QLatin1Char('/');
        int empty = 0;
        for (int file = 0; file < 9; ++file) {
            const int piece = position.board[rank * 9 + file];
            if (piece == 0) { ++empty; continue; }
            if (empty > 0) { board += QString::number(empty); empty = 0; }
            if (piece == -8) { board += QLatin1Char('k'); continue; }
            if (std::abs(piece) > 10) board += QLatin1Char('+');
            const QChar letter = pieces.at(std::abs(piece) % 10 - 1);
            board += piece > 0 ? letter : letter.toLower();
        }
        if (empty > 0) board += QString::number(empty);
    }
    QString hand;
    for (int color = 0; color < 2; ++color) {
        for (int index = 6; index >= 0; --index) {
            const int count = color == 0 ? position.hand[index] : remaining[index];
            if (count == 0) continue;
            if (count > 1) hand += QString::number(count);
            hand += color == 0 ? pieces.at(index) : pieces.at(index).toLower();
        }
    }
    return board + QStringLiteral(" b ") + (hand.isEmpty() ? QStringLiteral("-") : hand) + QStringLiteral(" 1");
}

inline QString sample(const QList<Position>& seeds, const TsumeshogiPositionGenerator::Settings& settings)
{
    if (seeds.isEmpty()) return {};
    auto* rng = QRandomGenerator::global();
    Position position = seeds.at(rng->bounded(static_cast<int>(seeds.size())));
    const int edits = rng->bounded(100) < 50 ? 1 : rng->bounded(2, 5);
    for (int edit = 0; edit < edits; ++edit) {
        QList<int> occupied;
        QList<int> nonKings;
        int king = -1;
        for (int square = 0; square < 81; ++square) {
            if (position.board[square] == 0) continue;
            occupied.append(square);
            if (position.board[square] == -8) king = square; else nonKings.append(square);
        }
        if (king < 0) return {};
        const int operation = rng->bounded(100);
        if (operation < 50) {
            // Relocate a single piece; this is not a board translation or reflection.
            const int from = occupied.at(rng->bounded(static_cast<int>(occupied.size())));
            const int rank = from / 9 + rng->bounded(-2, 3);
            const int file = from % 9 + rng->bounded(-2, 3);
            if (rank < 0 || rank > 8 || file < 0 || file > 8 || position.board[rank * 9 + file] != 0) continue;
            position.board[rank * 9 + file] = position.board[from];
            position.board[from] = 0;
        } else if (operation < 70 && !nonKings.isEmpty()) {
            const int square = nonKings.at(rng->bounded(static_cast<int>(nonKings.size())));
            int type = rng->bounded(1, 8);
            if (type != 5 && rng->bounded(100) < 35) type += 10;
            position.board[square] = position.board[square] > 0 ? type : -type;
        } else if (operation < 85) {
            const int rank = king / 9 + rng->bounded(-2, 3);
            const int file = king % 9 + rng->bounded(-2, 3);
            if (rank < 0 || rank > 8 || file < 0 || file > 8 || position.board[rank * 9 + file] != 0) continue;
            int type = rng->bounded(1, 8);
            if (type != 5 && rng->bounded(100) < 30) type += 10;
            position.board[rank * 9 + file] = rng->bounded(100) < 65 ? type : -type;
        } else if (operation < 95) {
            const int type = rng->bounded(7);
            position.hand[type] += position.hand[type] > 0 && rng->bounded(2) == 0 ? -1 : 1;
        } else if (!nonKings.isEmpty()) {
            position.board[nonKings.at(rng->bounded(static_cast<int>(nonKings.size())))] = 0;
        }
    }
    return sfen(position, settings);
}
}
