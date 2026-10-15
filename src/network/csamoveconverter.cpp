/// @file csamoveconverter.cpp
/// @brief CSA/USI/SFEN形式変換ユーティリティクラスの実装

#include "csamoveconverter.h"
#include "shoginotation.h"
#include "boardconstants.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "logcategories.h"
#include "usimove.h"
#include "kifmovetext.h"
#include <QHash>

namespace {
constexpr QChar kRankMin = QLatin1Char('a');
constexpr QChar kRankMax = QLatin1Char('i');
bool isValidBoardCoordinate(int file, int rank)
{
    return file >= 1 && file <= 9 && rank >= 1 && rank <= 9;
}
bool isValidDropSource(int file, int rank)
{
    return file == 0 && rank == 0;
}
bool isPromotedCsaPiece(const QString& piece)
{
    return isPromoted(ShogiNotation::pieceFromCsaCode(piece));
}
bool isKnownCsaPiece(const QString& piece)
{
    return ShogiNotation::pieceFromCsaCode(piece) != Piece::None;
}
bool isValidUsiRank(QChar rank)
{
    return rank >= kRankMin && rank <= kRankMax;
}
} // namespace

// ============================================================
// CSA <-> USI 形式変換
// ============================================================

QString CsaMoveConverter::csaToUsi(const QString& csaMove)
{
    if (csaMove.length() < 7) {
        return QString();
    }
    if (csaMove[0] != QLatin1Char('+') && csaMove[0] != QLatin1Char('-')) {
        qCWarning(lcNetwork) << "csaToUsi: invalid turn prefix:" << csaMove;
        return QString();
    }

    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    const int toFile = csaMove[3].digitValue();
    const int toRank = csaMove[4].digitValue();
    const QString piece = csaMove.mid(5, 2);

    if (!isValidBoardCoordinate(toFile, toRank)) {
        qCWarning(lcNetwork) << "csaToUsi: invalid destination coordinate:" << csaMove;
        return QString();
    }
    if (!isKnownCsaPiece(piece)) {
        qCWarning(lcNetwork) << "csaToUsi: unknown piece code:" << piece;
        return QString();
    }

    QString usiMove;

    if (isValidDropSource(fromFile, fromRank)) {
        // 駒打ち
        const QString usiPiece = csaPieceToUsi(piece);
        if (usiPiece.isEmpty() || usiPiece.startsWith(QLatin1Char('+'))) {
            qCWarning(lcNetwork) << "csaToUsi: invalid drop piece:" << piece;
            return QString();
        }
        usiMove = QString("%1*%2%3")
                      .arg(usiPiece)
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));
    } else {
        if (!isValidBoardCoordinate(fromFile, fromRank)) {
            qCWarning(lcNetwork) << "csaToUsi: invalid source coordinate:" << csaMove;
            return QString();
        }
        // 通常の移動
        usiMove = QString("%1%2%3%4")
                      .arg(fromFile)
                      .arg(QChar('a' + fromRank - 1))
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));

        // 成り判定
        if (isPromotedCsaPiece(piece)) {
            usiMove += QStringLiteral("+");
        }
    }

    return usiMove;
}

