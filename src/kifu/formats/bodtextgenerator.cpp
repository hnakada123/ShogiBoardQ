/// @file bodtextgenerator.cpp
/// @brief BOD形式テキスト生成クラスの実装

#include "bodtextgenerator.h"
#include "piecerules.h"
#include "shoginotation.h"
#include "shogiposition.h"

#include <QStringList>

QString BodTextGenerator::generate(const QString& sfenStr, int moveIndex, const QString& lastMoveStr,
                                   bool handicapNames)
{
    if (sfenStr.isEmpty()) return QString();
    const std::optional<Position> position = Position::fromSfenLike(sfenStr);
    if (!position) return QString();

    // 持ち駒を日本語文字列にする（飛・角・金・銀・桂・香・歩の順。2枚以上は漢数字を添える）
    auto handToString = [&position](Turn side) -> QString {
        QString result;
        for (const PieceKind kind : PieceRules::kHandOrder) {
            const Piece piece = makePiece(side, kind);
            const int count = position->handCount(piece);
            if (count <= 0) continue;
            result += ShogiNotation::movePieceName(toBlack(piece));
            if (count > 1) result += ShogiNotation::kanjiNumber(count);
            result += QStringLiteral("　");
        }
        return result.isEmpty() ? QStringLiteral("なし") : result.trimmed();
    };

    // 盤の各段（9筋から1筋）。後手の駒は "v"、局面図の駒は1文字（成香は杏、成桂は圭、成銀は全）
    auto rankText = [&position](int rank) {
        QString line;
        for (int file = 9; file >= 1; --file) {
            const Piece piece = position->pieceAt(file, rank);
            if (piece == Piece::None) {
                line += QStringLiteral(" ・");
                continue;
            }
            line += isBlackPiece(piece) ? QStringLiteral(" ") : QStringLiteral("v");
            line += ShogiNotation::boardPieceName(toBlack(piece));
        }
        return line;
    };

    // BOD形式の文字列を生成
    // 柿木形式の駒落ちの棋譜と同じく、駒落ちでは下手（先手側）・上手（後手側）と書く
    const QString blackName = handicapNames ? QStringLiteral("下手") : QStringLiteral("先手");
    const QString whiteName = handicapNames ? QStringLiteral("上手") : QStringLiteral("後手");
    QStringList bodLines;
    bodLines << QStringLiteral("%1の持駒：%2").arg(whiteName, handToString(Turn::White));
    bodLines << QStringLiteral("  ９ ８ ７ ６ ５ ４ ３ ２ １");
    bodLines << QStringLiteral("+---------------------------+");

    for (int rank = 0; rank < 9; ++rank) {
        const QString line = QStringLiteral("|") + rankText(rank + 1) + QStringLiteral("|")
                             + ShogiNotation::rankKanji(rank + 1);
        bodLines << line;
    }

    bodLines << QStringLiteral("+---------------------------+");
    bodLines << QStringLiteral("%1の持駒：%2").arg(blackName, handToString(Turn::Black));
    bodLines << (position->sideToMove() == Turn::Black ? blackName : whiteName) + QStringLiteral("番");

    if (moveIndex > 0) {
        const int displayMoveNum = position->moveNumber() - 1;
        if (!lastMoveStr.isEmpty()) {
            bodLines << QStringLiteral("手数＝%1  %2  まで").arg(displayMoveNum).arg(lastMoveStr);
        } else {
            bodLines << QStringLiteral("手数＝%1  まで").arg(displayMoveNum);
        }
    }

    return bodLines.join(QStringLiteral("\n"));
}
