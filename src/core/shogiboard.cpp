/// @file shogiboard.cpp
/// @brief 将棋盤の初期化・盤面アクセス・駒台操作・SFEN変換・局面編集の実装

#include "shogiboard.h"
#include "boardcoordinates.h"
#include "boardconstants.h"
#include "logcategories.h"

#include <algorithm>
#include "errorbus.h"
#include <array>

// ============================================================
// 初期化
// ============================================================

ShogiBoard::ShogiBoard(int ranks, int files, QObject *parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    initBoard();
    initStand();
}

void ShogiBoard::initBoard()
{
    m_boardData.fill(Piece::None, static_cast<qsizetype>(ranks()) * files());
}

namespace {

/// 16種の駒を0枚で持つ駒台の表。盤を作るたびに16個を挿入しないよう、すべての盤で共有する
/// （QMap は暗黙の共有なので、枚数を書き換えた盤だけが自分の表を持つ）
const QMap<Piece, int>& emptyStand()
{
    static const QMap<Piece, int> stand = [] {
        QMap<Piece, int> pieces;
        for (const Piece piece : {
                 Piece::WhitePawn, Piece::WhiteLance, Piece::WhiteKnight, Piece::WhiteSilver,
                 Piece::WhiteGold, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteKing,
                 Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
                 Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook, Piece::BlackKing}) {
            pieces.insert(piece, 0);
        }
        return pieces;
    }();
    return stand;
}

} // namespace

void ShogiBoard::initStand()
{
    m_pieceStand = emptyStand();
}

// ============================================================
// 盤面アクセス
// ============================================================

Piece ShogiBoard::pieceAt(BoardSquare square) const
{
    if (square.file < 1 || square.file > files() || square.rank < 1 || square.rank > ranks()) return Piece::None;
    return m_boardData.at((square.rank - 1) * files() + (square.file - 1));
}

void ShogiBoard::setPieceAt(BoardSquare square, Piece piece)
{
    if (square.file < 1 || square.file > files() || square.rank < 1 || square.rank > ranks()) return;
    setData(square.file, square.rank, piece);
}

int ShogiBoard::pieceStandCount(Piece piece) const
{
    return m_pieceStand.value(piece, 0);
}

void ShogiBoard::addStandPiece(Piece piece, int delta)
{
    if (delta <= 0 || !m_pieceStand.contains(piece)) {
        return;
    }
    m_pieceStand[piece] += delta;
    emit standChanged(piece);
}

bool ShogiBoard::consumeStandPiece(Piece piece)
{
    if (!m_pieceStand.contains(piece) || m_pieceStand[piece] <= 0) {
        return false;
    }
    m_pieceStand[piece]--;
    emit standChanged(piece);
    return true;
}

Turn ShogiBoard::currentPlayer() const
{
    return m_currentPlayer;
}

void ShogiBoard::setCurrentPlayer(Turn turn)
{
    if (m_currentPlayer == turn) return;
    m_currentPlayer = turn;
    emit turnChanged(turn);
}

// 指定した位置の駒を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
Piece ShogiBoard::pieceCharacter(const int file, const int rank) const
{
    if (file >= 1 && file <= 9) {
        if (rank < 1 || rank > ranks()) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return Piece::None;
        }
        return m_boardData.at((rank - 1) * files() + (file - 1));
    }
    const Piece standPiece = BoardCoordinates::standPieceAt(file, rank);
    if (standPiece == Piece::None) {
        qCWarning(lcCore, "Invalid stand coordinates: file=%d rank=%d", file, rank);
    }
    return standPiece;
}

bool ShogiBoard::setDataInternal(const int file, const int rank, const Piece piece)
{
    int index = (rank - 1) * files() + (file - 1);

    // 変更がなければ再描画不要
    if (m_boardData.at(index) == piece) return false;

    m_boardData[index] = piece;
    return true;
}

// ============================================================
// 盤面操作
// ============================================================