QString CsaMoveConverter::usiToCsa(const QString& usiMove, bool isBlack, ShogiBoard* board)
{
    if (usiMove.isEmpty()) {
        return QString();
    }

    QChar turnSign = isBlack ? QLatin1Char('+') : QLatin1Char('-');
    QString csaMove;

    if (usiMove.contains(QLatin1Char('*'))) {
        if (usiMove.size() != 4 || usiMove[1] != QLatin1Char('*')) {
            qCWarning(lcNetwork) << "usiToCsa: invalid drop format:" << usiMove;
            return QString();
        }
        // 駒打ち: P*7f
        const QChar usiPiece = usiMove[0];
        const int toFile = usiMove[2].digitValue();
        const QChar toRankChar = usiMove[3];
        const int toRank = toRankChar.toLatin1() - 'a' + 1;
        if (!isValidBoardCoordinate(toFile, toRank) || !isValidUsiRank(toRankChar)) {
            qCWarning(lcNetwork) << "usiToCsa: invalid drop destination:" << usiMove;
            return QString();
        }

        const QString csaPiece = usiPieceToCsa(QString(usiPiece), false);
        if (csaPiece.isEmpty()) {
            qCWarning(lcNetwork) << "usiToCsa: unknown drop piece:" << usiPiece;
            return QString();
        }
        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(0)
                      .arg(0)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    } else {
        if (usiMove.size() < 4) {
            qCWarning(lcNetwork) << "usiToCsa: invalid move format:" << usiMove;
            return QString();
        }
        // 通常の移動
        const int fromFile = usiMove[0].digitValue();
        const QChar fromRankChar = usiMove[1];
        const int fromRank = fromRankChar.toLatin1() - 'a' + 1;
        const int toFile = usiMove[2].digitValue();
        const QChar toRankChar = usiMove[3];
        const int toRank = toRankChar.toLatin1() - 'a' + 1;
        const bool promote = usiMove.endsWith(QLatin1Char('+'));
        if (!isValidBoardCoordinate(fromFile, fromRank)
            || !isValidBoardCoordinate(toFile, toRank)
            || !isValidUsiRank(fromRankChar) || !isValidUsiRank(toRankChar)) {
            qCWarning(lcNetwork) << "usiToCsa: invalid coordinates:" << usiMove;
            return QString();
        }

        QString csaPiece;
        if (!board) {
            qCWarning(lcNetwork) << "usiToCsa: board is null:" << usiMove;
            return QString();
        }
        const Piece piece = board->pieceCharacter(fromFile, fromRank);
        csaPiece = pieceCharToCsa(piece, promote);
        if (csaPiece.isEmpty()) {
            qCWarning(lcNetwork) << "usiToCsa: failed to resolve source piece, usiMove=" << usiMove;
            return QString();
        }

        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(fromFile)
                      .arg(fromRank)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    }

    return csaMove;
}

// ============================================================
// 盤面座標 <-> CSA 変換
// ============================================================

QString CsaMoveConverter::boardToCSA(const QPoint& from, const QPoint& to, bool promote,
                                     bool isBlackSide, ShogiBoard* board)
{
    qCDebug(lcNetwork) << "boardToCSA: from=" << from << "to=" << to << "promote=" << promote;
    qCDebug(lcNetwork) << "isBlackSide=" << isBlackSide;

    QChar turnSign = isBlackSide ? QLatin1Char('+') : QLatin1Char('-');

    int fromFile = from.x();
    int fromRank = from.y();
    int toFile = to.x();
    int toRank = to.y();

    qCDebug(lcNetwork) << "Calculated: fromFile=" << fromFile << "fromRank=" << fromRank
                       << "toFile=" << toFile << "toRank=" << toRank;

    bool isDrop = (from.x() >= BoardConstants::kBlackStandFile);
    if (isDrop) {
        fromFile = 0;
        fromRank = 0;
        qCDebug(lcNetwork) << "This is a drop move";
    }

    QString csaPiece;
    if (board) {
        Piece piece;
        if (isDrop) {
            piece = board->pieceCharacter(from.x(), from.y());
            qCDebug(lcNetwork) << "Got piece from stand:" << pieceToChar(piece)
                               << "at file=" << from.x() << "rank=" << from.y();
        } else {
            // 通常の移動: validateAndMoveで既に盤面が更新されているので、
            // 移動先(to)から駒を取得する
            piece = board->pieceCharacter(toFile, toRank);
            qCDebug(lcNetwork) << "Got piece from board (at destination):" << pieceToChar(piece)
                               << "at file=" << toFile << "rank=" << toRank;
        }
        csaPiece = pieceCharToCsa(piece, promote);
        qCDebug(lcNetwork) << "Converted to CSA piece:" << csaPiece;
    } else {
        qCWarning(lcNetwork) << "boardToCSA: board is null";
    }
    if (csaPiece.isEmpty()) {
        qCWarning(lcNetwork) << "boardToCSA: failed to resolve piece at move"
                             << "from=" << from << "to=" << to << "promote=" << promote;
        return QString();
    }

    QString result = QString("%1%2%3%4%5%6")
        .arg(turnSign)
        .arg(fromFile)
        .arg(fromRank)
        .arg(toFile)
        .arg(toRank)
        .arg(csaPiece);

    qCDebug(lcNetwork) << "Final CSA move string:" << result;
    return result;
}

