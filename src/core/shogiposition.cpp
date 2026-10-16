/// @file shogiposition.cpp
/// @brief 局面の値（SFEN の読み書き・指し手の適用）の実装

#include "shogiposition.h"

#include <QCoreApplication>

#include "piecerules.h"
#include "sfenutils.h"

namespace {

QString translated(const char* text)
{
    // 以前は ShogiBoard が出していた文言なので、同じ文脈の訳を使う
    return QCoreApplication::translate("ShogiBoard", text);
}

/// 盤の欄の1駒（"+P" などの成駒を含む）。駒でない文字は None
Piece readBoardPiece(QStringView field, qsizetype& pos)
{
    const QChar ch = field.at(pos++);
    if (ch != QLatin1Char('+')) return charToPiece(ch);
    if (pos >= field.size()) return Piece::None;
    // 成れる駒（歩・香・桂・銀・角・飛）だけに "+" を付けられる
    const Piece base = charToPiece(field.at(pos++));
    return isPromotable(base) ? promote(base) : Piece::None;
}

} // namespace

Position::Position()
{
    m_squares.fill(Piece::None);
}

Position Position::hirate()
{
    return *fromSfen(SfenUtils::hirateSfen());
}

// ============================================================
// SFEN の読み込み
// ============================================================

std::optional<Position> Position::fromSfen(QStringView sfen, QString* error)
{
    const auto fail = [&](const QString& message) -> std::optional<Position> {
        if (error) *error = message;
        return std::nullopt;
    };

    // 4つの欄（盤・手番・持ち駒・手数）
    std::array<QStringView, 4> fields;
    qsizetype count = 0;
    qsizetype start = 0;
    while (true) {
        const qsizetype space = sfen.indexOf(QLatin1Char(' '), start);
        const qsizetype end = space < 0 ? sfen.size() : space;
        if (count < 4) fields[static_cast<size_t>(count)] = sfen.sliced(start, end - start);
        ++count;
        if (space < 0) break;
        start = space + 1;
    }
    const QString shown = sfen.left(80).toString();
    if (count != 4) return fail(translated("SFEN board parse error: %1").arg(shown));
    if (fields[1] != u"b" && fields[1] != u"w") return fail(translated("SFEN board parse error: %1").arg(shown));
    bool ok = false;
    const int moveNumber = fields[3].toInt(&ok);
    if (!ok || moveNumber <= 0) return fail(translated("SFEN board parse error: %1").arg(shown));

    Position position;
    position.m_sideToMove = fields[1] == u"b" ? Turn::Black : Turn::White;
    position.m_moveNumber = moveNumber;

    // 盤（9段を "/" で区切り、各段は9筋から1筋へ。数字は空きマスの数）
    const QStringView board = fields[0];
    qsizetype pos = 0;
    for (int rank = 1; rank <= kRanks; ++rank) {
        if (rank > 1) {
            if (pos >= board.size() || board.at(pos) != QLatin1Char('/'))
                return fail(translated("SFEN board parse error: %1").arg(shown));
            ++pos;
        }
        int file = kFiles;
        while (pos < board.size() && board.at(pos) != QLatin1Char('/')) {
            const char16_t ch = board.at(pos).unicode();
            if (ch >= u'1' && ch <= u'9') {
                const int empty = ch - u'0';
                if (empty > file) return fail(translated("SFEN board parse error: %1").arg(shown));
                file -= empty;
                ++pos;
                continue;
            }
            const Piece piece = readBoardPiece(board, pos);
            if (piece == Piece::None || file < 1 || PieceRules::isDeadSquare(piece, rank))
                return fail(translated("SFEN board parse error: %1").arg(shown));
            position.setPieceAt({file, rank}, piece);
            --file;
        }
        if (file != 0) return fail(translated("SFEN board parse error: %1").arg(shown));
    }
    if (pos != board.size()) return fail(translated("SFEN board parse error: %1").arg(shown));

    // 持ち駒（"-" はなし。駒の前の1〜2桁の数字は枚数で、同じ駒が何度出ても足す）
    const QStringView hands = fields[2];
    if (hands != u"-") {
        for (qsizetype i = 0; i < hands.size();) {
            int number = 1;
            if (hands.at(i).isDigit()) {
                number = hands.at(i++).digitValue();
                if (i < hands.size() && hands.at(i).isDigit()) number = number * 10 + hands.at(i++).digitValue();
            }
            const Piece piece = i < hands.size() ? charToPiece(hands.at(i)) : Piece::None;
            if (piece == Piece::None) return fail(translated("SFEN piece stand parse error: %1").arg(shown));
            position.addToHand(piece, number);
            ++i;
        }
    }
    return position;
}