void ShogiBoard::setData(const int file, const int rank, const Piece piece)
{
    if (setDataInternal(file, rank, piece)) {
        // ShogiView::setBoardのconnectで再描画がトリガされる
        emit dataChanged(file, rank);
    }
}

// 編集局面でも使用されるため、歩/桂/香の禁置き段では自動で成駒に置き換える（必成）。
void ShogiBoard::movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                                   const int fileTo, const int rankTo, const bool promote)
{
    // 1) 「成る」指定が来ていれば、先に成駒へ
    if (promote) {
        selectedPiece = ::promote(selectedPiece);
    }

    // 2) 元位置を空白に
    if ((fileFrom >= 1) && (fileFrom <= 9)) {
        setData(fileFrom, rankFrom, Piece::None);
    }

    // 3) まず素の駒を置く（盤上のみ）
    if ((fileTo >= 1) && (fileTo <= 9)) {
        setData(fileTo, rankTo, selectedPiece);

        // 4) 置いた結果に対して「必成」補正（歩/桂/香）
        Piece placed = pieceCharacter(fileTo, rankTo);

        // 先手の必成
        if (placed == Piece::BlackPawn && rankTo == 1) {
            setData(fileTo, rankTo, Piece::BlackPromotedPawn);
        } else if (placed == Piece::BlackLance && rankTo == 1) {
            setData(fileTo, rankTo, Piece::BlackPromotedLance);
        } else if (placed == Piece::BlackKnight && (rankTo == 1 || rankTo == 2)) {
            setData(fileTo, rankTo, Piece::BlackPromotedKnight);
        }
        // 後手の必成
        else if (placed == Piece::WhitePawn && rankTo == 9) {
            setData(fileTo, rankTo, Piece::WhitePromotedPawn);
        } else if (placed == Piece::WhiteLance && rankTo == 9) {
            setData(fileTo, rankTo, Piece::WhitePromotedLance);
        } else if (placed == Piece::WhiteKnight && (rankTo == 8 || rankTo == 9)) {
            setData(fileTo, rankTo, Piece::WhitePromotedKnight);
        }
    }
}

bool ShogiBoard::applyUsiMove(const UsiMove& move, Turn mover)
{
    if (!move.isValid()) return false;
    if (move.isDrop()) {
        const Piece piece = move.dropPieceFor(mover);
        if (pieceCharacter(move.to.file, move.to.rank) != Piece::None) return false;
        if (!consumeStandPiece(piece)) return false;
        movePieceToSquare(piece, 0, 0, move.to.file, move.to.rank, false);
        advanceTurn(mover);
        return true;
    }
    const Piece moving = pieceCharacter(move.from.file, move.from.rank);
    if (moving == Piece::None) return false;
    const Piece captured = pieceCharacter(move.to.file, move.to.rank);
    if (captured != Piece::None) addPieceToStand(captured);
    movePieceToSquare(moving, move.from.file, move.from.rank, move.to.file, move.to.rank, move.promote);
    advanceTurn(mover);
    return true;
}

void ShogiBoard::advanceTurn(Turn mover)
{
    m_currentPlayer = (mover == Turn::Black) ? Turn::White : Turn::Black;
    ++m_moveNumber;
    emit turnChanged(m_currentPlayer);
}

void ShogiBoard::copyPositionFrom(const ShogiBoard& other)
{
    if (this == &other) return;
    m_ranks = other.m_ranks;
    m_files = other.m_files;
    if (m_boardData.size() == other.m_boardData.size()) {
        std::copy(other.m_boardData.cbegin(), other.m_boardData.cend(), m_boardData.begin());
    } else {
        m_boardData = other.m_boardData;
    }
    // 駒台は、駒の種類（キー）が同じなら枚数だけを上書きする。違えば丸ごと写す
    bool sameKinds = m_pieceStand.size() == other.m_pieceStand.size();
    auto dst = m_pieceStand.begin();
    for (auto src = other.m_pieceStand.cbegin(); sameKinds && src != other.m_pieceStand.cend(); ++src, ++dst) {
        sameKinds = dst.key() == src.key();
        if (sameKinds) dst.value() = src.value();
    }
    if (!sameKinds) m_pieceStand = other.m_pieceStand;
    m_currentPlayer = other.m_currentPlayer;
    m_moveNumber = other.m_moveNumber;

    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit boardReset();
}