// ============================================================
// 駒変換
// ============================================================

QString CsaMoveConverter::pieceCharToCsa(Piece piece, bool promote)
{
    const QString code = ShogiNotation::csaPieceCode(promote ? ::promote(piece) : piece);
    if (code.isEmpty()) {
        qCWarning(lcNetwork) << "Unknown piece character:" << pieceToChar(piece);
    }
    return code;
}

QString CsaMoveConverter::csaPieceToUsi(const QString& csaPiece)
{
    const Piece piece = ShogiNotation::pieceFromCsaCode(csaPiece);
    if (piece == Piece::None) {
        qCWarning(lcNetwork) << "csaPieceToUsi: unknown piece code:" << csaPiece;
        return QString();
    }
    return (isPromoted(piece) ? QStringLiteral("+") : QString()) + pieceToChar(demote(piece));
}

QString CsaMoveConverter::usiPieceToCsa(const QString& usiPiece, bool promoted)
{
    // USI の駒文字は大文字だけを読む
    const Piece base = usiPiece.size() == 1 ? charToPiece(usiPiece.at(0)) : Piece::None;
    // 金・玉は成れないので、成った駒としては読まない
    const Piece piece = promoted ? ::promote(base) : base;
    if (!isBlackPiece(base) || isPromoted(base) || (promoted && piece == base)) {
        qCWarning(lcNetwork) << "usiPieceToCsa: unknown piece:" << usiPiece << "promoted=" << promoted;
        return QString();
    }
    return ShogiNotation::csaPieceCode(piece);
}

QString CsaMoveConverter::csaPieceToKanji(const QString& csaPiece)
{
    const Piece piece = ShogiNotation::pieceFromCsaCode(csaPiece);
    return piece == Piece::None ? csaPiece : ShogiNotation::movePieceName(piece);
}

bool CsaMoveConverter::isPromotionMove(const QString& csaMove, ShogiBoard* board)
{
    if (!board || csaMove.length() < 7) return false;
    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    if (!isValidBoardCoordinate(fromFile, fromRank)) return false;  // 駒打ちは成らない
    if (!isPromoted(ShogiNotation::pieceFromCsaCode(csaMove.mid(5, 2)))) return false;
    const Piece source = board->pieceCharacter(fromFile, fromRank);
    return source != Piece::None && !isPromoted(source) && promote(source) != source;
}

int CsaMoveConverter::pieceTypeFromCsa(const QString& csaPiece)
{
    // 駒の種類（成駒は元の駒）: 歩1 香2 桂3 銀4 金5 角6 飛7 玉8
    switch (demote(ShogiNotation::pieceFromCsaCode(csaPiece))) {
    case Piece::BlackPawn:   return 1;
    case Piece::BlackLance:  return 2;
    case Piece::BlackKnight: return 3;
    case Piece::BlackSilver: return 4;
    case Piece::BlackGold:   return 5;
    case Piece::BlackBishop: return 6;
    case Piece::BlackRook:   return 7;
    case Piece::BlackKing:   return 8;
    default:                 return 0;
    }
}

Piece CsaMoveConverter::csaPieceToSfenPiece(const QString& csaPiece, bool isBlack)
{
    // 持ち駒・駒打ちに使う生駒だけを読む
    const Piece piece = ShogiNotation::pieceFromCsaCode(csaPiece, isBlack ? Turn::Black : Turn::White);
    if (piece == Piece::None || isPromoted(piece)) {
        qCWarning(lcNetwork) << "csaPieceToSfenPiece: unknown piece code:" << csaPiece;
        return Piece::None;
    }
    return piece;
}

// ============================================================
// CsaMoveConverter の表示・終局文言・盤面適用実装
// ============================================================

// ============================================================
// 表示用変換
// ============================================================

