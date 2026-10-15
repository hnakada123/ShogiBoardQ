/// @file sfenpositiontracer.cpp
/// @brief USI手列を順に適用して各手後のSFEN局面を生成する簡易トレーサの実装

#include "sfenpositiontracer.h"
#include "shogiboard.h"
#include "shogitypes.h"
#include "sfenutils.h"
#include "usimove.h"

namespace {

/// 駒を SFEN のトークン（成駒は "+P" の形）にする
QString tokenFor(Piece piece)
{
    if (piece == Piece::None) return {};
    if (isPromoted(piece)) return QStringLiteral("+") + pieceToChar(demote(piece));
    return QString(pieceToChar(piece));
}

/// 4フィールド形式の SFEN の形（盤9段・成りの印の後に駒）を確かめる
bool hasSfenShape(const QString& sfen)
{
    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 4) return false;
    const QStringList ranks = parts.at(0).split(QLatin1Char('/'));
    if (ranks.size() != 9) return false;
    for (const QString& row : ranks) {
        if (row.endsWith(QLatin1Char('+'))) return false;
    }
    return true;
}

} // namespace

// ======================================================================
// コンストラクタ・初期化
// ======================================================================

SfenPositionTracer::SfenPositionTracer()
    : m_board(std::make_unique<ShogiBoard>())
{
    resetToStartpos();
}

SfenPositionTracer::~SfenPositionTracer() = default;

SfenPositionTracer::SfenPositionTracer(const SfenPositionTracer& other)
    : m_board(std::make_unique<ShogiBoard>())
{
    *this = other;
}

SfenPositionTracer& SfenPositionTracer::operator=(const SfenPositionTracer& other)
{
    if (this != &other) {
        (void)setFromSfen(other.toSfenString());
    }
    return *this;
}

void SfenPositionTracer::resetToStartpos()
{
    (void)m_board->trySetSfen(SfenUtils::hirateSfen());
    m_blackToMove = true;
    m_plyNext = 1;
}

// ======================================================================
// 手の適用・SFEN生成
// ======================================================================

#ifdef SHOGIBOARDQ_TESTING
QStringList SfenPositionTracer::generateSfensForMoves(const QStringList& usiMoves) {
    resetToStartpos();
    QStringList out;
    out.reserve(usiMoves.size());
    for (const QString& mv : std::as_const(usiMoves)) {
        if (!applyUsiMove(mv)) break;
        out << toSfenString();
    }
    return out;
}
#endif

bool SfenPositionTracer::applyUsiMove(const QString& usi)
{
    const auto move = UsiMove::parse(usi);
    if (!move) return false;

    const Turn mover = m_blackToMove ? Turn::Black : Turn::White;
    if (move->isDrop()) {
        // 枚数チェックは緩めにしておく（棋譜の読み込みでは、持ち駒の数が合わなくても続けたい）
        const Piece piece = move->dropPieceFor(mover);
        if (m_board->pieceStandCount(piece) > 0) m_board->consumeStandPiece(piece);
        m_board->movePieceToSquare(piece, 0, 0, move->to.file, move->to.rank, false);
    } else if (!m_board->applyUsiMove(*move, mover)) {
        return false;
    }

    m_blackToMove = !m_blackToMove;
    ++m_plyNext;
    return true;
}

QString SfenPositionTracer::toSfenString() const
{
    return QStringLiteral("%1 %2 %3 %4")
        .arg(m_board->convertBoardToSfen(), m_blackToMove ? QStringLiteral("b") : QStringLiteral("w"),
             m_board->convertStandToSfen(), QString::number(m_plyNext));
}

QString SfenPositionTracer::sfenFromPositionCommand(const QString& positionCommand)
{
    const QStringList tokens = positionCommand.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.size() < 2 || tokens.at(0) != QStringLiteral("position")) {
        return {};
    }

    SfenPositionTracer tracer;
    int movesIndex = -1;

    if (tokens.at(1) == QStringLiteral("startpos")) {
        tracer.resetToStartpos();
        movesIndex = static_cast<int>(tokens.indexOf(QStringLiteral("moves"), 2));
    } else if (tokens.at(1) == QStringLiteral("sfen")) {
        if (tokens.size() < 6) {
            return {};
        }
        const QString sfen = tokens.mid(2, 4).join(QLatin1Char(' '));
        if (!tracer.setFromSfen(sfen)) {
            return {};
        }
        movesIndex = static_cast<int>(tokens.indexOf(QStringLiteral("moves"), 6));
    } else {
        return {};
    }

    if (movesIndex >= 0) {
        for (qsizetype i = movesIndex + 1; i < tokens.size(); ++i) {
            if (!tracer.applyUsiMove(tokens.at(i))) {
                return {};
            }
        }
    }

    return tracer.toSfenString();
}