// ============================================================
// 駒台操作
// ============================================================

// 成駒は相手の生駒に変換し、それ以外は先後を反転する。
Piece ShogiBoard::convertPieceChar(const Piece c) const
{
    // 成駒→相手の生駒
    if (isPromoted(c)) {
        Piece base = demote(c);
        return isBlackPiece(base) ? toWhite(base) : toBlack(base);
    }
    // 非成駒→先後反転
    return isBlackPiece(c) ? toWhite(c) : toBlack(c);
}

// 指したマスに相手の駒があった場合、自分の駒台の枚数に1加える。
void ShogiBoard::addPieceToStand(Piece dest)
{
    Piece pieceChar = convertPieceChar(dest);
    addStandPiece(pieceChar);
}

bool ShogiBoard::decrementPieceOnStand(Piece source)
{
    if (!consumeStandPiece(source)) {
        qCWarning(lcCore, "decrementPieceOnStand: no piece on stand (%c)",
                  static_cast<char>(source));
        return false;
    }
    return true;
}

// 駒台から指そうとした場合、駒台の駒数が0以下なら指せない。
bool ShogiBoard::isPieceAvailableOnStand(const Piece source, const int fileFrom) const
{
    if (fileFrom == BoardConstants::kPieceBoxFile) return pieceBoxCount(source) > 0;
    if (fileFrom == BoardConstants::kBlackStandFile || fileFrom == BoardConstants::kWhiteStandFile) {
        return pieceStandCount(source) > 0;
    }
    return true;
}

Piece ShogiBoard::convertPromotedPieceToOriginal(const Piece dest) const
{
    if (isPromoted(dest)) {
        return demote(dest);
    }
    return dest;
}

// 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
void ShogiBoard::incrementPieceOnStand(const Piece dest)
{
    Piece originalPiece = convertPromotedPieceToOriginal(dest);
    addStandPiece(originalPiece);
}

// ============================================================
// 盤面編集の実装
// ============================================================

// ============================================================
// 盤面更新
// ============================================================

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
{
    if (fileTo < BoardConstants::kBlackStandFile) {
        // 指したマスに相手の駒があった場合、自分の駒台に加える
        addPieceToStand(dest);
    } else if (fileTo != BoardConstants::kPieceBoxFile) {
        // 駒台に駒を移した場合
        incrementPieceOnStand(dest);
    }

    if (fileFrom == BoardConstants::kBlackStandFile || fileFrom == BoardConstants::kWhiteStandFile) {
        decrementPieceOnStand(source);
    }

    movePieceToSquare(source, fileFrom, rankFrom, fileTo, rankTo, promote);
}

QMap<Piece, int> ShogiBoard::pieceBox() const
{
    QMap<Piece, int> counts = {
        {Piece::BlackPawn, 18}, {Piece::BlackLance, 4}, {Piece::BlackKnight, 4},
        {Piece::BlackSilver, 4}, {Piece::BlackGold, 4}, {Piece::BlackBishop, 2},
        {Piece::BlackRook, 2}, {Piece::BlackKing, 2},
    };
    // SFENに駒箱は含めない。局面から復元するため、編集の再開・棋譜読込でも失われない。
    for (Piece piece : m_boardData) {
        if (piece != Piece::None) --counts[toBlack(demote(piece))];
    }
    for (auto it = m_pieceStand.cbegin(); it != m_pieceStand.cend(); ++it)
        counts[toBlack(demote(it.key()))] -= it.value();
    for (auto it = counts.begin(); it != counts.end(); ++it) it.value() = qMax(0, it.value());
    return counts;
}

int ShogiBoard::pieceBoxCount(Piece piece) const
{
    return pieceBox().value(toBlack(demote(piece)), 0);
}

