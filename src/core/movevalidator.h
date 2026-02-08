#ifndef MOVEVALIDATOR_H
#define MOVEVALIDATOR_H

/// @file movevalidator.h
/// @brief 合法手判定クラスの定義

#include <bitset>
#include <QVector>
#include <QStringList>
#include "shogimove.h"
#include "legalmovestatus.h"
#include <QMap>
#include <QObject>

/**
 * @brief 指し手の合法性を判定するクラス
 *
 * ビットボードを用いて各駒の利きを計算し、
 * 王手回避・二歩・打ち歩詰め等のルールを考慮した
 * 合法手判定および合法手生成を行う。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class MoveValidator : public QObject
{
    Q_OBJECT

public:
    explicit MoveValidator(QObject* parent = nullptr);

    /// 駒種別
    enum PieceType {
        PAWN,             ///< 歩
        LANCE,            ///< 香
        KNIGHT,           ///< 桂
        SILVER,           ///< 銀
        GOLD,             ///< 金
        BISHOP,           ///< 角
        ROOK,             ///< 飛
        KING,             ///< 玉
        PROMOTED_PAWN,    ///< と金
        PROMOTED_LANCE,   ///< 成香
        PROMOTED_KNIGHT,  ///< 成桂
        PROMOTED_SILVER,  ///< 成銀
        HORSE,            ///< 馬
        DRAGON,           ///< 龍
        PIECE_TYPE_SIZE   ///< 駒種の総数
    };

    /// 手番
    enum Turn {
        BLACK,     ///< 先手
        WHITE,     ///< 後手
        TURN_SIZE  ///< 手番の総数
    };

    static constexpr int NUM_PIECE_TYPES = PIECE_TYPE_SIZE;   ///< 駒種数
    static constexpr int NUM_PLAYERS = TURN_SIZE;              ///< 対局者数
    static constexpr int BOARD_SIZE = 9;                       ///< 盤の1辺のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE; ///< 盤のマス数
    static constexpr int BLACK_HAND_FILE = BOARD_SIZE;         ///< 先手駒台のX座標 (9)
    static constexpr int WHITE_HAND_FILE = BOARD_SIZE + 1;     ///< 後手駒台のX座標 (10)

    /**
     * @brief 指し手が合法手かどうかを判定する
     * @param turn 現在の手番
     * @param boardData 盤面データ（81マス）
     * @param pieceStand 駒台の駒数マップ
     * @param currentMove 判定する指し手
     * @return 成り/不成それぞれの合法手存在状態
     */
    LegalMoveStatus isLegalMove(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, ShogiMove& currentMove);

    /**
     * @brief 指定局面での全合法手を生成する
     * @return 合法手の総数
     */
    int generateLegalMoves(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);

    /**
     * @brief 指定手番の玉が王手されているかを判定する
     * @return 王手の数（0=なし, 1=王手, 2=両王手）
     */
    int checkIfKingInCheck(const Turn& turn, const QVector<QChar>& boardData);

signals:
    /// エラー発生通知（現在connect先なし）
    void errorOccurred(const QString& errorMessage);