// ======================================================================
// SFEN読み込み・駒の参照
// ======================================================================

bool SfenPositionTracer::setFromSfen(const QString& sfen)
{
    if (!hasSfenShape(sfen)) return false;
    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (!m_board->trySetSfen(parts.mid(0, 4).join(QLatin1Char(' ')))) return false;
    m_blackToMove = (parts.at(1) == QStringLiteral("b"));
    bool ok = false;
    const int ply = parts.at(3).toInt(&ok);
    m_plyNext = (ok && ply > 0) ? ply : 1;
    return true;
}

Piece SfenPositionTracer::pieceAt(int file, int rank) const
{
    return m_board->pieceAt(BoardSquare{file, rank});
}

QString SfenPositionTracer::tokenAtFileRank(int file, QChar rankLetter) const
{
    const auto square = UsiNotation::parseSquare(QString(QChar(u'0' + file)) + rankLetter.toLower());
    return square ? tokenFor(pieceAt(square->file, square->rank)) : QString();
}

QString SfenPositionTracer::tokenAt(int file, int row) const
{
    return tokenFor(pieceAt(file, row + 1));
}

// ======================================================================
// 局面列構築ユーティリティ
// ======================================================================

SfenPositionTracer::ReplayResult SfenPositionTracer::replay(const QString& initialSfen,
                                                            const QStringList& usiMoves)
{
    ReplayResult result;
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(initialSfen)) {
        result.failedPly = 0;
        result.error = QStringLiteral("Invalid initial SFEN: %1").arg(initialSfen);
        return result;
    }
    result.initialSfen = tracer.toSfenString();
    for (const QString& text : usiMoves) {
        const auto usi = UsiMove::parse(text);
        const Turn mover = tracer.blackToMove() ? Turn::Black : Turn::White;
        ReplayedMove step;
        step.beforeSfen = tracer.toSfenString();
        if (usi) {
            const Piece moving = usi->isDrop() ? usi->dropPieceFor(mover)
                                               : tracer.pieceAt(usi->from.file, usi->from.rank);
            const Piece captured = tracer.pieceAt(usi->to.file, usi->to.rank);
            step.move = ShogiMove::fromUsiMove(*usi, mover, moving, captured);
        }
        const bool emptySource = usi && !usi->isDrop() && step.move.movingPiece == Piece::None;
        if (!usi || emptySource || !tracer.applyUsiMove(text)) {
            result.failedPly = static_cast<int>(result.moves.size()) + 1;
            result.error = !usi ? QStringLiteral("Invalid USI move: %1").arg(text)
                : emptySource ? QStringLiteral("Source square is empty: %1").arg(text)
                              : QStringLiteral("Move cannot be applied: %1").arg(text);
            return result;
        }
        step.sfen = tracer.toSfenString();
        result.moves.append(step);
    }
    return result;
}

QStringList SfenPositionTracer::buildSfenRecord(const QString& initialSfen,
                                                const QStringList& usiMoves, bool hasTerminal)
{
    const auto result = replay(initialSfen, usiMoves);
    if (!result.success()) return {};
    QStringList sfens{result.initialSfen};
    for (const auto& move : result.moves) sfens.append(move.sfen);
    if (hasTerminal) sfens.append(sfens.last());
    return sfens;
}

QList<ShogiMove> SfenPositionTracer::buildGameMoves(const QString& initialSfen, const QStringList& usiMoves)
{
    const auto result = replay(initialSfen, usiMoves);
    if (!result.success()) return {};
    QList<ShogiMove> moves;
    for (const auto& move : result.moves) moves.append(move.move);
    return moves;
}