void ShogiBoard::resetGameBoard()
{
    initBoard();
    initStand();
    emit boardReset();
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    QList<Piece> originalBoardData = m_boardData;
    QList<Piece> newBoardData;

    for (int i = 0; i < 81; i++) {
        Piece piece = originalBoardData.at(80 - i);
        // 先後反転
        if (isBlackPiece(piece)) {
            piece = toWhite(piece);
        } else if (isWhitePiece(piece)) {
            piece = toBlack(piece);
        }
        newBoardData.append(piece);
    }

    m_boardData = newBoardData;

    const QMap<Piece, int> originalPieceStand = m_pieceStand;

    for (auto it = originalPieceStand.cbegin(); it != originalPieceStand.cend(); ++it) {
        Piece flipped = isBlackPiece(it.key()) ? toWhite(it.key()) : toBlack(it.key());
        m_pieceStand[flipped] = it.value();
    }
}

// ============================================================
// 局面編集: 成駒/不成駒/先後の巡回変換
// ============================================================

// 局面編集中に右クリックで成駒/不成駒/先後を巡回変換する（禁置き段＋二歩をスキップ）。
void ShogiBoard::promoteOrDemotePiece(const int fileFrom, const int rankFrom)
{
    if (fileFrom < 1 || fileFrom > files() || rankFrom < 1 || rankFrom > ranks()) return;
    // 処理フロー:
    // 1. 駒種ごとの巡回リストを特定
    // 2. 禁置き段・二歩の候補をフィルタ
    // 3. フィルタ済みリストで次の候補に切り替え

    auto nextInCycle = [](const QList<Piece>& cyc, Piece cur)->Piece {
        qsizetype idx = cyc.indexOf(cur);
        if (idx < 0) return cur;
        return cyc[(idx + 1) % cyc.size()];
    };

    const auto lanceCycle  = QList<Piece>{Piece::BlackLance, Piece::BlackPromotedLance, Piece::WhiteLance, Piece::WhitePromotedLance};
    const auto knightCycle = QList<Piece>{Piece::BlackKnight, Piece::BlackPromotedKnight, Piece::WhiteKnight, Piece::WhitePromotedKnight};
    const auto silverCycle = QList<Piece>{Piece::BlackSilver, Piece::BlackPromotedSilver, Piece::WhiteSilver, Piece::WhitePromotedSilver};
    const auto bishopCycle = QList<Piece>{Piece::BlackBishop, Piece::BlackHorse, Piece::WhiteBishop, Piece::WhiteHorse};
    const auto rookCycle   = QList<Piece>{Piece::BlackRook, Piece::BlackDragon, Piece::WhiteRook, Piece::WhiteDragon};
    const auto pawnCycle   = QList<Piece>{Piece::BlackPawn, Piece::BlackPromotedPawn, Piece::WhitePawn, Piece::WhitePromotedPawn};
    const auto goldCycle   = QList<Piece>{Piece::BlackGold, Piece::WhiteGold};
    const auto kingCycle   = QList<Piece>{Piece::BlackKing, Piece::WhiteKing};

    const Piece cur = pieceCharacter(fileFrom, rankFrom);
    QList<Piece> base;
    switch (static_cast<char>(cur)) {
    case 'L': case 'M': case 'l': case 'm': base = lanceCycle;  break;
    case 'N': case 'O': case 'n': case 'o': base = knightCycle; break;
    case 'S': case 'T': case 's': case 't': base = silverCycle; break;
    case 'B': case 'C': case 'b': case 'c': base = bishopCycle; break;
    case 'R': case 'U': case 'r': case 'u': base = rookCycle;   break;
    case 'P': case 'Q': case 'p': case 'q': base = pawnCycle;   break;
    case 'G': case 'g': base = goldCycle; break;
    case 'K': case 'k': base = kingCycle; break;
    default:
        return;
    }

    const bool onBoard = (fileFrom >= 1 && fileFrom <= 9);

    auto isRankDisallowed = [&](Piece piece)->bool {
        if (!onBoard) return false;
        if (piece == Piece::BlackLance && rankFrom == 1) return true;
        if (piece == Piece::BlackKnight && (rankFrom == 1 || rankFrom == 2)) return true;
        if (piece == Piece::BlackPawn && rankFrom == 1) return true;
        if (piece == Piece::WhiteLance && rankFrom == 9) return true;
        if (piece == Piece::WhiteKnight && (rankFrom == 8 || rankFrom == 9)) return true;
        if (piece == Piece::WhitePawn && rankFrom == 9) return true;
        return false;
    };

    auto isNiFuDisallowed = [&](Piece candidate)->bool {
        if (!onBoard) return false;
        if (candidate != Piece::BlackPawn && candidate != Piece::WhitePawn) return false;
        for (int r = 1; r <= 9; ++r) {
            if (r == rankFrom) continue;
            const Piece pc = pieceCharacter(fileFrom, r);
            if (candidate == Piece::BlackPawn && pc == Piece::BlackPawn) return true;
            if (candidate == Piece::WhitePawn && pc == Piece::WhitePawn) return true;
        }
        return false;
    };

    auto isDisallowed = [&](Piece piece)->bool {
        return isRankDisallowed(piece) || isNiFuDisallowed(piece);
    };

    QList<Piece> filtered;
    filtered.reserve(base.size());
    for (Piece p : std::as_const(base)) {
        if (!isDisallowed(p)) filtered.push_back(p);
    }
    if (filtered.isEmpty()) return;

    // 現在形がfiltered外（既に禁止形）なら、baseを回して最初の許可形へジャンプ
    Piece next = cur;
    if (filtered.indexOf(cur) < 0) {
        Piece probe = cur;
        for (qsizetype i = 0; i < base.size(); ++i) {
            probe = nextInCycle(base, probe);
            if (filtered.indexOf(probe) >= 0) { next = probe; break; }
        }
    } else {
        qsizetype idx = filtered.indexOf(cur);
        next = filtered[(idx + 1) % filtered.size()];
    }

    setData(fileFrom, rankFrom, next);
}

