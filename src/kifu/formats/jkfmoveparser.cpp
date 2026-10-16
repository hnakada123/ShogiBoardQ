/// @file jkfmoveparser.cpp
/// @brief JKF形式固有の指し手変換・初期局面構築・時間解析の実装

#include "jkfmoveparser.h"
#include "handicaps.h"
#include "kifmovetext.h"
#include "shoginotation.h"
#include "sfenutils.h"
#include "shogiposition.h"

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

    // 盤は board[筋 - 1][段 - 1]
    Position position;
    for (int x = 0; x < 9; ++x) {
        const QJsonArray column = boardData[x].toArray();
        for (int y = 0; y < 9; ++y) {
            const QJsonObject cell = column[y].toObject();
            if (cell.isEmpty() || !cell.contains(QStringLiteral("kind"))) continue;
            const Turn owner = cell[QStringLiteral("color")].toInt() == 0 ? Turn::Black : Turn::White;
            position.setPieceAt({x + 1, y + 1},
                                ShogiNotation::pieceFromCsaCode(cell[QStringLiteral("kind")].toString(), owner));
        }
    }
    // 持ち駒は hands[0] が先手、hands[1] が後手
    for (int side = 0; side < 2; ++side) {
        const QJsonObject hand = handsData[side].toObject();
        for (auto it = hand.begin(); it != hand.end(); ++it) {
            const Piece piece = ShogiNotation::pieceFromCsaCode(it.key(), side == 0 ? Turn::Black : Turn::White);
            position.addToHand(piece, it.value().toInt());
        }
    }
    position.setSideToMove(color == 0 ? Turn::Black : Turn::White);
    position.setMoveNumber(moveNumber);
    return position.toSfen();
}

// --- 指し手変換 ---

std::optional<UsiMove> toUsiMove(const QJsonObject& move)
{
    if (!move.contains(QStringLiteral("to"))) return std::nullopt;
    const QJsonObject to = move[QStringLiteral("to")].toObject();
    UsiMove usi;
    usi.to = BoardSquare{to[QStringLiteral("x")].toInt(), to[QStringLiteral("y")].toInt()};
    const Piece piece = ShogiNotation::pieceFromCsaCode(move[QStringLiteral("piece")].toString());
    if (!usi.to.isValid() || piece == Piece::None) return std::nullopt;

    if (!move.contains(QStringLiteral("from"))) {
        // 打てるのは生駒（玉以外）だけ
        if (isPromoted(piece) || piece == Piece::BlackKing) return std::nullopt;
        usi.drop = piece;
        return usi;
    }
    const QJsonObject from = move[QStringLiteral("from")].toObject();
    usi.from = BoardSquare{from[QStringLiteral("x")].toInt(), from[QStringLiteral("y")].toInt()};
    if (!usi.from.isValid()) return std::nullopt;
    usi.promote = move[QStringLiteral("promote")].toBool(false);
    return usi;
}

KifMoveText::Notes notesOf(const QJsonObject& move)
{
    KifMoveText::Notes notes;
    notes.modifier = relativeToModifier(move[QStringLiteral("relative")].toString());
    const QJsonValue promote = move[QStringLiteral("promote")];
    notes.declinedPromotion = promote.isBool() && !promote.toBool();
    return notes;
}

// --- 時間解析 ---

Time timeOf(const QJsonObject& timeObj)
{
    const auto seconds = [](const QJsonObject& hms) {
        return qint64(hms[QStringLiteral("h")].toInt(0)) * 3600 + hms[QStringLiteral("m")].toInt(0) * 60
            + hms[QStringLiteral("s")].toInt(0);
    };
    Time time;
    if (timeObj.contains(QStringLiteral("now"))) time.elapsedMs = seconds(timeObj[QStringLiteral("now")].toObject()) * 1000;
    if (timeObj.contains(QStringLiteral("total")))
        time.totalMs = seconds(timeObj[QStringLiteral("total")].toObject()) * 1000;
    return time;
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