bool Position::assignSfen(QStringView sfen, QString* error)
{
    std::optional<Position> parsed = fromSfen(sfen, error);
    if (!parsed) return false;
    *this = *parsed;
    return true;
}

std::optional<Position> Position::fromSfenLike(QStringView text, QString* error)
{
    QStringView rest = text.trimmed();
    if (rest.startsWith(u"position ")) rest = rest.sliced(9).trimmed();
    QStringView movesPart;
    if (const qsizetype moves = rest.indexOf(u" moves"); moves >= 0) {
        movesPart = rest.sliced(moves + 6).trimmed();
        rest = rest.left(moves).trimmed();
    } else if (rest.endsWith(u"moves")) {
        rest = rest.chopped(5).trimmed();
    }
    if (rest.startsWith(u"sfen ")) rest = rest.sliced(5).trimmed();

    std::optional<Position> position;
    if (rest == u"startpos") {
        position = hirate();
    } else if (rest.count(QLatin1Char(' ')) == 2) {
        position = fromSfen(rest.toString() + QStringLiteral(" 1"), error);
    } else {
        position = fromSfen(rest, error);
    }
    if (!position) return std::nullopt;

    for (const QStringView token : movesPart.tokenize(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const auto move = UsiMove::parse(token);
        if (!move || !position->apply(*move, DropRule::Lenient)) {
            if (error) *error = QStringLiteral("Move cannot be applied: %1").arg(token);
            return std::nullopt;
        }
    }
    return position;
}

// ============================================================
// SFEN の書き出し
// ============================================================

QString Position::boardSfen() const
{
    QString text;
    text.reserve(96);
    for (int rank = 1; rank <= kRanks; ++rank) {
        if (rank > 1) text += QLatin1Char('/');
        int empty = 0;
        for (int file = kFiles; file >= 1; --file) {
            const Piece piece = pieceAt(file, rank);
            if (piece == Piece::None) {
                ++empty;
                continue;
            }
            if (empty > 0) text += QChar(u'0' + empty);
            empty = 0;
            text += pieceSfen(piece);
        }
        if (empty > 0) text += QChar(u'0' + empty);
    }
    return text;
}

QString Position::handsSfen() const
{
    QString text;
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (const PieceKind kind : PieceRules::kHandOrder) {
            const Piece piece = makePiece(side, kind);
            const int count = handCount(piece);
            if (count <= 0) continue;
            if (count > 1) text += QString::number(count);
            text += pieceLetter(piece);
        }
    }
    return text.isEmpty() ? QStringLiteral("-") : text;
}

QString Position::toSfen() const
{
    return positionKey() + QLatin1Char(' ') + QString::number(m_moveNumber);
}

QString Position::toSfen(Turn side, int moveNumber) const
{
    Position copy = *this;
    copy.setSideToMove(side);
    copy.setMoveNumber(moveNumber);
    return copy.toSfen();
}

QString Position::positionKey() const
{
    return boardSfen() + (m_sideToMove == Turn::Black ? QStringLiteral(" b ") : QStringLiteral(" w ")) + handsSfen();
}

// ============================================================
// 盤と持ち駒
// ============================================================

Piece Position::pieceAt(BoardSquare square) const
{
    const int index = indexOf(square);
    return index < 0 ? Piece::None : m_squares[static_cast<size_t>(index)];
}

void Position::setPieceAt(BoardSquare square, Piece piece)
{
    const int index = indexOf(square);
    if (index >= 0) m_squares[static_cast<size_t>(index)] = piece;
}

int Position::countOnBoard(Piece piece) const
{
    int count = 0;
    for (const Piece square : m_squares) count += square == piece ? 1 : 0;
    return count;
}

