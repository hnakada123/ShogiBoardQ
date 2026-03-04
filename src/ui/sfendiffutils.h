#ifndef SFENDIFFUTILS_H
#define SFENDIFFUTILS_H

/// @file sfendiffutils.h
/// @brief SFEN 盤面差分の共通ユーティリティ

#include <QChar>
#include <QPoint>
#include <QString>
#include <QStringList>

namespace SfenDiffUtils {

struct DiffResult
{
    QPoint from = QPoint(-1, -1);
    QPoint to = QPoint(-1, -1);
    QChar droppedPiece;
    int emptyCount = 0;
    int filledCount = 0;
    int changedCount = 0;

    [[nodiscard]] bool hasFrom() const
    {
        return from.x() >= 0 && from.y() >= 0;
    }

    [[nodiscard]] bool hasTo() const
    {
        return to.x() >= 0 && to.y() >= 0;
    }

    [[nodiscard]] bool isSingleMovePattern() const
    {
        // 通常移動 / 駒取り / 駒打ちのいずれか
        return (emptyCount == 1 && changedCount == 0 && filledCount == 1) ||
               (emptyCount == 1 && changedCount == 1 && filledCount == 0) ||
               (emptyCount == 0 && changedCount == 0 && filledCount == 1);
    }
};

inline bool parseBoardGrid(const QString& sfen, QString grid[9][9])
{
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            grid[y][x].clear();
        }
    }

    if (sfen.isEmpty()) return false;
    const QString boardField = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts).value(0);
    const QStringList rows = boardField.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    if (rows.size() != 9) return false;

    for (int r = 0; r < 9; ++r) {
        const QString& row = rows.at(r);
        const int y = r;
        int x = 8;

        for (qsizetype i = 0; i < row.size(); ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                x -= (ch.toLatin1() - '0');
            } else if (ch == QLatin1Char('+')) {
                if (i + 1 >= row.size() || x < 0) return false;
                grid[y][x] = QStringLiteral("+") + row.at(++i);
                --x;
            } else {
                if (x < 0) return false;
                grid[y][x] = QString(ch);
                --x;
            }
        }
        if (x != -1) return false;
    }
    return true;
}

inline bool diffBoards(const QString& prevSfen, const QString& currSfen, DiffResult& out)
{
    out = DiffResult{};

    QString prevGrid[9][9];
    QString currGrid[9][9];
    if (!parseBoardGrid(prevSfen, prevGrid) || !parseBoardGrid(currSfen, currGrid)) {
        return false;
    }

    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            if (prevGrid[y][x] == currGrid[y][x]) continue;

            if (!prevGrid[y][x].isEmpty() && currGrid[y][x].isEmpty()) {
                ++out.emptyCount;
                out.from = QPoint(x, y);
                continue;
            }

            if (prevGrid[y][x].isEmpty() && !currGrid[y][x].isEmpty()) {
                ++out.filledCount;
                out.to = QPoint(x, y);
                out.droppedPiece = currGrid[y][x].at(0);
                if (out.droppedPiece == QLatin1Char('+') && currGrid[y][x].size() >= 2) {
                    out.droppedPiece = currGrid[y][x].at(1);
                }
                continue;
            }

            ++out.changedCount;
            out.to = QPoint(x, y);
        }
    }

    return out.hasTo();
}

} // namespace SfenDiffUtils

#endif // SFENDIFFUTILS_H
