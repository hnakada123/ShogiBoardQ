/// @file shogiboard_sfen.cpp
/// @brief SFEN文字列の入力・検証・出力・記録処理

#include "shogiboard.h"
#include "errorbus.h"
#include "logcategories.h"

#include <algorithm>
#include <array>

// ============================================================
// SFEN入力・検証
// ============================================================

namespace {

constexpr int kBoardSize = 9;
constexpr int kStandKinds = 16;

/// SFEN の4つの欄（盤・手番・持駒・手数）。盤と持駒は元の文字列を指す
struct SfenFields
{
    QStringView board;
    QStringView stand;
    Turn turn = Turn::Black;
    int moveNumber = 0;
};

/// SFEN を空白で4つの欄に分け、手番と手数を確かめる
std::optional<SfenFields> splitSfenFields(const QString& sfenStr)
{
    std::array<QStringView, 4> fields;
    qsizetype count = 0;
    qsizetype start = 0;
    while (true) {
        const qsizetype space = sfenStr.indexOf(QLatin1Char(' '), start);
        const qsizetype end = (space < 0) ? sfenStr.size() : space;
        if (count < 4) fields[static_cast<size_t>(count)] = QStringView(sfenStr).sliced(start, end - start);
        ++count;
        if (space < 0) break;
        start = space + 1;
    }

    if (count != 4) {
        qCWarning(lcCore, "SFEN components: %lld in %s", count, qUtf8Printable(sfenStr));
        return std::nullopt;
    }

    const QStringView turnStr = fields[1];
    if (turnStr != u"b" && turnStr != u"w") {
        qCWarning(lcCore, "Invalid turn: %s in %s",
                 qUtf8Printable(turnStr.toString()), qUtf8Printable(sfenStr));
        return std::nullopt;
    }

    bool ok = false;
    const int moveNumber = fields[3].toInt(&ok);
    if (!ok || moveNumber <= 0) {
        qCWarning(lcCore, "Invalid move number in SFEN: %s", qUtf8Printable(sfenStr));
        return std::nullopt;
    }

    return SfenFields{fields[0], fields[2], turnStr == u"b" ? Turn::Black : Turn::White, moveNumber};
}

/// 盤の欄の1駒（"+P" などの成駒を含む）。駒でない文字は None
Piece boardPieceAt(QStringView field, qsizetype& pos)
{
    const QChar ch = field.at(pos++);
    if (ch != QLatin1Char('+')) return charToPiece(ch);
    if (pos >= field.size()) return Piece::None;

    // 成れる駒（歩・香・桂・銀・角・飛）だけに "+" を付けられる
    const Piece base = charToPiece(field.at(pos++));
    const Piece promoted = promote(base);
    return (promoted != base) ? promoted : Piece::None;
}

/// 段に置ける駒か（行き所のない駒は置けない。先手の歩・香は1段目、桂は1・2段目、後手は逆）
bool canStandOnRank(Piece piece, int rank)
{
    switch (piece) {
    case Piece::BlackPawn:
    case Piece::BlackLance:  return rank >= 2;
    case Piece::BlackKnight: return rank >= 3;
    case Piece::WhitePawn:
    case Piece::WhiteLance:  return rank <= kBoardSize - 1;
    case Piece::WhiteKnight: return rank <= kBoardSize - 2;
    default:                 return true;
    }
}

/// SFEN の盤の欄を読む（9段を "/" で区切り、各段は9筋から1筋へ9マス。数字は空きマスの数）
bool readBoardField(QStringView field, std::array<Piece, kBoardSize * kBoardSize>& squares)
{
    qsizetype pos = 0;
    for (int rank = 1; rank <= kBoardSize; ++rank) {
        if (rank > 1) {
            if (pos >= field.size()) {
                qCWarning(lcCore, "SFEN parts != 9: %s", qUtf8Printable(field.toString()));
                return false;
            }
            ++pos;  // 段の区切りの "/"
        }

        int file = kBoardSize;
        while (pos < field.size() && field.at(pos) != QLatin1Char('/')) {
            const char16_t ch = field.at(pos).unicode();
            if (ch >= u'1' && ch <= u'9') {
                const int empty = ch - u'0';
                if (empty > file) {
                    qCWarning(lcCore, "SFEN rank piece count exceeds 9 at rank %d in %s", rank, qUtf8Printable(field.toString()));
                    return false;
                }
                for (int i = 0; i < empty; ++i) {
                    squares[static_cast<size_t>((rank - 1) * kBoardSize + (file - 1))] = Piece::None;
                    --file;
                }
                ++pos;
                continue;
            }

            const Piece piece = boardPieceAt(field, pos);
            if (piece == Piece::None || !canStandOnRank(piece, rank)) {
                qCWarning(lcCore, "Unexpected SFEN char at rank %d in %s", rank, qUtf8Printable(field.toString()));
                return false;
            }
            if (file < 1) {
                qCWarning(lcCore, "SFEN rank piece count exceeds 9 at rank %d in %s", rank, qUtf8Printable(field.toString()));
                return false;
            }
            squares[static_cast<size_t>((rank - 1) * kBoardSize + (file - 1))] = piece;
            --file;
        }

        if (file != 0) {
            qCWarning(lcCore, "SFEN rank piece count: %d at rank %d in %s",
                     kBoardSize - file, rank, qUtf8Printable(field.toString()));
            return false;
        }
    }

    if (pos != field.size()) {
        qCWarning(lcCore, "SFEN parts != 9: %s", qUtf8Printable(field.toString()));
        return false;
    }
    return true;
}

/// 駒台に置く駒（玉を含む先後16種）の番号。それ以外の駒は -1
int standSlot(Piece piece)
{
    switch (piece) {
    case Piece::BlackPawn:   return 0;
    case Piece::BlackLance:  return 1;
    case Piece::BlackKnight: return 2;
    case Piece::BlackSilver: return 3;
    case Piece::BlackGold:   return 4;
    case Piece::BlackBishop: return 5;
    case Piece::BlackRook:   return 6;
    case Piece::BlackKing:   return 7;
    case Piece::WhitePawn:   return 8;
    case Piece::WhiteLance:  return 9;
    case Piece::WhiteKnight: return 10;
    case Piece::WhiteSilver: return 11;
    case Piece::WhiteGold:   return 12;
    case Piece::WhiteBishop: return 13;
    case Piece::WhiteRook:   return 14;
    case Piece::WhiteKing:   return 15;
    default:                 return -1;
    }
}

/// SFEN の持駒の欄を読む（"-" は持駒なし。駒の前の1〜2桁の数字は枚数で、同じ駒が何度出ても足す）
bool readStandField(QStringView field, std::array<int, kStandKinds>& counts)
{
    counts.fill(0);
    if (field == u"-") return true;

    for (qsizetype pos = 0; pos < field.size();) {
        int count = 1;
        if (field.at(pos).isDigit()) {
            count = field.at(pos++).digitValue();
            if (pos < field.size() && field.at(pos).isDigit()) count = count * 10 + field.at(pos++).digitValue();
        }
        const int slot = (pos < field.size()) ? standSlot(charToPiece(field.at(pos))) : -1;
        if (slot < 0) {
            qCWarning(lcCore, "Invalid piece in stand string: %s", qUtf8Printable(field.toString()));
            return false;
        }
        counts[static_cast<size_t>(slot)] += count;
        ++pos;
    }
    return true;
}

} // namespace