BoardSquare Position::kingSquare(Turn side) const
{
    const Piece king = makePiece(side, PieceKind::King);
    for (int i = 0; i < kSquareCount; ++i) {
        if (m_squares[static_cast<size_t>(i)] == king) return BoardSquare{i % kFiles + 1, i / kFiles + 1};
    }
    return {};
}

int Position::handSlot(Piece piece)
{
    if (piece == Piece::None) return -1;
    const Piece base = demote(piece);
    return (isWhitePiece(base) ? kHandKindCount : 0) + static_cast<int>(pieceKind(base));
}

int Position::handCount(Piece piece) const
{
    const int slot = handSlot(piece);
    return slot < 0 ? 0 : m_hands[static_cast<size_t>(slot)];
}

void Position::setHandCount(Piece piece, int count)
{
    const int slot = handSlot(piece);
    if (slot >= 0) m_hands[static_cast<size_t>(slot)] = count < 0 ? 0 : count;
}

void Position::addToHand(Piece piece, int count)
{
    const int slot = handSlot(piece);
    if (slot >= 0 && count > 0) m_hands[static_cast<size_t>(slot)] += count;
}

bool Position::takeFromHand(Piece piece)
{
    const int slot = handSlot(piece);
    if (slot < 0 || m_hands[static_cast<size_t>(slot)] <= 0) return false;
    --m_hands[static_cast<size_t>(slot)];
    return true;
}

bool Position::handsEmpty() const
{
    for (const int count : m_hands) {
        if (count > 0) return false;
    }
    return true;
}

QMap<Piece, int> Position::pieceBox() const
{
    QMap<Piece, int> counts = {
        {Piece::BlackPawn, 18}, {Piece::BlackLance, 4}, {Piece::BlackKnight, 4},
        {Piece::BlackSilver, 4}, {Piece::BlackGold, 4}, {Piece::BlackBishop, 2},
        {Piece::BlackRook, 2}, {Piece::BlackKing, 2},
    };
    // SFENに駒箱は含めない。局面から求めるので、編集の再開・棋譜読込でも失われない。
    for (const Piece piece : m_squares) {
        if (piece != Piece::None) --counts[toBlack(demote(piece))];
    }
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (int kind = 0; kind < kHandKindCount; ++kind) {
            const Piece piece = makePiece(side, static_cast<PieceKind>(kind));
            counts[toBlack(piece)] -= handCount(piece);
        }
    }
    for (auto it = counts.begin(); it != counts.end(); ++it) it.value() = qMax(0, it.value());
    return counts;
}

// ============================================================
// 指し手
// ============================================================

ShogiMove Position::describe(const UsiMove& move) const
{
    const Piece moving = move.isDrop() ? move.dropPieceFor(m_sideToMove) : pieceAt(move.from);
    return ShogiMove(move, moving, move.isDrop() ? Piece::None : pieceAt(move.to));
}

bool Position::apply(const UsiMove& move, DropRule rule)
{
    if (!move.isValid()) return false;
    const Turn mover = m_sideToMove;
    if (move.isDrop()) {
        const Piece piece = move.dropPieceFor(mover);
        if (rule == DropRule::RequireHand) {
            if (pieceAt(move.to) != Piece::None || !takeFromHand(piece)) return false;
        } else {
            takeFromHand(piece);
        }
        setPieceAt(move.to, piece);
    } else {
        Piece moving = pieceAt(move.from);
        if (moving == Piece::None) return false;
        // 取った駒は、取られた駒の相手の持ち駒になる（駒の色は確かめない。合法かは呼び出し側が確かめる）
        const Piece captured = pieceAt(move.to);
        if (captured != Piece::None) addToHand(capturedToHand(captured));
        if (move.promote) moving = promote(moving);
        // 行き所のない段へ動いた歩・香・桂は成る（局面編集・反則手を含む棋譜でも同じ）
        if (PieceRules::isDeadSquare(moving, move.to.rank)) moving = promote(moving);
        setPieceAt(move.from, Piece::None);
        setPieceAt(move.to, moving);
    }
    m_sideToMove = opponent(mover);
    ++m_moveNumber;
    return true;
}

std::optional<Position> Position::after(const UsiMove& move, DropRule rule) const
{
    Position next = *this;
    if (!next.apply(move, rule)) return std::nullopt;
    return next;
}