QString CsaMoveConverter::csaToPretty(const QString& csaMove, bool isPromotion, BoardSquare previousTo)
{
    if (csaMove.length() < 7) {
        return csaMove;
    }

    const Turn mover = (csaMove[0] == QLatin1Char('+')) ? Turn::Black : Turn::White;
    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    const int toFile = csaMove[3].digitValue();
    const int toRank = csaMove[4].digitValue();
    const bool validTo = isValidBoardCoordinate(toFile, toRank);
    const bool validFrom = isValidDropSource(fromFile, fromRank)
        || isValidBoardCoordinate(fromFile, fromRank);
    if (!validTo || !validFrom) {
        return csaMove;
    }

    // CSA の駒種は着手後の駒。成る手は成る前の駒名で書く
    const Piece after = ShogiNotation::pieceFromCsaCode(csaMove.mid(5, 2), mover);
    const Piece moving = isPromotion ? demote(after) : after;
    if (moving == Piece::None) {
        return csaMove;
    }

    UsiMove move;
    move.to = BoardSquare{toFile, toRank};
    if (fromFile == 0 && fromRank == 0) {
        move.drop = toBlack(moving);
    } else {
        move.from = BoardSquare{fromFile, fromRank};
        move.promote = isPromotion;
    }
    // 「同」は前の指し手の移動先と同じときだけ（初手では書かない）
    return KifMoveText::format(mover, moving, move, previousTo);
}

// ============================================================
// 対局結果文字列変換
// ============================================================

QString CsaMoveConverter::gameResultToString(CsaClient::GameResult result)
{
    switch (result) {
    case CsaClient::GameResult::Win:      return tr("勝ち");
    case CsaClient::GameResult::Lose:     return tr("負け");
    case CsaClient::GameResult::Draw:     return tr("引き分け");
    case CsaClient::GameResult::Censored: return tr("打ち切り");
    case CsaClient::GameResult::Chudan:   return tr("中断");
    default:                              return tr("不明");
    }
}

QString CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause cause)
{
    switch (cause) {
    case CsaClient::GameEndCause::Resign:         return tr("投了");
    case CsaClient::GameEndCause::TimeUp:         return tr("時間切れ");
    case CsaClient::GameEndCause::IllegalMove:    return tr("反則");
    case CsaClient::GameEndCause::Sennichite:     return tr("千日手");
    case CsaClient::GameEndCause::OuteSennichite: return tr("連続王手の千日手");
    case CsaClient::GameEndCause::Jishogi:        return tr("入玉宣言");
    case CsaClient::GameEndCause::MaxMoves:       return tr("手数制限");
    case CsaClient::GameEndCause::Chudan:         return tr("中断");
    case CsaClient::GameEndCause::IllegalAction:  return tr("不正行為");
    default:                                      return tr("不明");
    }
}

// ============================================================
// 盤面操作
// ============================================================

std::optional<RecordEntry> CsaMoveConverter::applyMoveToBoard(const QString& csaMove, ShogiGameController* gc)
{
    if (!gc || !gc->board() || csaMove.length() < 7) return std::nullopt;

    const QChar turnSign = csaMove[0];
    if (turnSign != QLatin1Char('+') && turnSign != QLatin1Char('-')) return std::nullopt;
    const Turn mover = (turnSign == QLatin1Char('+')) ? Turn::Black : Turn::White;
    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    const int toFile = csaMove[3].digitValue();
    const int toRank = csaMove[4].digitValue();
    ShogiBoard* board = gc->board();

    // CSA の駒種は着手後の駒なので、成ったかどうかは移動元の駒と比べて決める
    UsiMove move;
    move.to = BoardSquare{toFile, toRank};
    if (isValidDropSource(fromFile, fromRank)) {
        const Piece dropPiece = csaPieceToSfenPiece(csaMove.mid(5, 2), mover == Turn::Black);
        if (dropPiece == Piece::None) return std::nullopt;
        move.drop = toBlack(dropPiece);
    } else {
        move.from = BoardSquare{fromFile, fromRank};
        move.promote = isPromotionMove(csaMove, board);
    }
    if (!move.isValid() || mover != gc->currentPlayer()
        || usiToCsa(move.toString(), mover == Turn::Black, board) != csaMove.left(7)) return std::nullopt;
    return gc->applyRecordedMove(move);
}