// SFEN文字列をパースし、各要素に分解する（副作用なしの static メソッド）。
std::optional<SfenComponents> ShogiBoard::parseSfen(const QString& sfenStr)
{
    const auto fields = splitSfenFields(sfenStr);
    if (!fields) return std::nullopt;
    return SfenComponents{fields->board.toString(), fields->stand.toString(), fields->turn, fields->moveNumber};
}

// SFEN文字列で将棋盤全体（盤面＋駒台）を更新し再描画する。
void ShogiBoard::setSfen(const QString& sfenStr)
{
    QString error;
    if (!trySetSfen(sfenStr, &error) && !error.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, error);
    }
}

bool ShogiBoard::trySetSfen(const QString& sfenStr, QString* error)
{
    // 処理フロー:
    // 1. SFEN文字列を4つの欄に分け、手番と手数を確かめる
    // 2. 盤と持駒を作業用の配列に読む（不正なら盤面を変更せずに返す）
    // 3. 盤面・駒台・手番を書き換え、再描画シグナルを発行

    if (lcCore().isDebugEnabled()) {
        const QString preview = (sfenStr.size() > 200) ? sfenStr.left(200) + QStringLiteral(" ...") : sfenStr;
        qCDebug(lcCore, "setSfen: %s", qUtf8Printable(preview));
    }
    if (sfenStr.startsWith(QLatin1String("position "))) {
        qCWarning(lcCore, "NON-SFEN passed to setSfen (caller bug)");
    }

    const auto fields = splitSfenFields(sfenStr);
    if (!fields)
        return false;

    std::array<Piece, kBoardSize * kBoardSize> squares;
    if (!readBoardField(fields->board, squares)) {
        if (error) *error = tr("SFEN board parse error: %1").arg(sfenStr.left(80));
        return false;
    }
    std::array<int, kStandKinds> standCounts;
    if (!readStandField(fields->stand, standCounts)) {
        if (error) *error = tr("SFEN piece stand parse error: %1").arg(sfenStr.left(80));
        return false;
    }

    if (m_boardData.size() != static_cast<qsizetype>(squares.size())) {
        m_boardData.resize(static_cast<qsizetype>(squares.size()));
    }
    std::copy(squares.cbegin(), squares.cend(), m_boardData.begin());

    // 駒台は16種の駒の枚数。種類がそろっていれば値だけ書き換える（作り直さない）
    bool sameKinds = m_pieceStand.size() == kStandKinds;
    for (auto it = m_pieceStand.cbegin(); sameKinds && it != m_pieceStand.cend(); ++it) {
        sameKinds = standSlot(it.key()) >= 0;
    }
    if (!sameKinds) initStand();
    for (auto it = m_pieceStand.begin(); it != m_pieceStand.end(); ++it) {
        it.value() = standCounts[static_cast<size_t>(standSlot(it.key()))];
    }

    m_currentPlayer = fields->turn;
    m_currentMoveNumber = fields->moveNumber;

    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit boardReset();
    return true;
}