// ============================================================
// SFEN文字列の入力・検証・出力・記録処理
// ============================================================

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
    if (ch != QLatin1Char('+')) {
        // 成駒は "+" で書く。内部で使う1文字の成駒（Q・M・O・T・C・U）は SFEN の文字ではない
        const Piece piece = charToPiece(ch);
        return isPromoted(piece) ? Piece::None : piece;
    }
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

    // 駒台は16種の駒の枚数。種類がそろっていれば値だけ書き換える（作り直さない）。
    // 作ったばかりの盤は0枚の表をほかの盤と共有しているので、枚数が変わらなければ書き込まない（書き込むと表を複製する）
    bool sameKinds = m_pieceStand.size() == kStandKinds;
    bool sameCounts = sameKinds;
    for (auto it = m_pieceStand.cbegin(); sameKinds && it != m_pieceStand.cend(); ++it) {
        const int slot = standSlot(it.key());
        sameKinds = slot >= 0;
        sameCounts = sameCounts && sameKinds && it.value() == standCounts[static_cast<size_t>(slot)];
    }
    if (!sameKinds) initStand();
    if (!sameCounts) {
        for (auto it = m_pieceStand.begin(); it != m_pieceStand.end(); ++it) {
            it.value() = standCounts[static_cast<size_t>(standSlot(it.key()))];
        }
    }

    m_currentPlayer = fields->turn;
    m_moveNumber = fields->moveNumber;

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

QString ShogiBoard::convertBoardToSfen() const
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
// SFEN出力（局面全体）
// ============================================================

QString ShogiBoard::toSfen() const
{
    return QStringLiteral("%1 %2 %3 %4")
        .arg(convertBoardToSfen(), turnToSfen(m_currentPlayer), convertStandToSfen(), QString::number(m_moveNumber));
}
