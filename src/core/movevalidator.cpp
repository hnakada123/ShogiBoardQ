/// @file movevalidator.cpp
/// @brief 指し手の合法性判定・ビットボードによる合法手生成の実装
/// @todo remove コメントスタイルガイド適用済み

#include "movevalidator.h"
#include "shogimove.h"
#include "legalmovestatus.h"

#include <iostream>
#include <iomanip>
#include <QMap>
#include <array>
#include <QDebug>

// ============================================================
// 初期化
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
MoveValidator::MoveValidator(QObject* parent) : QObject(parent)
{
    m_allPieces = {'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K', 'Q', 'M', 'O', 'T', 'C', 'U',
                   'p', 'l', 'n', 's', 'g', 'b', 'r', 'k', 'q', 'm', 'o', 't', 'c', 'u'};

    m_pieceOrderBlack = {{'P', 0}, {'L', 1}, {'N', 2}, {'S', 3}, {'G', 4}, {'B', 5}, {'R', 6}};
    m_pieceOrderWhite = {{'r', 2}, {'b', 3}, {'g', 4}, {'s', 5}, {'n', 6}, {'l', 7}, {'p', 8}};
}

// ============================================================
// 盤面バリデーション
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::checkDoublePawn(const QVector<QChar>& boardData)
{
    for (int file = 0; file < BOARD_SIZE; ++file) {
        int pawnCountBlack = 0;
        int pawnCountWhite = 0;
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            int index = rank * BOARD_SIZE + file;
            if (boardData[index] == 'P') ++pawnCountBlack;
            if (boardData[index] == 'p') ++pawnCountWhite;
        }

        if (pawnCountBlack > 1 || pawnCountWhite > 1) {
            const QString errorMessage = tr("An error occurred in MoveValidator::checkDoublePawn. There is a double pawn situation.");
            emit errorOccurred(errorMessage);
            return;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::checkPieceCount(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    QMap<QChar, int> currentPieceCount;

    for (auto piece : boardData) {
        if(piece != ' ') {
            currentPieceCount[piece]++;
        }
    }

    for (auto it = pieceStand.begin(); it != pieceStand.end(); ++it) {
        currentPieceCount[it.key()] += it.value();
    }

    static const QMap<QChar, int> maxTotalPieceCount = {
        {'P', 18}, {'L', 4}, {'N', 4}, {'S', 4}, {'G', 4}, {'B', 2}, {'R', 2}, {'K', 1},
        {'p', 18}, {'l', 4}, {'n', 4}, {'s', 4}, {'g', 4}, {'b', 2}, {'r', 2}, {'k', 1}
    };

    for (auto it = maxTotalPieceCount.begin(); it != maxTotalPieceCount.end(); ++it) {
        if (currentPieceCount[it.key()] > it.value()) {
            const QString errorMessage = tr("An error occurred in MoveValidator::checkPieceCount. The number of pieces exceeds the maximum allowed.");
            emit errorOccurred(errorMessage);
            return;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::checkKingPresence(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    Q_UNUSED(pieceStand);  // 玉は持ち駒にできないため使用しない

    int kingCountBlack = 0;
    int kingCountWhite = 0;

    for (const auto& piece : std::as_const(boardData)) {
        if (piece == 'K') ++kingCountBlack;
        if (piece == 'k') ++kingCountWhite;
    }

    if ((kingCountBlack != 1) || (kingCountWhite != 1)) {
        const QString errorMessage = tr("An error occurred in MoveValidator::checkKingPresence. There is not exactly one king per player.");
        emit errorOccurred(errorMessage);
        return;
    }
}

// 歩/香/桂が行き場のない段に不成で存在しないか検証する。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::checkCorrectPosition(const QVector<QChar>& boardData)
{
    for (qsizetype i = 0; i < boardData.size(); ++i) {
        QChar piece = boardData[i];
        int rank = static_cast<int>(i) / BOARD_SIZE + 1;

        if ((piece == 'p' && rank == BOARD_SIZE) || (piece == 'l' && rank == BOARD_SIZE) ||
            (piece == 'n' && (rank == 8 || rank == BOARD_SIZE)) ||
            (piece == 'P' && rank == 1) || (piece == 'L' && rank == 1) ||
            (piece == 'N' && (rank == 1 || rank == 2))) {
            const QString errorMessage = tr("An error occurred in MoveValidator::checkCorrectPosition. A piece that should be promoted is in an incorrect position.");
            emit errorOccurred(errorMessage);
            return;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::validateBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    checkDoublePawn(boardData);
    checkPieceCount(boardData, pieceStand);
    checkCorrectPosition(boardData);
    checkKingPresence(boardData, pieceStand);
}

// ============================================================
// 指し手バリデーション
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::validateMoveFileValue(int fromSquareX)
{
    // 筋の値が10より大きい場合は無効（10=先手駒台, 11=後手駒台まで有効）
    if (fromSquareX > 10) {
        const QString errorMessage = tr("An error occurred in MoveValidator::validateMoveFileValue. Validation Error: The file value of the move is incorrect.");
        qDebug() << "Move file value: " << fromSquareX;
        emit errorOccurred(errorMessage);
        return;
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::validateMovingPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData)
{
    if (currentMove.fromSquare.x() < BOARD_SIZE) {
        if (currentMove.fromSquare.y() < 0 || currentMove.fromSquare.y() >= BOARD_SIZE) {
            const QString errorMessage = tr("An error occurred in MoveValidator::validateMovingPiece. Validation Error: The rank value of the move is out of bounds.");
            qDebug() << "Move rank value: " << currentMove.fromSquare.y();
            emit errorOccurred(errorMessage);
            return;
        }

        int fromIndex = currentMove.fromSquare.y() * BOARD_SIZE + currentMove.fromSquare.x();

        if (fromIndex < 0 || fromIndex >= boardData.size()) {
            const QString errorMessage = tr("An error occurred in MoveValidator::validateMovingPiece. Validation Error: The board index is out of bounds.");
            qDebug() << "Board index: " << fromIndex << ", Board data size: " << boardData.size();
            emit errorOccurred(errorMessage);
            return;
        }

        if (currentMove.movingPiece != boardData[fromIndex]) {
            const QString errorMessage = tr("An error occurred in MoveValidator::validateMovingPiece. Validation Error: The piece in the move does not match the piece on the square.");
            qDebug() << "Move piece: " << currentMove.movingPiece;
            qDebug() << "Piece on the square: " << boardData[fromIndex];
            emit errorOccurred(errorMessage);
            return;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::logAndShowPieceStandError(const QString& errorMessage, QChar piece, const QMap<QChar, int>& pieceStand)
{
    qWarning() << errorMessage;
    qDebug() << "Piece: " << piece;
    qDebug() << "Number of pieces in the stand: " << pieceStand[piece];
    emit errorOccurred(errorMessage);
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::validatePieceStand(Turn turn, const ShogiMove& currentMove, const QMap<QChar, int>& pieceStand)
{
    // 先手の駒台から打つ場合
    if (currentMove.fromSquare.x() == BOARD_SIZE) {
        if (turn == BLACK) {
            if (pieceStand[currentMove.movingPiece] <= 0) {
                const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. The number of pieces in the stand of the player moving is not positive.");
                logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
                return;
            }
        } else {
            // 後手なのに先手の駒台から打とうとしている
            const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. It's White's turn, but trying to drop a piece from Black's piece stand.");
            logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
            return;
        }
    }
    // 後手の駒台から打つ場合
    else if (currentMove.fromSquare.x() == BOARD_SIZE + 1) {
        if (turn == WHITE) {
            if (pieceStand[currentMove.movingPiece] <= 0) {
                const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. The number of pieces on White's piece stand to drop is not positive.");
                logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
                return;
            }
        } else {
            // 先手なのに後手の駒台から打とうとしている
            const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. It's Black's turn, but trying to drop a piece from White's piece stand.");
            logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
            return;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::validateCapturedPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData)
{
    int toIndex = currentMove.toSquare.y() * BOARD_SIZE + currentMove.toSquare.x();

    if (currentMove.capturedPiece != boardData[toIndex]) {
        const QString errorMessage = tr("An error occurred in MoveValidator::validateCapturedPiece. The captured piece does not match the piece on the destination square of the board.");
        qWarning() << errorMessage;
        qDebug() << "Captured piece: " << currentMove.capturedPiece;
        qDebug() << "Piece on the square: " << boardData[toIndex];
        emit errorOccurred(errorMessage);
        return;
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::validateMoveComponents(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove)
{
    validateBoardAndPieces(boardData, pieceStand);
    validateMoveFileValue(currentMove.fromSquare.x());
    validateMovingPiece(currentMove, boardData);
    validatePieceStand(turn, currentMove, pieceStand);
    validateCapturedPiece(currentMove, boardData);
}

// ============================================================
// 合法手判定メイン
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
LegalMoveStatus MoveValidator::isLegalMove(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, ShogiMove& currentMove)
{
    // 処理フロー:
    // 1. 指し手の各要素を検証
    // 2. ビットボードを生成（駒配置・個別駒・攻撃範囲）
    // 3. 相手玉への王手状態を確認（既に王手なら不正局面）
    // 4. 自玉への王手状態を確認
    // 5. 合法手を判定して結果を返す

    validateMoveComponents(turn, boardData, pieceStand, currentMove);

    // 28種類の駒配置ビットボードを生成
    BoardStateArray piecePlacedBitboards;
    generateBitboard(boardData, piecePlacedBitboards);

    // 各個別駒のビットボードを生成
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    generateAllPieceBitboards(allPieceBitboards, boardData);

    // 各個別駒の攻撃範囲ビットボードを生成
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;
    generateAllIndividualPieceAttackBitboards(allPieceBitboards, piecePlacedBitboards, allPieceAttackBitboards);

    // 王手回避に必要なマスを表すビットボード
    std::bitset<NUM_BOARD_SQUARES> necessaryMovesBitboard;

    // 玉を除いた攻撃範囲のビットボード
    std::bitset<NUM_BOARD_SQUARES> attackWithoutKingBitboard;

    Turn opponentTurn = (turn == BLACK) ? WHITE : BLACK;

    // 局面がすでに相手玉に王手をかけている状態かどうかを確認
    int numOpponentChecks = isKingInCheck(opponentTurn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    // 王手をかけている局面でさらに指すのは不正
    if (numOpponentChecks) {
        const QString errorMessage = tr("An error occurred in MoveValidator::isLegalMove. Validation Error: The position is already in check. Making another move is incorrect.");
        qDebug() << "numOpponentChecks = " << numOpponentChecks;
        printBoardAndPieces(boardData, pieceStand);
        emit errorOccurred(errorMessage);

        LegalMoveStatus legalMoveStatus;
        legalMoveStatus.promotingMoveExists = false;
        legalMoveStatus.nonPromotingMoveExists = false;
        return legalMoveStatus;
    }

    // 自玉が王手されているかどうかを確認
    int numChecks = isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    QVector<ShogiMove> legalMovesList;

    return validateMove(numChecks, boardData, currentMove, turn, piecePlacedBitboards, pieceStand, necessaryMovesBitboard, legalMovesList,
                        opponentTurn, attackWithoutKingBitboard);
}

// ============================================================
// 盤上の駒移動の合法手判定
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
LegalMoveStatus MoveValidator::isBoardMoveValid(const Turn& turn, const QVector<QChar>& boardData, const ShogiMove& currentMove,
                                                const int& numChecks, const QChar& piece, const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                                const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard, const std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                                QVector<ShogiMove>& legalMovesList)
{
    QVector<ShogiMove> allBoardPieceMovesList;

    generateShogiMoveFromBitboard(piece, pieceBitboard, attackBitboard, allBoardPieceMovesList, boardData);

    // 王手の有無により考慮すべき指し手を絞り込む
    QVector<ShogiMove> relevantMovesList;

    if (numChecks) {
        // 両王手の場合、玉以外の駒では対処できない
        if (numChecks == 2 && currentMove.movingPiece != ((turn == BLACK) ? 'K' : 'k')) {
            const QString errorMessage = tr("An error occurred in MoveValidator::isBoardMoveValid. The piece in the move is not a king.");
            qDebug() << "Piece: " << currentMove.movingPiece;
            emit errorOccurred(errorMessage);
            LegalMoveStatus legalMoveStatus;
            legalMoveStatus.promotingMoveExists = false;
            legalMoveStatus.nonPromotingMoveExists = false;
            return legalMoveStatus;
        }

        // 王手回避手を抽出
        filterMovesThatBlockThreat(turn, allBoardPieceMovesList, necessaryMovesBitboard, relevantMovesList);
    } else {
        relevantMovesList = allBoardPieceMovesList;
    }

    filterLegalMovesList(turn, relevantMovesList, boardData, legalMovesList);

    return checkLegalMoveStatus(currentMove, legalMovesList);
}

// ============================================================
// 駒台からの打ち手の合法手判定
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::isHandPieceMoveValid(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                                         const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList)
{
    switch(numChecks) {
    case 0:
        if (!generateLegalMovesForPiece(turn, boardData, pieceStand, currentMove, emptySquareBitboard, legalMovesList)) {
            return false;
        }
        break;

    case 1: {
        QVector<ShogiMove> candidateLegalMovesList;

        if (!generateLegalMovesForPiece(turn, boardData, pieceStand, currentMove, emptySquareBitboard, candidateLegalMovesList)) {
            return false;
        }

        filterLegalMovesList(turn, candidateLegalMovesList, boardData, legalMovesList);
        break;
    }

    case 2: {
        // 両王手の場合、駒台から駒を打つ手はあり得ない
        const QString errorMessage = tr("An error occurred in MoveValidator::isHandPieceMoveValid. In the case of double check, dropping a piece from the piece stand is not possible.");
        emit errorOccurred(errorMessage);
        return false;
    }

    default: {
        // 王手数が3以上は不正局面
        const QString errorMessage = tr("An error occurred in MoveValidator::isHandPieceMoveValid. The number of pieces putting the player's king in check is 3 or more.");
        qDebug() << "Number of pieces putting the player's king in check: " << numChecks;
        emit errorOccurred(errorMessage);
        return false;
    }
    }

    return isMoveInList(currentMove, legalMovesList);
}

/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::generateLegalMovesForPiece(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                                               const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList)
{
    QChar dropPiece = currentMove.movingPiece;

    if (pieceStand[dropPiece] <= 0) {
        const QString errorMessage = tr("An error occurred in MoveValidator::generateLegalMovesForPiece. The piece to be dropped does not exist in the piece stand.");
        qDebug() << "Piece: " << dropPiece;
        emit errorOccurred(errorMessage);
        return false;
    }

    switch (dropPiece.toUpper().toLatin1()) {
    case 'P':
        generateDropMoveForPawn(legalMovesList, pieceStand, emptySquareBitboard, turn, boardData);
        break;

    case 'L':
    case 'N':
    case 'S':
    case 'G':
    case 'B':
    case 'R':
        generateDropMoveForPiece(legalMovesList, pieceStand, emptySquareBitboard, turn, dropPiece);
        break;

    default:
        const QString errorMessage = tr("An error occurred in MoveValidator::generateLegalMovesForPiece. The character representing the piece to be dropped is incorrect.");
        qDebug() << "Piece: " << dropPiece;
        emit errorOccurred(errorMessage);
        return false;
    }

    return true;
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateDropMoveForPawn(QVector<ShogiMove>& legalMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                            const Turn& turn, const QVector<QChar>& boardData) const
{
    // 二歩回避のため、歩が無い筋のビットボードを生成してAND演算する
    std::bitset<NUM_BOARD_SQUARES> emptyFilePawnBitboard;
    generateBitboardForEmptyFiles(turn, boardData, emptyFilePawnBitboard);

    std::bitset<NUM_BOARD_SQUARES> emptySquaresNoPawnsFilesBitboard;
    performAndOperation(emptySquareBitboard, emptyFilePawnBitboard, emptySquaresNoPawnsFilesBitboard);

    generateDropPawnMoves(legalMovesList, pieceStand, emptySquaresNoPawnsFilesBitboard, turn);
}

// ============================================================
// 全合法手生成
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
int MoveValidator::generateLegalMoves(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    // 処理フロー:
    // 1. 盤面バリデーション
    // 2. ビットボード生成（駒配置・個別駒・攻撃範囲）
    // 3. 王手状態に応じて盤上の駒の合法手を生成
    // 4. 駒台から打つ手の合法手を生成
    // 5. 合法手の総数を返す

    validateBoardAndPieces(boardData, pieceStand);

    BoardStateArray piecePlacedBitboards;
    generateBitboard(boardData, piecePlacedBitboards);

    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    generateAllPieceBitboards(allPieceBitboards, boardData);

    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;
    generateAllIndividualPieceAttackBitboards(allPieceBitboards, piecePlacedBitboards, allPieceAttackBitboards);

    // 盤上の駒の全指し手リスト
    QVector<ShogiMove> allBoardPieceMovesList;
    generateShogiMoveFromBitboards(turn, allPieceBitboards, allPieceAttackBitboards, allBoardPieceMovesList, boardData);

    std::bitset<MoveValidator::NUM_BOARD_SQUARES> necessaryMovesBitboard;
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> attackWithoutKingBitboard;

    Turn opponentTurn = (turn == BLACK) ? WHITE : BLACK;

    // 既に相手玉に王手をかけている不正局面を検出
    int numOpponentChecks = isKingInCheck(opponentTurn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    if (numOpponentChecks) {
        const QString errorMessage = tr("An error occurred in MoveValidator::generateLegalMoves. The position is already in check. Making another move is incorrect.");
        printBoardAndPieces(boardData, pieceStand);
        emit errorOccurred(errorMessage);
        return 0;
    }

    int numChecks = isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    QVector<ShogiMove> legalMovesList;

    // 駒台から打てる空きマスのビットボード
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> emptySquareBitboard;

    if (numChecks) {
        // 王手回避手の候補を生成
        QVector<ShogiMove> kingBlockingMovesList;
        filterMovesThatBlockThreat(turn, allBoardPieceMovesList, necessaryMovesBitboard, kingBlockingMovesList);

        // 自玉に王手が掛かっていない手だけを合法手に
        filterLegalMovesList(turn, kingBlockingMovesList, boardData, legalMovesList);

        if (numChecks == 2) {
            // 両王手の場合、合い駒は不可。玉を動かす手のみ
            return static_cast<int>(legalMovesList.size());
        }

        // 駒台から打てるマスは王手筋上のマスのみ
        emptySquareBitboard = attackWithoutKingBitboard;
    } else {
        filterLegalMovesList(turn, allBoardPieceMovesList, boardData, legalMovesList);
        generateEmptySquareBitboard(boardData, emptySquareBitboard);
    }

    generateDropNonPawnMoves(legalMovesList, pieceStand, emptySquareBitboard, turn);
    generateDropMoveForPawn(legalMovesList, pieceStand, emptySquareBitboard, turn, boardData);

    return static_cast<int>(legalMovesList.size());
}

// ============================================================
// ビットボード生成
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateBitboard(const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboard) const
{
    for (qsizetype i = 0; i < boardData.size(); ++i) {
        QChar piece = boardData.at(i);
        Turn turnPiece = piece.isUpper() ? BLACK : WHITE;
        piece = piece.toUpper();

        switch (piece.unicode()) {
        case 'P':
            piecePlacedBitboard[turnPiece][PAWN].set(static_cast<size_t>(i), true);
            break;
        case 'L':
            piecePlacedBitboard[turnPiece][LANCE].set(static_cast<size_t>(i), true);
            break;
        case 'N':
            piecePlacedBitboard[turnPiece][KNIGHT].set(static_cast<size_t>(i), true);
            break;
        case 'S':
            piecePlacedBitboard[turnPiece][SILVER].set(static_cast<size_t>(i), true);
            break;
        case 'G':
            piecePlacedBitboard[turnPiece][GOLD].set(static_cast<size_t>(i), true);
            break;
        case 'B':
            piecePlacedBitboard[turnPiece][BISHOP].set(static_cast<size_t>(i), true);
            break;
        case 'R':
            piecePlacedBitboard[turnPiece][ROOK].set(static_cast<size_t>(i), true);
            break;
        case 'K':
            piecePlacedBitboard[turnPiece][KING].set(static_cast<size_t>(i), true);
            break;
        case 'Q':
            piecePlacedBitboard[turnPiece][PROMOTED_PAWN].set(static_cast<size_t>(i), true);
            break;
        case 'M':
            piecePlacedBitboard[turnPiece][PROMOTED_LANCE].set(static_cast<size_t>(i), true);
            break;
        case 'O':
            piecePlacedBitboard[turnPiece][PROMOTED_KNIGHT].set(static_cast<size_t>(i), true);
            break;
        case 'T':
            piecePlacedBitboard[turnPiece][PROMOTED_SILVER].set(static_cast<size_t>(i), true);
            break;
        case 'C':
            piecePlacedBitboard[turnPiece][HORSE].set(static_cast<size_t>(i), true);
            break;
        case 'U':
            piecePlacedBitboard[turnPiece][DRAGON].set(static_cast<size_t>(i), true);
            break;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::isPieceOnSquare(int index, const std::array<std::bitset<NUM_BOARD_SQUARES>, NUM_PIECE_TYPES>& turnBitboards) const
{
    for (int pieceType = 0; pieceType < NUM_PIECE_TYPES; ++pieceType) {
        if (turnBitboards[static_cast<size_t>(pieceType)].test(static_cast<size_t>(index))) {
            return true;
        }
    }

    return false;
}

/// @todo remove コメントスタイルガイド適用済み
QVector<std::bitset<MoveValidator::NUM_BOARD_SQUARES>> MoveValidator::generateIndividualPieceBitboards(const QVector<QChar>& boardData, const QChar& targetPiece) const
{
    QVector<std::bitset<NUM_BOARD_SQUARES>> individualPieceBitboards;

    for (qsizetype i = 0; i < boardData.size(); ++i) {
        QChar piece = boardData.at(i);

        if (piece == targetPiece) {
            std::bitset<NUM_BOARD_SQUARES> pieceBitboard;

            pieceBitboard.set(static_cast<size_t>(i), true);

            individualPieceBitboards.append(pieceBitboard);
        }
    }

    return individualPieceBitboards;
}

// 全28種類の駒について、各個別駒のビットボードを生成する。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateAllPieceBitboards(QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const QVector<QChar>& boardData) const
{
    allPieceBitboards.clear();

    for (qsizetype i = 0; i < m_allPieces.size(); ++i) {
        const QChar& targetPiece = m_allPieces.at(i);
        allPieceBitboards.append(generateIndividualPieceBitboards(boardData, targetPiece));
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateAllIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const BoardStateArray& piecePlacedBitboard,
                                                              QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const
{
    allPieceAttackBitboards.resize(allPieceBitboards.size());

    for (qsizetype i = 0; i < allPieceBitboards.size(); ++i) {
        allPieceAttackBitboards[i].clear();
        QChar piece = m_allPieces.at(i);
        Turn turnPiece = piece.isUpper() ? BLACK : WHITE;
        piece = piece.toUpper();
        for (qsizetype j = 0; j < allPieceBitboards[i].size(); ++j) {
            std::bitset<MoveValidator::NUM_BOARD_SQUARES> attackBitboard;

            generateSinglePieceAttackBitboard(attackBitboard, allPieceBitboards[i].at(j), turnPiece, piece, piecePlacedBitboard);

            allPieceAttackBitboards[i].append(attackBitboard);
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateBitboardForEmptyFiles(const Turn& turn, const QVector<QChar>& boardData, std::bitset<NUM_BOARD_SQUARES>& emptyFilePawnBitboard) const
{
    const QChar pawn = (turn == BLACK) ? 'P' : 'p';
    std::array<bool, 9> fileContainsPawn = {false, false, false, false, false, false, false, false, false};

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            if(boardData[index] == pawn) {
                fileContainsPawn[static_cast<size_t>(file)] = true;
            }
        }
    }

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            emptyFilePawnBitboard[static_cast<size_t>(index)] = fileContainsPawn[static_cast<size_t>(file)] ? 0 : 1;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateEmptySquareBitboard(const QVector<QChar>& boardData, std::bitset<MoveValidator::NUM_BOARD_SQUARES>& emptySquareBitboard) const
{
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            if (boardData[index] == ' ') {
                emptySquareBitboard.set(static_cast<size_t>(index));
            }
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::performAndOperation(const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const std::bitset<NUM_BOARD_SQUARES>& emptyFilePawnBitboard,
                         std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard) const
{
    emptySquaresNoPawnsFilesBitboard = emptySquareBitboard & emptyFilePawnBitboard;
}

// ============================================================
// 攻撃範囲ビットボード生成
// ============================================================

// 各駒の移動方向に基づいて攻撃可能マスのビットボードを生成する。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateSinglePieceAttackBitboard(std::bitset<MoveValidator::NUM_BOARD_SQUARES>& attackBitboard,
                                                      const std::bitset<MoveValidator::NUM_BOARD_SQUARES>& singlePieceBitboard,
                                                      const Turn& turn, QChar pieceType, const BoardStateArray& piecePlacedBitboard) const
{
    for (int index = 0; index < NUM_BOARD_SQUARES; ++index) {
        if (singlePieceBitboard.test(static_cast<size_t>(index))) {
            switch (pieceType.toUpper().toLatin1()) {
            case 'P':
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, 0}}, false, false);
                }
                break;
            case 'L':
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}}, true, true);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, 0}}, true, true);
                }
                break;
            case 'N':
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-2, -1}, {-2, 1}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{2, -1}, {2, 1}}, false, false);
                }
                break;
            case 'S':
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 0}, {-1, 1}, {1, -1}, {1, 1}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, -1}, {1, 0}, {1, 1}, {-1, -1}, {-1, 1}}, false, false);
                }
                break;
            case 'G':
            case 'Q':
            case 'M':
            case 'O':
            case 'T':
                // 金と金と同じ動きの成駒（と金、成香、成桂、成銀）
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, 0}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, -1}, {1, 0}, {1, 1}, {0, -1}, {0, 1}, {-1, 0}}, false, false);
                }
                break;
            case 'B':
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}, true, true);
                break;
            case 'R':
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}, true, true);
                break;
            case 'K':
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}}, false, false);
                break;
            case 'C':
                // 馬: 角の動き＋上下左右1マス
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}, true, true);
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}, false, false);
                break;
            case 'U':
                // 龍: 飛車の動き＋斜め1マス
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}, true, true);
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}, false, false);
                break;
            default:
                break;
            }
        }
    }
}