private:
    QVector<QChar> m_allPieces;            ///< 全駒種の1文字表記リスト

    QMap<QChar, int> m_pieceOrderBlack;    ///< 先手駒台の駒順序マップ
    QMap<QChar, int> m_pieceOrderWhite;    ///< 後手駒台の駒順序マップ

    // --- 先手/後手 × 駒種のビットボード型 ---
    using BoardStateArray = std::array<std::array<std::bitset<NUM_BOARD_SQUARES>, NUM_PIECE_TYPES>, NUM_PLAYERS>;

    /// 駒のインデックス定義（ビットボード配列のアクセス用）
    enum PieceIndex {
        P_IDX = 0, L_IDX, N_IDX, S_IDX, G_IDX, B_IDX, R_IDX, K_IDX, Q_IDX, M_IDX, O_IDX, T_IDX, C_IDX, U_IDX,
        p_IDX, l_IDX, n_IDX, s_IDX, g_IDX, b_IDX, r_IDX, k_IDX, q_IDX, m_IDX, o_IDX, t_IDX, c_IDX, u_IDX
    };

    // --- 盤面検証 ---
    void checkDoublePawn(const QVector<QChar>& boardData);
    void checkPieceCount(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);
    void checkKingPresence(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);
    void checkCorrectPosition(const QVector<QChar>& boardData);

    // --- ビットボード生成 ---
    void generateBitboard(const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboard) const;
    void printBitboards(const BoardStateArray& piecePlacedBitboard) const;
    bool isPieceOnSquare(int index, const std::array<std::bitset<NUM_BOARD_SQUARES>, NUM_PIECE_TYPES>& turnBitboards) const;
    QVector<std::bitset<NUM_BOARD_SQUARES>> generateIndividualPieceBitboards(const QVector<QChar>& boardData, const QChar& targetPiece) const;
    void generateAllPieceBitboards(QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const QVector<QChar>& boardData) const;
    void printAllPieceBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards) const;

    // --- 利き計算 ---

    /// 各駒の移動可能マスを示す利きビットボードを生成する
    void generateAllIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const BoardStateArray& piecePlacedBitboard,
                                                   QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const;
    void printIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                             const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const;
    void generateSinglePieceAttackBitboard(std::bitset<MoveValidator::NUM_BOARD_SQUARES>& attackBitboard, const std::bitset<NUM_BOARD_SQUARES>& singlePieceBitboard,
                                           const Turn& turn, QChar pieceType, const BoardStateArray& piecePlacedBitboard) const;

    /// 方向リストに基づいて利きビットボードを生成する（飛び駒対応）
    void generateAttackBitboard(std::bitset<NUM_BOARD_SQUARES>& attackBitboard, const Turn& turn,
                                const BoardStateArray& piecePlacedBitboard, const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard,
                                const QList<QPoint>& directions, bool continuous, bool enemyOccupiedStop) const;

    // --- 指し手生成 ---
    void generateShogiMoveFromBitboards(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                        const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                                        QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const;
    void generateShogiMoveFromBitboard(const QChar piece, const std::bitset<NUM_BOARD_SQUARES>& bitboard,
                                       const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                       QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const;
    void generateAllPromotions(ShogiMove& move, QVector<ShogiMove>& promotions) const;

    // --- 王手判定・回避 ---
    int isKingInCheck(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                       const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                       std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                      std::bitset<NUM_BOARD_SQUARES>& tmpBitboard);
    void filterMovesThatBlockThreat(const Turn& turn, const QVector<ShogiMove>& allMovesList, const std::bitset<NUM_BOARD_SQUARES> &necessaryMovesBitboard,
                                    QVector<ShogiMove> &nonKingBlockingMoves) const;

    // --- 合法手フィルタ ---
    void filterLegalMovesList(const Turn& turn, const QVector<ShogiMove>& moveList, const QVector<QChar>& boardData, QVector<ShogiMove>& legalMovesList);

    // --- 駒打ち ---
    void generateEmptySquareBitboard(const QVector<QChar>& boardData, std::bitset<MoveValidator::NUM_BOARD_SQUARES>& emptySquareBitboard) const;
    void generateDropNonPawnMoves(QVector<ShogiMove>& dropMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const Turn& turn) const;
    void generateDropPawnMoves(QVector<ShogiMove>& dropMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard, const Turn& turn) const;
    void generateBitboardForEmptyFiles(const Turn& turn, const QVector<QChar>& boardData, std::bitset<NUM_BOARD_SQUARES>& bitboard) const;
    void performAndOperation(const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const std::bitset<NUM_BOARD_SQUARES>& emptyFilePawnBitboard,
                             std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard) const;

    // --- 盤面変更適用 ---
    void applyMovesToBoard(const ShogiMove& move, const QVector<QChar>& boardData, QVector<QChar>& boardDataAfterMove) const;
    void applyPromotionMovesToBoard(const ShogiMove& move, int toIndex, QVector<QChar>& boardDataAfterMove) const;
    void applyStandardMovesToBoard(const ShogiMove& move, int toIndex, QVector<QChar>& boardDataAfterMove) const;
    void decreasePieceCount(const ShogiMove& move, const QMap<QChar, int>& pieceStand, QMap<QChar, int>& pieceStandAfterMove);

    // --- 合法手判定内部 ---
    LegalMoveStatus isBoardMoveValid(const Turn& turn, const QVector<QChar>& boardData, const ShogiMove& currentMove,
                                     const int& numChecks, const QChar& piece, const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                     const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard, const std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                     QVector<ShogiMove>& legalMovesList);
    bool isHandPieceMoveValid(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                              const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList);
    bool generateLegalMovesForPiece(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                                    const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList);
    void generateDropMoveForPawn(QVector<ShogiMove>& legalMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                 const Turn& turn, const QVector<QChar>& boardData) const;
    void generateDropMoveForPiece(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                  const Turn& turn, const QChar& piece) const;
    bool isMoveInList(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const;
    LegalMoveStatus checkLegalMoveStatus(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const;

    /// 打ち歩詰め判定: 相手玉の直前に歩を打ったかどうか
    bool isPawnInFrontOfKing(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData) const;

    LegalMoveStatus validateMove(const int& numChecks, const QVector<QChar>& boardData, const ShogiMove& currentMove, const Turn& turn, BoardStateArray& piecePlacedBitboards,
                                 const QMap<QChar, int>& pieceStand, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard, QVector<ShogiMove>& legalMovesList,
                                 const Turn& opponentTurn, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard);
    LegalMoveStatus generateBitboardAndValidateMove(const int& numChecks, const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboards, const ShogiMove& currentMove,
                                         const Turn& turn, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard, QVector<ShogiMove>& legalMovesList);
    bool validateMoveWithChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard, QVector<ShogiMove>& legalMovesList);
    bool validateMoveWithoutChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                   const int& numChecks, QVector<ShogiMove>& legalMovesList, const Turn& opponentTurn);

    // --- 入力検証 ---
    void validateBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);
    void validateMoveFileValue(int fromSquareX);
    void validateMovingPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData);
    void validatePieceStand(Turn turn, const ShogiMove& currentMove, const QMap<QChar, int>& pieceStand);
    void logAndShowPieceStandError(const QString& errorMessage, QChar piece, const QMap<QChar, int>& pieceStand);
    void validateCapturedPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData);
    void validateMoveComponents(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove);

    // --- デバッグ出力 ---
    void printBitboardContent(const QVector<std::bitset<NUM_BOARD_SQUARES>>& bitboards) const;
    void printSingleBitboard(const std::bitset<NUM_BOARD_SQUARES>& bitboard) const;
    void printShogiMoveList(const QVector<ShogiMove>& moveList) const;
    void printShogiBoard(const QVector<QChar>& boardData);
    void printBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand) const;
};

#endif // MOVEVALIDATOR_H
