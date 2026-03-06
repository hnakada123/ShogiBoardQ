/// @file csamoveconverter_game.cpp
/// @brief CsaMoveConverter の表示・終局文言・盤面適用実装

#include "csamoveconverter.h"
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

bool isPromotedCsaPiece(const QString& piece)
{
    return piece == QStringLiteral("TO") || piece == QStringLiteral("NY")
        || piece == QStringLiteral("NK") || piece == QStringLiteral("NG")
        || piece == QStringLiteral("UM") || piece == QStringLiteral("RY");
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

    QChar turnSign = csaMove[0];
    QString turnMark = (turnSign == QLatin1Char('+')) ? QStringLiteral("▲") : QStringLiteral("△");

    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);
    const bool validTo = isValidBoardCoordinate(toFile, toRank);
    const bool validFrom = isValidDropSource(fromFile, fromRank)
        || isValidBoardCoordinate(fromFile, fromRank);
    if (!validTo || !validFrom) {
        return csaMove;
    }

    static const QString zenFile[] = {
        QString(), QStringLiteral("１"), QStringLiteral("２"), QStringLiteral("３"),
        QStringLiteral("４"), QStringLiteral("５"), QStringLiteral("６"),
        QStringLiteral("７"), QStringLiteral("８"), QStringLiteral("９")
    };
    static const QString kanjiRank[] = {
        QString(), QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };

    // 成りの場合は成る前の駒名に変換
    QString displayPiece = piece;
    if (isPromotion) {
        static const QHash<QString, QString> unpromoteMap = {
            {QStringLiteral("TO"), QStringLiteral("FU")},
            {QStringLiteral("NY"), QStringLiteral("KY")},
            {QStringLiteral("NK"), QStringLiteral("KE")},
            {QStringLiteral("NG"), QStringLiteral("GI")},
            {QStringLiteral("UM"), QStringLiteral("KA")},
            {QStringLiteral("RY"), QStringLiteral("HI")}
        };
        displayPiece = unpromoteMap.value(piece, piece);
    }

    QString pieceKanji = csaPieceToKanji(displayPiece);

    QString moveStr = turnMark;

    // 「同」の判定（前の指し手の移動先と同じ場合）
    if (toFile == prevToFile && toRank == prevToRank && moveCount > 0) {
        moveStr += QStringLiteral("同　") + pieceKanji;
    } else {
        moveStr += zenFile[toFile] + kanjiRank[toRank] + pieceKanji;
    }

    // 成る手の場合は「成」を追加
    if (isPromotion) {
        moveStr += QStringLiteral("成");
    }

    // 駒打ちの場合
    if (fromFile == 0 && fromRank == 0) {
        moveStr += QStringLiteral("打");
    } else {
        moveStr += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank)
                + QStringLiteral(")");
    }

    return moveStr;
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

    const QString usiMove = csaToUsi(csaMove);
    if (usiMove.isEmpty()) {
        return false;
    }

    const QChar turnSign = csaMove[0];
    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    const int toFile = csaMove[3].digitValue();
    const int toRank = csaMove[4].digitValue();
    const QString piece = csaMove.mid(5, 2);
    if (turnSign != QLatin1Char('+') && turnSign != QLatin1Char('-')) {
        return false;
    }
    if (!isValidBoardCoordinate(toFile, toRank)) {
        return false;
    }

    const bool isPromoted = isPromotedCsaPiece(piece);

    ShogiBoard* board = gc->board();

    if (!isValidDropSource(fromFile, fromRank)) {
        if (!isValidBoardCoordinate(fromFile, fromRank)) {
            return false;
        }
        // 通常移動
        const Piece selectedPiece = board->pieceCharacter(fromFile, fromRank);
        if (selectedPiece == Piece::None) {
            return false;
        }
        const Piece capturedPiece = board->pieceCharacter(toFile, toRank);

        if (capturedPiece != Piece::None) {
            board->addPieceToStand(capturedPiece);
        }

        board->movePieceToSquare(selectedPiece, fromFile, fromRank, toFile, toRank, isPromoted);
    } else {
        // 駒打ち
        const Piece dropPiece = csaPieceToSfenPiece(piece, turnSign == QLatin1Char('+'));
        if (dropPiece == Piece::None) {
            return false;
        }
        if (!board->decrementPieceOnStand(dropPiece)) {
            return false;
        }
        board->movePieceToSquare(dropPiece, 0, 0, toFile, toRank, false);
    }

    gc->changeCurrentPlayer();

    // USI形式の指し手をリストに追加
    usiMoves.append(usiMove);

    // SFEN記録を更新
    QString boardSfen = board->convertBoardToSfen();
    QString standSfen = board->convertStandToSfen();
    QString currentPlayerStr = (gc->currentPlayer() == ShogiGameController::Player1)
                               ? QStringLiteral("b") : QStringLiteral("w");
    QString fullSfen = QString("%1 %2 %3 %4")
                           .arg(boardSfen, currentPlayerStr, standSfen)
                           .arg(moveCount + 1);
    if (sfenHistory) {
        sfenHistory->append(fullSfen);
    }

    return true;
}