// ============================================================
// SFEN出力
// ============================================================

QString ShogiBoard::convertPieceToSfen(const Piece piece) const
{
    static const QMap<Piece, QString> sfenMap = {
        {Piece::BlackPromotedPawn, "+P"}, {Piece::BlackPromotedLance, "+L"},
        {Piece::BlackPromotedKnight, "+N"}, {Piece::BlackPromotedSilver, "+S"},
        {Piece::BlackHorse, "+B"}, {Piece::BlackDragon, "+R"},
        {Piece::WhitePromotedPawn, "+p"}, {Piece::WhitePromotedLance, "+l"},
        {Piece::WhitePromotedKnight, "+n"}, {Piece::WhitePromotedSilver, "+s"},
        {Piece::WhiteHorse, "+b"}, {Piece::WhiteDragon, "+r"}
    };

    return sfenMap.contains(piece) ? sfenMap.value(piece) : QString(pieceToChar(piece));
}

QString ShogiBoard::convertBoardToSfen()
{
    QString str;
    str.reserve(200);

    for (int rank = 1; rank <= ranks(); ++rank) {
        int spacecount = 0;
        for (int file = files(); file > 0; --file) {
            Piece piece = pieceCharacter(file, rank);

            if (piece == Piece::None) {
                spacecount++;

                if (spacecount == 9 || file == 1) {
                    str += QString::number(spacecount);
                }
            }
            else {
                if (spacecount > 0) {
                    str += QString::number(spacecount);
                    spacecount = 0;
                }

                str += convertPieceToSfen(piece);
            }
        }

        if (rank != 9) str += "/";
    }

    return str;
}

