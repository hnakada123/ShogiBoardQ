/// @file tsumeshogikanjibuilder.cpp
/// @brief 詰将棋の漢字PV構築ユーティリティの実装

#include "tsumeshogikanjibuilder.h"
#include "shogiboard.h"

namespace TsumeshogiKanjiBuilder {

QString pieceCharToKanji(QChar piece)
{
    switch (piece.toUpper().toLatin1()) {
    case 'P': return QStringLiteral("歩");
    case 'L': return QStringLiteral("香");
    case 'N': return QStringLiteral("桂");
    case 'S': return QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return QStringLiteral("角");
    case 'R': return QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    case 'Q': return QStringLiteral("と");
    case 'M': return QStringLiteral("成香");
    case 'O': return QStringLiteral("成桂");
    case 'T': return QStringLiteral("成銀");
    case 'C': return QStringLiteral("馬");
    case 'U': return QStringLiteral("龍");
    default:  return {};
    }
}

QString buildKanjiPv(const QString& baseSfen, const QStringList& pvMoves)
{
    static const QChar kFullWidthDigits[] = {
        u'０', u'１', u'２', u'３', u'４',
        u'５', u'６', u'７', u'８', u'９'
    };
    static const QString kKanjiRanks[] = {
        QStringLiteral("〇"), QStringLiteral("一"), QStringLiteral("二"),
        QStringLiteral("三"), QStringLiteral("四"), QStringLiteral("五"),
        QStringLiteral("六"), QStringLiteral("七"), QStringLiteral("八"),
        QStringLiteral("九")
    };
    ShogiBoard board;
    board.setSfen(baseSfen);
    bool blackToMove = !baseSfen.contains(QStringLiteral(" w "));

    QString result;

    for (const QString& usiMove : std::as_const(pvMoves)) {
        if (usiMove.length() < 4) continue;
        const QString turnMark = blackToMove ? QStringLiteral("▲") : QStringLiteral("△");

        if (usiMove.at(1) == QLatin1Char('*')) {
            // 駒打ち: "P*5e"
            const QChar pieceChar = usiMove.at(0);
            const int toFile = usiMove.at(2).toLatin1() - '0';
            const int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

            if (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9) {
                result += turnMark;
                result += kFullWidthDigits[toFile];
                result += kKanjiRanks[toRank];
                result += pieceCharToKanji(pieceChar);
                result += QStringLiteral("打");
            }

            // 盤面に反映
            const Piece boardPiece = charToPiece(blackToMove ? pieceChar.toUpper() : pieceChar.toLower());
            board.movePieceToSquare(boardPiece, 0, 0, toFile, toRank, false);
            if (board.m_pieceStand.contains(boardPiece) && board.m_pieceStand[boardPiece] > 0) {
                board.m_pieceStand[boardPiece]--;
            }
        } else {
            // 通常移動: "7g7f" or "7g7f+"
            const int fromFile = usiMove.at(0).toLatin1() - '0';
            const int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
            const int toFile = usiMove.at(2).toLatin1() - '0';
            const int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
            const bool promote = (usiMove.length() >= 5 && usiMove.at(4) == QLatin1Char('+'));

            const Piece piece = board.getPieceCharacter(fromFile, fromRank);

            if (fromFile >= 1 && fromFile <= 9 && fromRank >= 1 && fromRank <= 9 &&
                toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9) {
                result += turnMark;
                result += kFullWidthDigits[toFile];
                result += kKanjiRanks[toRank];
                result += pieceCharToKanji(pieceToChar(piece));
                if (promote) {
                    result += QStringLiteral("成");
                }
                result += QLatin1Char('(');
                result += QString::number(fromFile);
                result += QString::number(fromRank);
                result += QLatin1Char(')');
            }

            // 駒取りの処理
            const Piece captured = board.getPieceCharacter(toFile, toRank);
            if (captured != Piece::None) {
                Piece standPiece = demote(captured);
                standPiece = isBlackPiece(standPiece) ? toWhite(standPiece) : toBlack(standPiece);
                board.m_pieceStand[standPiece]++;
            }

            // 移動を盤面に反映
            board.movePieceToSquare(piece, fromFile, fromRank, toFile, toRank, promote);
        }

        blackToMove = !blackToMove;
    }

    return result;
}

} // namespace TsumeshogiKanjiBuilder
