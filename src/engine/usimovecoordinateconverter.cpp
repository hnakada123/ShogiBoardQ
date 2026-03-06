/// @file usimovecoordinateconverter.cpp
/// @brief USI座標変換ユーティリティの実装

#include "usimovecoordinateconverter.h"

#include "logcategories.h"

#include <QCoreApplication>

// ============================================================
// 静的駒変換マップ
// ============================================================

const QMap<int, QString>& UsiMoveCoordinateConverter::firstPlayerPieceMap()
{
    static const auto& m = *[]() {
        static const QMap<int, QString> map = {
            {1, "P*"}, {2, "L*"}, {3, "N*"}, {4, "S*"}, {5, "G*"}, {6, "B*"}, {7, "R*"}
        };
        return &map;
    }();
    return m;
}

const QMap<int, QString>& UsiMoveCoordinateConverter::secondPlayerPieceMap()
{
    static const auto& m = *[]() {
        static const QMap<int, QString> map = {
            {3, "R*"}, {4, "B*"}, {5, "G*"}, {6, "S*"}, {7, "N*"}, {8, "L*"}, {9, "P*"}
        };
        return &map;
    }();
    return m;
}

const QMap<QChar, int>& UsiMoveCoordinateConverter::pieceRankWhiteMap()
{
    static const auto& m = *[]() {
        static const QMap<QChar, int> map = {
            {'P', 9}, {'L', 8}, {'N', 7}, {'S', 6}, {'G', 5}, {'B', 4}, {'R', 3}
        };
        return &map;
    }();
    return m;
}

const QMap<QChar, int>& UsiMoveCoordinateConverter::pieceRankBlackMap()
{
    static const auto& m = *[]() {
        static const QMap<QChar, int> map = {
            {'P', 1}, {'L', 2}, {'N', 3}, {'S', 4}, {'G', 5}, {'B', 6}, {'R', 7}
        };
        return &map;
    }();
    return m;
}

// ============================================================
// 座標変換
// ============================================================

QChar UsiMoveCoordinateConverter::rankToAlphabet(int rank)
{
    if (rank < 1 || rank > 9) {
        qCWarning(lcEngine, "rankToAlphabet: rank %d out of range 1-9", rank);
        return QChar('?');
    }
    return QChar('a' + rank - 1);
}

std::optional<int> UsiMoveCoordinateConverter::alphabetToRank(QChar c)
{
    const char ch = c.toLatin1();
    if (ch < 'a' || ch > 'i')
        return std::nullopt;
    return ch - 'a' + 1;
}

// ============================================================
// 駒変換
// ============================================================

QString UsiMoveCoordinateConverter::convertFirstPlayerPieceSymbol(int rankFrom)
{
    return firstPlayerPieceMap().value(rankFrom, QString());
}

QString UsiMoveCoordinateConverter::convertSecondPlayerPieceSymbol(int rankFrom)
{
    return secondPlayerPieceMap().value(rankFrom, QString());
}

int UsiMoveCoordinateConverter::pieceToRankWhite(QChar c)
{
    return pieceRankWhiteMap().value(c, 0);
}

int UsiMoveCoordinateConverter::pieceToRankBlack(QChar c)
{
    return pieceRankBlackMap().value(c, 0);
}

// ============================================================
// 指し手パース
// ============================================================

std::optional<MoveCoord> UsiMoveCoordinateConverter::parseMoveFrom(const QString& move,
                                                                    bool isFirstPlayer)
{
    if (QStringLiteral("123456789").contains(move[0])) {
        const int fileFrom = move[0].digitValue();
        auto optRank = alphabetToRank(move[1]);
        if (!optRank || fileFrom < 1 || fileFrom > 9) {
            qCWarning(lcEngine,
                       "Invalid move coordinates in moveFrom: file=%d, rank=%d",
                       fileFrom, optRank.value_or(-1));
            return std::nullopt;
        }
        return MoveCoord{fileFrom, *optRank};
    }

    if (QStringLiteral("PLNSGBR").contains(move[0]) && move[1] == '*') {
        if (isFirstPlayer) {
            return MoveCoord{SENTE_HAND_FILE, pieceToRankBlack(move[0])};
        }
        return MoveCoord{GOTE_HAND_FILE, pieceToRankWhite(move[0])};
    }

    qCWarning(lcEngine, "Invalid move format in moveFrom: \"%s\"",
              qPrintable(move));
    return std::nullopt;
}

std::optional<MoveCoord> UsiMoveCoordinateConverter::parseMoveTo(const QString& move)
{
    if (!move[2].isDigit() || !move[3].isLetter()) {
        qCWarning(lcEngine, "Invalid move format in moveTo: \"%s\"",
                  qPrintable(move));
        return std::nullopt;
    }

    const int fileTo = move[2].digitValue();
    auto optRank = alphabetToRank(move[3]);
    if (!optRank || fileTo < 1 || fileTo > 9) {
        qCWarning(lcEngine,
                   "Invalid move coordinates in moveTo: file=%d, rank=%d",
                   fileTo, optRank.value_or(-1));
        return std::nullopt;
    }
    return MoveCoord{fileTo, *optRank};
}

// ============================================================
// 人間操作→USI変換
// ============================================================

QString UsiMoveCoordinateConverter::convertHumanMoveToUsi(const QPoint& from, const QPoint& to,
                                                          bool promote, QString& errorMsg)
{
    const int fileFrom = from.x();
    const int rankFrom = from.y();
    const int fileTo = to.x();
    const int rankTo = to.y();

    QString result;

    if (fileFrom >= 1 && fileFrom <= BOARD_SIZE) {
        result = QString::number(fileFrom) + rankToAlphabet(rankFrom);
        result += QString::number(fileTo) + rankToAlphabet(rankTo);
        if (promote) result += "+";
    } else if (fileFrom == SENTE_HAND_FILE) {
        result = convertFirstPlayerPieceSymbol(rankFrom);
        result += QString::number(fileTo) + rankToAlphabet(rankTo);
    } else if (fileFrom == GOTE_HAND_FILE) {
        result = convertSecondPlayerPieceSymbol(rankFrom);
        result += QString::number(fileTo) + rankToAlphabet(rankTo);
    } else {
        errorMsg = QCoreApplication::translate(
            "UsiProtocolHandler", "Invalid fileFrom value");
        return QString();
    }

    return result;
}