QString ShogiBoard::convertStandToSfen() const
{
    QString handPiece;
    handPiece.reserve(40);

    // SFEN（USI）の持ち駒は、先手の駒をすべて書いてから後手の駒を書く。どちらも飛・角・金・銀・桂・香・歩の順
    static const QList<Piece> keys = {
        Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
        Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn
    };
    for (const bool white : {false, true}) {
        for (const auto& blackKey : keys) {
            const Piece key = white ? toWhite(blackKey) : blackKey;
            const int value = m_pieceStand.value(key, 0);
            if (value > 0) {
                handPiece += (value > 1 ? QString::number(value) : QString()) + convertPieceToSfen(key);
            }
        }
    }

    return handPiece.isEmpty() ? "-" : handPiece;
}

// ============================================================
// SFEN記録
// ============================================================

void ShogiBoard::addSfenRecord(Turn nextTurn, int moveIndex, QStringList* sfenRecord)
{
    if (!sfenRecord) {
        qCWarning(lcCore, "addSfenRecord: sfenRecord is null");
        return;
    }

    // moveIndex: 0始まりの着手番号を想定
    //   通常記録     → +1 で手数フィールドへ
    //   特例(-1等)   → 1 を明示（開始直後など）
    const int moveCountField = (moveIndex < 0) ? 1 : (moveIndex + 1);

    const qsizetype before = sfenRecord->size();
    const QString boardSfen = convertBoardToSfen();
    QString stand = convertStandToSfen();
    if (stand.isEmpty()) stand = QStringLiteral("-");

    const QString nextTurnStr = turnToSfen(nextTurn);
    qCDebug(lcCore, "addSfenRecord BEFORE size= %lld nextTurn= %s moveIndex= %d => field= %d",
            before, qUtf8Printable(nextTurnStr), moveIndex, moveCountField);
    qCDebug(lcCore, "addSfenRecord board= %s stand= %s",
            qUtf8Printable(boardSfen), qUtf8Printable(stand));

    const QString sfen = QStringLiteral("%1 %2 %3 %4")
                             .arg(boardSfen, nextTurnStr, stand, QString::number(moveCountField));
    sfenRecord->append(sfen);

    qCDebug(lcCore, "addSfenRecord AFTER size= %lld appended= %s",
            sfenRecord->size(), qUtf8Printable(sfen));

    if (!sfenRecord->isEmpty()) {
        qCDebug(lcCore, "addSfenRecord head[0]= %s", qUtf8Printable(sfenRecord->first()));
        qCDebug(lcCore, "addSfenRecord tail[last]= %s", qUtf8Printable(sfenRecord->last()));
    }

    // 先頭が破壊されてないか簡易チェック
    if (!sfenRecord->isEmpty()) {
        const QStringList parts = sfenRecord->first().split(QLatin1Char(' '), Qt::KeepEmptyParts);
        if (parts.size() == 4) {
            const QString& turn0 = parts[1];
            const QString& move0 = parts[3];
            if (move0 != QLatin1String("1")) {
                qCWarning(lcCore, "addSfenRecord head[0] moveCount != 1  head= %s", qUtf8Printable(sfenRecord->first()));
            }
            if (turn0 != QLatin1String("b") && turn0 != QLatin1String("w")) {
                qCWarning(lcCore, "addSfenRecord head[0] turn invalid  head= %s", qUtf8Printable(sfenRecord->first()));
            }
        } else {
            qCWarning(lcCore, "addSfenRecord head[0] malformed  head= %s", qUtf8Printable(sfenRecord->first()));
        }
    }
}