// 指定方向への攻撃範囲ビットボードを生成する。
// continuous: 飛車・角・馬・龍など連続移動する駒かどうか
// enemyOccupiedStop: 敵駒に到達したら停止するか
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateAttackBitboard(std::bitset<NUM_BOARD_SQUARES>& attackBitboard, const Turn& turn,
                                           const BoardStateArray& piecePlacedBitboard, const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard,
                                           const QList<QPoint>& directions, bool continuous, bool enemyOccupiedStop) const
{
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;

            if (pieceBitboard.test(static_cast<size_t>(index))) {
                for (const auto& direction : std::as_const(directions)) {
                    int currentRank = rank;
                    int currentFile = file;
                    int opponent = 1 - turn;

                    while (true) {
                        currentRank += direction.x();
                        currentFile += direction.y();

                        if (currentRank >= 0 && currentRank < BOARD_SIZE &&
                            currentFile >= 0 && currentFile < BOARD_SIZE) {
                            int targetIndex = currentRank * BOARD_SIZE + currentFile;

                            // 味方の駒がある場所には移動できない
                            if (isPieceOnSquare(targetIndex, piecePlacedBitboard[turn])) {
                                break;
                            }

                            attackBitboard.set(static_cast<size_t>(targetIndex), true);

                            // 敵駒は取れるがその先には進めない
                            if (isPieceOnSquare(targetIndex, piecePlacedBitboard[static_cast<size_t>(opponent)])) {
                                if (enemyOccupiedStop) break;
                            }

                            if (!continuous) {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
}

// ============================================================
// 指し手生成
// ============================================================

// 盤上の全駒の指し手を一括生成する。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateShogiMoveFromBitboards(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                                   const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                                                   QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const
{
    int start = (turn == BLACK) ? P_IDX : p_IDX;
    int end = (turn == BLACK) ? U_IDX + 1 : u_IDX + 1;

    for (int i = start; i < end; ++i) {
        QChar piece = m_allPieces.at(i);

        for (qsizetype j = 0; j < allPieceBitboards[i].size(); ++j) {
            auto& bitboard = allPieceBitboards[i].at(j);
            auto& attackBitboard = allPieceAttackBitboards[i].at(j);
            ShogiMove move;

            for (int rank = 0; rank < BOARD_SIZE; ++rank) {
                for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                    int index = rank * BOARD_SIZE + file;
                    if (bitboard.test(static_cast<size_t>(index))) {
                        move.movingPiece = piece;
                        move.fromSquare = QPoint(file, rank);
                    }
                }
            }

            for (int rank = 0; rank < BOARD_SIZE; ++rank) {
                for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                    int index = rank * BOARD_SIZE + file;
                    if (attackBitboard.test(static_cast<size_t>(index))) {
                        move.toSquare = QPoint(file, rank);
                        move.capturedPiece = boardData.at(index);

                        // 行き場のない駒の不成は禁止
                        bool disallowedMove =
                                (piece == 'P' && rank == 0) ||
                                (piece == 'p' && rank == 8) ||
                                (piece == 'L' && rank == 0) ||
                                (piece == 'l' && rank == 8) ||
                                (piece == 'N' && (rank == 0 || rank == 1)) ||
                                (piece == 'n' && (rank == 7 || rank == 8));

                        if (!disallowedMove) {
                            allMovesList.append(move);
                        }

                        generateAllPromotions(move, allMovesList);
                    }
                }
            }
        }
    }
}

// 指定した1つの駒の指し手を生成する。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateShogiMoveFromBitboard(const QChar piece, const std::bitset<NUM_BOARD_SQUARES>& bitboard,
                                                  const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                                  QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const
{
    ShogiMove move;

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            if (bitboard.test(static_cast<size_t>(index))) {
                move.movingPiece = piece;
                move.fromSquare = QPoint(file, rank);
            }
        }
    }

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            if (attackBitboard.test(static_cast<size_t>(index))) {
                move.toSquare = QPoint(file, rank);
                move.capturedPiece = boardData.at(index);

                bool disallowedMove =
                    (piece == 'P' && rank == 0) ||
                    (piece == 'p' && rank == 8) ||
                    (piece == 'L' && rank == 0) ||
                    (piece == 'l' && rank == 8) ||
                    (piece == 'N' && (rank == 0 || rank == 1)) ||
                    (piece == 'n' && (rank == 7 || rank == 8));

                if (!disallowedMove) {
                    allMovesList.append(move);
                }

                generateAllPromotions(move, allMovesList);
            }
        }
    }
}

