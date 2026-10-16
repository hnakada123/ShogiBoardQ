/// @file pvnotationformatter.cpp
/// @brief USI の info（UsiInfo）の読み筋・探索中の手を漢字表記にするクラスの実装

#include "pvnotationformatter.h"
#include "terminaltype.h"
#include "kifmovetext.h"
#include "logcategories.h"

namespace {

/// 読み筋の末尾に付く "(57.54%)" のような、指し手でない語か（指し手の形の崩れたものは含めない）
bool isPvAnnotation(QStringView token)
{
    if (token.size() < 4) return false;
    const QChar head = token.front();
    return !head.isDigit() && !QStringView(u"PLNSGBR").contains(head);
}

bool belongsTo(Piece piece, Turn side)
{
    return side == Turn::Black ? isBlackPiece(piece) : isWhitePiece(piece);
}

} // namespace

// ============================================================
// 局面
// ============================================================

void PvNotationFormatter::setPosition(const QString& sfen)
{
    m_hasPosition = !sfen.isEmpty() && m_position.assignSfen(sfen);
    if (!m_hasPosition && !sfen.isEmpty()) {
        qCWarning(lcEngine) << "読み筋の局面を読めません:" << sfen.left(80);
    }
}

void PvNotationFormatter::setPreviousMove(BoardSquare to)
{
    m_previousTo = to;
}

Piece PvNotationFormatter::movingPiece(const Position& board, const UsiMove& move, Turn mover)
{
    if (move.isDrop()) {
        const Piece piece = move.dropPieceFor(mover);
        if (board.pieceAt(move.to) != Piece::None || board.handCount(piece) <= 0) return Piece::None;
        return piece;
    }
    const Piece piece = board.pieceAt(move.from);
    if (!belongsTo(piece, mover) || belongsTo(board.pieceAt(move.to), mover)) return Piece::None;
    return piece;
}

// ============================================================
// 変換
// ============================================================

QString PvNotationFormatter::convertCurrMoveToKanjiNotation(QStringView str) const
{
    const std::optional<UsiMove> move = UsiMove::parse(str);
    if (!m_hasPosition || !move) return QString();

    const Turn mover = m_position.sideToMove();
    const Piece moving = movingPiece(m_position, *move, mover);
    if (moving == Piece::None) return QString();

    QString text;
    KifMoveText::append(text, mover, moving, *move, m_previousTo, KifMoveText::SameSquare::Compact);
    return text;
}

void PvNotationFormatter::format(const UsiInfo& info)
{
    m_pvKanjiStr.clear();
    m_pvUsiStr.clear();
    m_searchedHand.clear();
    m_searchedUsi.clear();

    if (!info.currmove.isEmpty()) {
        m_searchedUsi = info.currmove;
        m_searchedHand = convertCurrMoveToKanjiNotation(info.currmove);
    }
    // pv のない info（info string など）は読み筋として表示しない
    if (!info.pv.isEmpty()) formatPv(info.pv);
}

void PvNotationFormatter::formatPv(const QStringList& pv)
{
    if (!m_hasPosition) return;

    // 読み筋は info ごとに全手を変換する。1手ずつ文字列を作らず、結果の文字列に直接足す
    const qsizetype moveCount = pv.size();
    m_pvKanjiStr.reserve(moveCount * 10);
    m_pvUsiStr.reserve(moveCount * 6);

    m_pvBoard = m_position;
    Turn mover = m_position.sideToMove();
    BoardSquare previousTo = m_previousTo;

    for (qsizetype i = 0; i < pv.size(); ++i) {
        const QString& token = pv.at(i);

        // 詰んだ局面などでエンジンが返す resign / win は指し手ではなく終局の宣言。
        // 棋譜と同じ日本語表記にし、表示時に KifuPresentation が表記・言語に合わせる
        if (token == QLatin1String("resign") || token == QLatin1String("win")) {
            m_pvKanjiStr += QChar(mover == Turn::Black ? u'▲' : u'△');
            m_pvKanjiStr += TerminalTypes::label(token == QLatin1String("resign") ? TerminalType::Resign
                                                                                  : TerminalType::DeclarationWin);
            return;
        }

        const std::optional<UsiMove> move = UsiMove::parse(token);
        if (!move) {
            if (i == pv.size() - 1 && isPvAnnotation(token)) {
                // 末尾の "(57.54%)" 等はそのまま付加して終了（m_pvUsiStr には含めない）
                m_pvKanjiStr += QLatin1Char(' ');
                m_pvKanjiStr += token;
            } else if (!isPvAnnotation(token)) {
                qCWarning(lcEngine) << "読み筋の指し手を読めません:" << token;
            }
            return;
        }

        const Piece moving = movingPiece(m_pvBoard, *move, mover);
        if (moving == Piece::None) {
            // 局面と合わない指し手は、USI形式の読み筋を残さない
            qCWarning(lcEngine) << "読み筋の指し手が局面と合いません:" << token;
            m_pvUsiStr.clear();
            return;
        }

        // USI形式の読み筋は、変換できた手までを空白区切りで並べる
        if (!m_pvUsiStr.isEmpty()) m_pvUsiStr += QLatin1Char(' ');
        m_pvUsiStr += token;

        const qsizetype moveStart = m_pvKanjiStr.size();
        KifMoveText::append(m_pvKanjiStr, mover, moving, *move, previousTo, KifMoveText::SameSquare::Compact);
        if (i == 0) {
            m_searchedHand = m_pvKanjiStr.mid(moveStart);
            m_searchedUsi = token;
        }

        m_pvBoard.applyAs(*move, mover);
        previousTo = move->to;
        mover = (mover == Turn::Black) ? Turn::White : Turn::Black;
    }
}
