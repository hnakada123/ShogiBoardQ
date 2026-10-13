/// @file csamoveconverter_game.cpp
/// @brief CsaMoveConverter の表示・終局文言・盤面適用実装

#include "csamoveconverter.h"
#include "usimove.h"
#include "kifmovetext.h"
#include "logcategories.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include <QHash>

namespace {
bool isValidBoardCoordinate(int file, int rank)
{
    return file >= 1 && file <= 9 && rank >= 1 && rank <= 9;
}

bool isValidDropSource(int file, int rank)
{
    return file == 0 && rank == 0;
}
} // namespace

// ============================================================
// 表示用変換
// ============================================================

QString CsaMoveConverter::csaToPretty(const QString& csaMove, bool isPromotion,
                                      int prevToFile, int prevToRank, int moveCount)
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
    const BoardSquare previousTo = (moveCount > 0) ? BoardSquare{prevToFile, prevToRank} : BoardSquare{};
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

bool CsaMoveConverter::applyMoveToBoard(const QString& csaMove, ShogiGameController* gc,
                                        QStringList& usiMoves, QStringList* sfenHistory,
                                        int moveCount)
{
    if (!gc || !gc->board()) {
        return false;
    }

    if (csaMove.length() < 7) {
        return false;
    }

    const QChar turnSign = csaMove[0];
    if (turnSign != QLatin1Char('+') && turnSign != QLatin1Char('-')) {
        return false;
    }
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
        if (dropPiece == Piece::None) return false;
        move.drop = toBlack(dropPiece);
    } else {
        move.from = BoardSquare{fromFile, fromRank};
        move.promote = isPromotionMove(csaMove, board);
    }
    if (!move.isValid() || !board->applyUsiMove(move, mover)) {
        return false;
    }
    const QString usiMove = move.toString();

    gc->changeCurrentPlayer();

    // USI形式の指し手をリストに追加
    usiMoves.append(usiMove);

    // SFEN記録を更新
    QString boardSfen = board->convertBoardToSfen();
    QString standSfen = board->convertStandToSfen();
    QString currentPlayerStr = (gc->currentPlayer() == Turn::Black)
                               ? QStringLiteral("b") : QStringLiteral("w");
    QString fullSfen = QString("%1 %2 %3 %4")
                           .arg(boardSfen, currentPlayerStr, standSfen)
                           .arg(moveCount + 2);
    if (sfenHistory) {
        sfenHistory->append(fullSfen);
    }

    return true;
}