// ============================================================
// 成り手生成
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateAllPromotions(ShogiMove& move, QVector<ShogiMove>& promotions) const
{
    bool canPromote = false;
    int fromRank = move.fromSquare.y();
    int toRank = move.toSquare.y();

    if (move.movingPiece == 'P' || move.movingPiece == 'L' || move.movingPiece == 'N' || move.movingPiece == 'S' ||
        move.movingPiece == 'B' || move.movingPiece == 'R') {
        // 先手: 移動元か移動先が敵陣（1〜3段目）
        if (toRank <= 2 || fromRank <= 2) {
            canPromote = true;
        }
    } else if (move.movingPiece == 'p' || move.movingPiece == 'l' || move.movingPiece == 'n' || move.movingPiece == 's' ||
               move.movingPiece == 'b' || move.movingPiece == 'r') {
        // 後手: 移動元か移動先が敵陣（7〜9段目）
        if (toRank >= 6 || fromRank >= 6) {
            canPromote = true;
        }
    }

    if (canPromote) {
        move.isPromotion = true;
        promotions.append(move);
        move.isPromotion = false;
    }
}

// ============================================================
// 駒台から打つ手の生成
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateDropMoveForPiece(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                             const Turn& turn, const QChar& piece) const
{
    int count = pieceStand[piece];

    if (count > 0) {
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                int index = rank * BOARD_SIZE + file;

                if (emptySquareBitboard.test(static_cast<size_t>(index))) {
                    bool canDrop = true;

                    // 香車・桂馬は行き場のない段には打てない
                    if ((piece == 'L' && rank == 0) || (piece == 'N' && rank < 2)) canDrop = false;
                    else if ((piece == 'l' && rank == BOARD_SIZE - 1) || (piece == 'n' && rank >= BOARD_SIZE - 2)) canDrop = false;

                    if (canDrop) {
                        int fromFile = turn == BLACK ? BLACK_HAND_FILE : WHITE_HAND_FILE;
                        int fromRank = turn == BLACK ? m_pieceOrderBlack[piece] : m_pieceOrderWhite[piece];
                        allMovesList.append(ShogiMove(QPoint(fromFile, fromRank), QPoint(file, rank), piece, ' ', false));
                    }
                }
            }
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateDropNonPawnMoves(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const Turn& turn) const
{
    QList<QChar> pieces = turn == BLACK ? QList<QChar>({'L', 'N', 'S', 'G', 'B', 'R'}) : QList<QChar>({'l', 'n', 's', 'g', 'b', 'r'});

    for (auto it = pieces.cbegin(); it != pieces.cend(); ++it) {
        generateDropMoveForPiece(allMovesList, pieceStand, emptySquareBitboard, turn, *it);
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::generateDropPawnMoves(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard,
                                          const Turn& turn) const
{
    QChar pawn = turn == BLACK ? 'P' : 'p';
    int count = pieceStand[pawn];

    if (count > 0) {
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                int index = rank * BOARD_SIZE + file;

                if (emptySquaresNoPawnsFilesBitboard.test(static_cast<size_t>(index))) {
                    bool canDrop = true;
                    // 1段目/9段目には歩を打てない
                    if (pawn == 'P' && rank == 0) canDrop = false;
                    else if (pawn == 'p' && rank == BOARD_SIZE - 1) canDrop = false;

                    if (canDrop) {
                        int fromFile = turn == BLACK ? BLACK_HAND_FILE : WHITE_HAND_FILE;
                        int fromRank = turn == BLACK ? m_pieceOrderBlack[pawn] : m_pieceOrderWhite[pawn];
                        allMovesList.append(ShogiMove(QPoint(fromFile, fromRank), QPoint(file, rank), pawn, ' ', false));
                    }
                }
            }
        }
    }
}

// ============================================================
// 王手判定・王手回避
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
int MoveValidator::isKingInCheck(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                  const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                                  std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                  std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard)
{
    necessaryMovesBitboard.reset();
    attackWithoutKingBitboard.reset();

    int kingIndex = turn == BLACK ? K_IDX : k_IDX;

    // 玉が盤上に存在しない不正な局面
    if (allPieceBitboards[kingIndex].isEmpty()) {
        const QString errorMessage = tr("An error occurred in MoveValidator::isKingInCheck. The king is not on the board.");
        emit errorOccurred(errorMessage);
        return 0;
    }

    const std::bitset<NUM_BOARD_SQUARES>& kingBitboard = allPieceBitboards[kingIndex][0];
    int numChecks = 0;

    for (qsizetype i = 0; i < allPieceBitboards.size(); ++i) {
        if (i == kingIndex) continue;

        for (qsizetype j = 0; j < allPieceBitboards[i].size(); ++j) {
            const auto& attackBitboard = allPieceAttackBitboards[i][j];
            if ((kingBitboard & attackBitboard).any()) {
                attackWithoutKingBitboard = attackBitboard & ~kingBitboard;

                // 王手を防ぐために駒を置きたいマス＋王手元の駒のマス
                // numChecks==1のときのみ有効（両王手では意味がない）
                necessaryMovesBitboard = attackWithoutKingBitboard | allPieceBitboards[i][j];
                ++numChecks;
            }
        }
    }

    if (numChecks > 2) {
        const QString errorMessage = tr("An error occurred in MoveValidator::isKingInCheck. The player's king is checked by three or more pieces at the same time.");
        emit errorOccurred(errorMessage);
        return numChecks;
    }

    return numChecks;
}

// 王手されているときに王手を避ける指し手候補を生成する。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::filterMovesThatBlockThreat(const Turn& turn, const QVector<ShogiMove>& allMovesList, const std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                               QVector<ShogiMove>& kingBlockingMovesList) const
{
    kingBlockingMovesList.clear();
    QChar kingPiece = (turn == BLACK) ? 'K' : 'k';

    for (const auto &move : allMovesList) {
        int squareIndex = move.toSquare.y() * BOARD_SIZE + move.toSquare.x();

        // 王手回避マスへの移動、または玉自身の移動を候補に加える
        if (necessaryMovesBitboard[static_cast<size_t>(squareIndex)] || move.movingPiece == kingPiece) {
            kingBlockingMovesList.append(move);
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
int MoveValidator::checkIfKingInCheck(const Turn& turn, const QVector<QChar>& boardData)
{
    BoardStateArray piecePlacedBitboards;
    generateBitboard(boardData, piecePlacedBitboards);

    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    generateAllPieceBitboards(allPieceBitboards, boardData);

    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;
    generateAllIndividualPieceAttackBitboards(allPieceBitboards, piecePlacedBitboards, allPieceAttackBitboards);

    std::bitset<MoveValidator::NUM_BOARD_SQUARES> necessaryMovesBitboard;
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> attackWithoutKingBitboard;

    return isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);
}

// ============================================================
// 合法手フィルタ
// ============================================================

// 指し手リストの各指し手について、指した直後に自玉が王手されていない手だけを合法手リストに加える。
/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::filterLegalMovesList(const Turn& turn, const QVector<ShogiMove>& moveList, const QVector<QChar>& boardData, QVector<ShogiMove>& legalMovesList)
{
    legalMovesList.clear();

    QVector<QChar> boardDataAfterMove;

    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;

    for (const auto& move : std::as_const(moveList)) {
        applyMovesToBoard(move, boardData, boardDataAfterMove);

        generateAllPieceBitboards(allPieceBitboards, boardDataAfterMove);

        BoardStateArray bitboard;
        generateBitboard(boardDataAfterMove, bitboard);

        generateAllIndividualPieceAttackBitboards(allPieceBitboards, bitboard, allPieceAttackBitboards);

        Turn opponentTurn = (turn == BLACK) ? WHITE : BLACK;
        QVector<ShogiMove> allMovesList;

        generateShogiMoveFromBitboards(opponentTurn, allPieceBitboards, allPieceAttackBitboards, allMovesList, boardDataAfterMove);

        std::bitset<NUM_BOARD_SQUARES> necessaryMovesBitboard;
        std::bitset<NUM_BOARD_SQUARES> attackWithoutKingBitboard;

        int numchecks = isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

        // 自玉が王手状態でなければ合法手
        if (!numchecks) {
            legalMovesList.append(move);
        }
    }
}

// ============================================================
// 合法手照合
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::isMoveInList(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const
{
    for (const auto& move : std::as_const(movesList)) {
        if (currentMove == move) {
            return true;
        }
    }

    return false;
}

/// @todo remove コメントスタイルガイド適用済み
LegalMoveStatus MoveValidator::checkLegalMoveStatus(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const
{
    LegalMoveStatus legalMoveStatus;

    legalMoveStatus.nonPromotingMoveExists = isMoveInList(currentMove, movesList);

    ShogiMove promotingMove = currentMove;
    promotingMove.isPromotion = true;
    legalMoveStatus.promotingMoveExists = isMoveInList(promotingMove, movesList);

    return legalMoveStatus;
}

// ============================================================
// 指し手の合法手判定（振り分け）
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
LegalMoveStatus MoveValidator::validateMove(const int& numChecks, const QVector<QChar>& boardData, const ShogiMove& currentMove, const Turn& turn, BoardStateArray& piecePlacedBitboards,
                                 const QMap<QChar, int>& pieceStand, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard, QVector<ShogiMove>& legalMovesList,
                                 const Turn& opponentTurn, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard)
{
    if (currentMove.fromSquare.x() < BOARD_SIZE) {
        // 盤上の駒を動かす手
        return generateBitboardAndValidateMove(numChecks, boardData, piecePlacedBitboards, currentMove, turn, necessaryMovesBitboard, legalMovesList);
    } else {
        // 駒台から打つ手
        LegalMoveStatus legalMoveStatus;
        legalMoveStatus.promotingMoveExists = false;

        if (numChecks) {
            legalMoveStatus.nonPromotingMoveExists = validateMoveWithChecks(currentMove, turn, boardData, pieceStand, numChecks, attackWithoutKingBitboard, legalMovesList);
        } else {
            legalMoveStatus.nonPromotingMoveExists = validateMoveWithoutChecks(currentMove, turn, boardData, pieceStand, numChecks, legalMovesList, opponentTurn);
        }

        return legalMoveStatus;
    }
}

/// @todo remove コメントスタイルガイド適用済み
LegalMoveStatus MoveValidator::generateBitboardAndValidateMove(const int& numChecks, const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboards, const ShogiMove& currentMove,
                                                    const Turn& turn, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                                    QVector<ShogiMove>& legalMovesList)
{
    int fromIndex = currentMove.fromSquare.y() * BOARD_SIZE + currentMove.fromSquare.x();

    QChar piece = boardData[fromIndex];

    std::bitset<NUM_BOARD_SQUARES> pieceBitboard;
    pieceBitboard.set(static_cast<size_t>(fromIndex), true);

    std::bitset<NUM_BOARD_SQUARES> attackBitboard;
    generateSinglePieceAttackBitboard(attackBitboard, pieceBitboard, turn, piece, piecePlacedBitboards);

    return isBoardMoveValid(turn, boardData, currentMove, numChecks, piece, attackBitboard, pieceBitboard, necessaryMovesBitboard, legalMovesList);
}

/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::validateMoveWithChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                                      const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard, QVector<ShogiMove>& legalMovesList)
{
    return isHandPieceMoveValid(turn, boardData, pieceStand, currentMove, numChecks,  attackWithoutKingBitboard, legalMovesList);
}

// 自玉が王手されていない場合の打ち手判定（打ち歩詰めチェックを含む）。
/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::validateMoveWithoutChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                              const int& numChecks, QVector<ShogiMove>& legalMovesList, const Turn& opponentTurn)
{
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> emptySquareBitboard;
    generateEmptySquareBitboard(boardData, emptySquareBitboard);

    bool isLegal = isHandPieceMoveValid(turn, boardData, pieceStand, currentMove, numChecks,  emptySquareBitboard, legalMovesList);

    // 打った手が合法手でかつ相手玉の前に歩を打った場合、打ち歩詰めチェックを行う
    if ((isLegal) && (isPawnInFrontOfKing(currentMove, turn, boardData))) {
        QVector<QChar> boardDataAfterMove;
        QMap<QChar, int> pieceStandAfterMove;

        applyMovesToBoard(currentMove, boardData, boardDataAfterMove);
        decreasePieceCount(currentMove, pieceStand, pieceStandAfterMove);

        // 相手に合法手があれば打ち歩詰めではない
        if (generateLegalMoves(opponentTurn, boardDataAfterMove, pieceStandAfterMove)) {
            return true;
        } else {
            // 合法手が無ければ打ち歩詰め（禁手）
            qDebug() << tr("An error occurred in MoveValidator::validateMoveWithoutChecks. Dropping a pawn to give checkmate is not allowed.");
            return false;
        }
    }

    return isLegal;
}

/// @todo remove コメントスタイルガイド適用済み
bool MoveValidator::isPawnInFrontOfKing(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData) const
{
    QChar opponentKingPiece = (turn == BLACK) ? 'k' : 'K';
    int direction = (turn == BLACK) ? +1 : -1;

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;

            if (boardData[index] == opponentKingPiece) {
                if ((currentMove.toSquare.x() == file) &&
                    (currentMove.toSquare.y() == rank + direction) &&
                    ((turn == BLACK && currentMove.movingPiece == 'P' && currentMove.fromSquare.x() == BLACK_HAND_FILE) ||
                     (turn == WHITE && currentMove.movingPiece == 'p' && currentMove.fromSquare.x() == WHITE_HAND_FILE))) {
                    return true;
                } else {
                    return false;
                }
            }
        }
    }

    return false;
}

// ============================================================
// 盤面シミュレーション
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::applyMovesToBoard(const ShogiMove& move, const QVector<QChar>& boardData, QVector<QChar>& boardDataAfterMove) const
{
    int fromIndex = move.fromSquare.y() * MoveValidator::BOARD_SIZE + move.fromSquare.x();
    int toIndex = move.toSquare.y() * MoveValidator::BOARD_SIZE + move.toSquare.x();

    boardDataAfterMove = boardData;
    boardDataAfterMove[fromIndex] = ' ';

    if (move.isPromotion) {
        applyPromotionMovesToBoard(move, toIndex, boardDataAfterMove);
    } else {
        applyStandardMovesToBoard(move, toIndex, boardDataAfterMove);
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::applyPromotionMovesToBoard(const ShogiMove& move, int toIndex, QVector<QChar>& boardDataAfterMove) const
{
    static const QMap<QChar, QChar> promotionMap = {
        {'P', 'Q'}, {'p', 'q'},
        {'L', 'M'}, {'l', 'm'},
        {'N', 'O'}, {'n', 'o'},
        {'S', 'T'}, {'s', 't'},
        {'B', 'C'}, {'b', 'c'},
        {'R', 'U'}, {'r', 'u'}
    };

    QChar movingPiece = move.movingPiece;
    QChar promotedPiece = ' ';

    if (promotionMap.contains(movingPiece)) {
        promotedPiece = promotionMap[movingPiece];
    }

    boardDataAfterMove[toIndex] = promotedPiece;
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::applyStandardMovesToBoard(const ShogiMove& move, int toIndex, QVector<QChar>& boardDataAfterMove) const
{
    boardDataAfterMove[toIndex] = move.movingPiece;
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::decreasePieceCount(const ShogiMove& move, const QMap<QChar, int>& pieceStand, QMap<QChar, int>& pieceStandAfterMove)
{
    pieceStandAfterMove = pieceStand;

    QChar piece = move.movingPiece;

    if (pieceStand.contains(piece)) {
        if (pieceStandAfterMove[piece] > 0) {
            pieceStandAfterMove[piece]--;
        } else {
            const QString errorMessage = tr("An error occurred in MoveValidator::decreasePieceCount. There is no piece of %1.").arg(piece);
            emit errorOccurred(errorMessage);
            return;
        }
    } else {
        const QString errorMessage = tr("An error occurred in MoveValidator::decreasePieceCount. There is no piece of %1.").arg(piece);
        emit errorOccurred(errorMessage);
        return;
    }
}

// ============================================================
// デバッグ出力
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printBitboards(const BoardStateArray& piecePlacedBitboard) const
{
    static const QStringList pieceName = { "歩", "香車", "桂馬", "銀", "金", "角", "飛車", "玉", "と金", "成香", "成桂", "成銀", "馬", "龍" };

    for (int turnNumber = 0; turnNumber < NUM_PLAYERS; ++turnNumber) {
        for (int pieceType = 0; pieceType < NUM_PIECE_TYPES; ++pieceType) {
            qDebug() << (turnNumber == 0 ? "先手:" : "後手:");
            qDebug() << "Piece type" << pieceName.at(pieceType) << ":";
            const std::bitset<NUM_BOARD_SQUARES>& bitboard = piecePlacedBitboard[static_cast<size_t>(turnNumber)][static_cast<size_t>(pieceType)];
            QString bitboardRow;
            for (int rank = 0; rank < BOARD_SIZE; ++rank) {
                bitboardRow.clear();
                for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                    int index = rank * BOARD_SIZE + file;
                    bitboardRow.append(QString::number(bitboard.test(static_cast<size_t>(index))) + ' ');
                }
                qDebug() << bitboardRow;
            }
            qDebug() << "";
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printAllPieceBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards) const
{
    for (qsizetype i = 0; i < allPieceBitboards.size(); ++i) {
        qDebug() << "Piece:" << m_allPieces.at(i);
        const auto& currentBitboards = allPieceBitboards.at(i);
        for (const auto& bitboard : std::as_const(currentBitboards)) {
            QString bitboardString;
            for (int rank = 0; rank < BOARD_SIZE; ++rank) {
                for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                    int index = rank * BOARD_SIZE + file;
                    bitboardString.append(bitboard.test(static_cast<size_t>(index)) ? '1' : '0').append(' ');
                }
                bitboardString.append('\n');
            }
            bitboardString.append('\n');
            qDebug().noquote() << bitboardString;
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                                        const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const
{
    for (qsizetype i = 0; i < allPieceBitboards.size(); ++i) {
        const auto& currentAttackBitboards = allPieceAttackBitboards.at(i);
        for (const auto& bitboard : std::as_const(currentAttackBitboards)) {
            QString bitboardRow;
            for (int rank = 0; rank < BOARD_SIZE; ++rank) {
                bitboardRow.clear();
                for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                    int index = rank * BOARD_SIZE + file;
                    bitboardRow.append(QString::number(bitboard.test(static_cast<size_t>(index))) + ' ');
                }
                qDebug() << bitboardRow;
            }
            qDebug() << "";
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printBitboardContent(const QVector<std::bitset<NUM_BOARD_SQUARES>>& bitboards) const
{
    for (const auto& bitboard : std::as_const(bitboards)) {
        printSingleBitboard(bitboard);
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printSingleBitboard(const std::bitset<NUM_BOARD_SQUARES>& bitboard) const
{
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        QString bitboardRow;
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            bitboardRow.append(QString::number(bitboard.test(static_cast<size_t>(index))) + ' ');
        }
        qDebug() << bitboardRow;
    }
    qDebug() << "";
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printShogiMoveList(const QVector<ShogiMove>& moveList) const
{
    int i = 0;
    for (const auto& move : std::as_const(moveList)) {
        std::cout << std::setw(3) << std::setfill(' ') << ++i << " Shogi move: " << move << std::endl;
    }
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printShogiBoard(const QVector<QChar>& boardData)
{
    QString boardString;
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            QChar piece = boardData[rank * BOARD_SIZE + file];
            if (piece == ' ') {
                boardString.append("  ");
            } else {
                boardString.append(piece).append(' ');
            }
        }
        boardString.append('\n');
    }
    boardString.append('\n');
    qDebug().noquote() << boardString;
}

/// @todo remove コメントスタイルガイド適用済み
void MoveValidator::printBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand) const
{
    QString row;

    static const QVector<QChar> pieceOrder = {'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K', 'k', 'r', 'b', 'g', 's', 'n', 'l', 'p'};

    static const QMap<QChar, QString> pieceKanjiNames = {
        {' ', " 　"}, {'P', " 歩"}, {'L', " 香"}, {'N', " 桂"}, {'S', " 銀"}, {'G', " 金"}, {'B', " 角"}, {'R', " 飛"}, {'K', " 玉"},
        {'Q', " と"}, {'M', " 杏"}, {'O', " 圭"}, {'T', " 全"}, {'C', " 馬"}, {'U', " 龍"},
        {'p', "v歩"}, {'l', "v香"}, {'n', "v桂"}, {'s', "v銀"}, {'g', "v金"}, {'b', "v角"}, {'r', "v飛"}, {'k', "v玉"},
        {'q', "vと"}, {'m', "v杏"}, {'o', "v圭"}, {'t', "v全"}, {'c', "v馬"}, {'u', "v龍"}
    };

    QString gotePieces = "持ち駒：";
    for (const auto& piece : std::as_const(pieceOrder)) {
        if (piece.isLower()) {
            gotePieces += pieceKanjiNames[piece] + " " + QString::number(pieceStand[piece]) + " ";
        }
    }
    qDebug() << gotePieces;

    static const QVector<QString> rowKanjiNames = {"一", "二", "三", "四", "五", "六", "七", "八", "九"};

    qDebug() << "  ９ ８ ７ ６ ５ ４ ３ ２ １";
    qDebug() << "+------------------------+";
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            row.append(pieceKanjiNames[boardData[index]]);
        }
        qDebug() << "|" << row << "|" << rowKanjiNames[rank];
        row.clear();
    }
    qDebug() << "+------------------------+";

    QString sentePieces = "持ち駒：";
    for (const auto& piece : std::as_const(pieceOrder)) {
        if (piece.isUpper()) {
            sentePieces += pieceKanjiNames[piece] + " " + QString::number(pieceStand[piece]) + " ";
        }
    }
    qDebug() << sentePieces;
}
