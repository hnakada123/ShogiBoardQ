/// @file jkfmoveparser.cpp
/// @brief JKF形式固有の指し手変換・初期局面構築・時間解析の実装

#include "jkfmoveparser.h"
#include "handicaps.h"
#include "kifmovetext.h"
#include "shoginotation.h"
#include "notationutils.h"
#include "parsecommon.h"
#include "sfenutils.h"

namespace {

// JKF の relative の記号と KI2 の修飾語（並びは修飾語にするときの順）
struct RelativeEntry { char letter; char16_t modifier; };
constexpr RelativeEntry kRelatives[] = {
    {'L', u'左'}, {'C', u'直'}, {'R', u'右'}, {'U', u'上'}, {'M', u'寄'}, {'D', u'引'}, {'H', u'打'},
};

} // namespace

namespace JkfMoveParser {

// --- プリセット・初期局面 ---

QString presetToSfen(const QString& preset)
{
    const Handicaps::Handicap* handicap = Handicaps::byJkfPreset(preset);
    return handicap ? QString::fromLatin1(handicap->sfen) : SfenUtils::hirateSfen();
}

QString presetFromSfen(const QString& sfen)
{
    if (sfen.trimmed().isEmpty()) return QStringLiteral("HIRATE");
    const Handicaps::Handicap* handicap = Handicaps::bySfen(sfen);
    return handicap ? QString::fromLatin1(handicap->jkfPreset) : QString();
}

QString buildSfenFromInitialData(const QJsonObject& data, int moveNumber)
{
    const int color = data[QStringLiteral("color")].toInt(-1);
    const QJsonArray boardData = data[QStringLiteral("board")].toArray();
    const QJsonArray handsData = data[QStringLiteral("hands")].toArray();
    if ((color != 0 && color != 1) || boardData.size() != 9 || handsData.size() != 2) return {};
    for (const auto& column : boardData) {
        if (!column.isArray()) return {};
        const QJsonArray cells = column.toArray();
        if (cells.size() != 9) return {};
        for (const auto& value : cells) {
            if (!value.isObject()) return {};
            const auto cell = value.toObject();
            if (cell.isEmpty()) continue;
            const int side = cell[QStringLiteral("color")].toInt(-1);
            if ((side != 0 && side != 1)
                || ShogiNotation::pieceFromCsaCode(cell[QStringLiteral("kind")].toString()) == Piece::None) return {};
        }
    }
    for (const auto& value : handsData) {
        if (!value.isObject()) return {};
        const auto hand = value.toObject();
        for (auto it = hand.begin(); it != hand.end(); ++it) {
            // 持ち駒は玉以外の生駒
            const Piece piece = ShogiNotation::pieceFromCsaCode(it.key());
            if (piece == Piece::None || isPromoted(piece) || piece == Piece::BlackKing || it.value().toInt(-1) < 0)
                return {};
        }
    }

    QString boardStr;
    if (data.contains(QStringLiteral("board"))) {
        const QJsonArray board = data[QStringLiteral("board")].toArray();

        for (int y = 0; y < 9; ++y) {
            int emptyCount = 0;
            QString rowStr;
            for (int x = 8; x >= 0; --x) {
                const QJsonArray col = board[x].toArray();
                const QJsonObject cell = col[y].toObject();

                if (cell.isEmpty() || !cell.contains(QStringLiteral("kind"))) {
                    ++emptyCount;
                } else {
                    if (emptyCount > 0) {
                        rowStr += QString::number(emptyCount);
                        emptyCount = 0;
                    }
                    const Turn owner = cell[QStringLiteral("color")].toInt() == 0 ? Turn::Black : Turn::White;
                    const Piece piece = ShogiNotation::pieceFromCsaCode(cell[QStringLiteral("kind")].toString(), owner);
                    if (isPromoted(piece)) rowStr += QLatin1Char('+');
                    rowStr += pieceToChar(demote(piece));
                }
            }
            if (emptyCount > 0) {
                rowStr += QString::number(emptyCount);
            }
            if (!boardStr.isEmpty()) boardStr += QLatin1Char('/');
            boardStr += rowStr;
        }
    }

    QString handsStr;
    if (data.contains(QStringLiteral("hands"))) {
        const QJsonArray hands = data[QStringLiteral("hands")].toArray();
        if (hands.size() >= 2) {
            const QJsonObject blackHands = hands[0].toObject();
            const QJsonObject whiteHands = hands[1].toObject();

            auto emitHands = [](const QJsonObject& h, bool isBlack) -> QString {
                QString s;
                const char* order[] = {"HI", "KA", "KI", "GI", "KE", "KY", "FU"};
                const char* sfenChar[] = {"R", "B", "G", "S", "N", "L", "P"};
                for (int i = 0; i < 7; ++i) {
                    const QString key = QString::fromUtf8(order[i]);
                    if (h.contains(key)) {
                        int count = h[key].toInt();
                        if (count > 0) {
                            if (count > 1) s += QString::number(count);
                            QString ch = QString::fromUtf8(sfenChar[i]);
                            if (!isBlack) ch = ch.toLower();
                            s += ch;
                        }
                    }
                }
                return s;
            };

            handsStr = emitHands(blackHands, true) + emitHands(whiteHands, false);
        }
    }
    if (handsStr.isEmpty()) handsStr = QStringLiteral("-");

    const QString turnStr = (color == 0) ? QStringLiteral("b") : QStringLiteral("w");

    return QStringLiteral("%1 %2 %3 %4")
        .arg(boardStr, turnStr, handsStr, QString::number(moveNumber));
}

// --- 指し手変換 ---

QString convertMoveToUsi(const QJsonObject& move, int& prevToX, int& prevToY)
{
    if (!move.contains(QStringLiteral("to"))) {
        return QString();
    }

    const QJsonObject to = move[QStringLiteral("to")].toObject();
    const int toX = to[QStringLiteral("x")].toInt();
    const int toY = to[QStringLiteral("y")].toInt();
    if (toX < 1 || toX > 9 || toY < 1 || toY > 9
        || ShogiNotation::pieceFromCsaCode(move[QStringLiteral("piece")].toString()) == Piece::None) return {};

    QString usi;

    if (!move.contains(QStringLiteral("from"))) {
        // 打てるのは生駒（玉以外）だけ
        const Piece piece = ShogiNotation::pieceFromCsaCode(move[QStringLiteral("piece")].toString());
        usi = NotationUtils::formatSfenDrop(pieceToChar(piece), toX, toY);
        if (usi.isEmpty()) return QString();
    } else {
        const QJsonObject from = move[QStringLiteral("from")].toObject();
        const int fromX = from[QStringLiteral("x")].toInt();
        const int fromY = from[QStringLiteral("y")].toInt();
        if (fromX < 1 || fromX > 9 || fromY < 1 || fromY > 9) return {};

        const bool promotes = move.contains(QStringLiteral("promote")) && move[QStringLiteral("promote")].toBool();
        usi = NotationUtils::formatSfenMove(fromX, fromY, toX, toY, promotes);
    }

    prevToX = toX;
    prevToY = toY;

    return usi;
}

QString convertMoveToPretty(const QJsonObject& move, Turn mover, const UsiMove& usi, BoardSquare previousTo)
{
    KifMoveText::Notes notes;
    notes.modifier = relativeToModifier(move[QStringLiteral("relative")].toString());
    const QJsonValue promote = move[QStringLiteral("promote")];
    notes.declinedPromotion = promote.isBool() && !promote.toBool();
    const Piece moving = ShogiNotation::pieceFromCsaCode(move[QStringLiteral("piece")].toString());
    return KifMoveText::format(mover, moving, usi, previousTo, notes);
}

// --- 時間解析 ---

QString formatTimeText(const QJsonObject& timeObj, qint64& cumSec)
{
    qint64 nowSec = 0;
    if (timeObj.contains(QStringLiteral("now"))) {
        const QJsonObject now = timeObj[QStringLiteral("now")].toObject();
        int h = now[QStringLiteral("h")].toInt(0);
        int m = now[QStringLiteral("m")].toInt(0);
        int s = now[QStringLiteral("s")].toInt(0);
        nowSec = h * 3600 + m * 60 + s;
    }

    qint64 totalSec = 0;
    if (timeObj.contains(QStringLiteral("total"))) {
        const QJsonObject total = timeObj[QStringLiteral("total")].toObject();
        int h = total[QStringLiteral("h")].toInt(0);
        int m = total[QStringLiteral("m")].toInt(0);
        int s = total[QStringLiteral("s")].toInt(0);
        totalSec = h * 3600 + m * 60 + s;
        cumSec = totalSec;
    } else {
        cumSec += nowSec;
        totalSec = cumSec;
    }

    return KifuParseCommon::formatTimeText(nowSec * 1000, totalSec * 1000);
}

// --- コメント・修飾語 ---

QString extractCommentsFromMoveObj(const QJsonObject& moveObj)
{
    if (!moveObj.contains(QStringLiteral("comments"))) {
        return QString();
    }
    const QJsonArray comments = moveObj[QStringLiteral("comments")].toArray();
    QStringList cmtList;
    for (const QJsonValueConstRef c : comments) {
        cmtList.append(c.toString());
    }
    return cmtList.join(QLatin1Char('\n'));
}

QString relativeToModifier(const QString& relative)
{
    QString mod;
    for (const auto& entry : kRelatives) {
        if (relative.contains(QLatin1Char(entry.letter))) mod += QChar(entry.modifier);
    }
    return mod;
}

QString modifierToRelative(QStringView modifiers)
{
    QString relative;
    for (const QChar ch : modifiers) {
        for (const auto& entry : kRelatives) {
            if (ch == QChar(entry.modifier)) relative += QLatin1Char(entry.letter);
        }
    }
    return relative;
}



} // namespace JkfMoveParser
