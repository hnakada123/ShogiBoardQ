#ifndef MOVEVALIDATOR_H
#define MOVEVALIDATOR_H

#include <bitset>
#include <QVector>
#include <QStringList>
#include "shogimove.h"
#include "legalmovestatus.h"
#include <QMap>
#include <QObject>

// 指した手が合法手であるかどうかを判定するクラス
class MoveValidator : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit MoveValidator(QObject* parent = nullptr);

    // 駒番号を表す列挙型
    enum PieceType {
        PAWN,
        LANCE,
        KNIGHT,
        SILVER,
        GOLD,
        BISHOP,
        ROOK,
        KING,
        PROMOTED_PAWN,
        PROMOTED_LANCE,
        PROMOTED_KNIGHT,
        PROMOTED_SILVER,
        HORSE,
        DRAGON,
        PIECE_TYPE_SIZE
    };

    // 先手番、後手番を示す列挙型
    enum Turn { BLACK, WHITE, TURN_SIZE };

    // 駒の種類数
    static constexpr int NUM_PIECE_TYPES = PIECE_TYPE_SIZE;

    // 対局者数（先手と後手）
    static constexpr int NUM_PLAYERS = TURN_SIZE;

    // 盤面の1辺のマス数（列数と段数）
    static constexpr int BOARD_SIZE = 9;

    // 将棋盤のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    // 指し手が合法手かどうかを判定する。
    LegalMoveStatus isLegalMove(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, ShogiMove& currentMove);

    // 指定局面での合法手を生成する。
    int generateLegalMoves(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);

signals:
    // エラーを報告するためのシグナル
    void errorOccurred(const QString& errorMessage);

private:
    // 全ての駒の種類を1文字のアルファベットで表す。
    QVector<QChar> m_allPieces;

    // 先手と後手の駒台の順序番号
    QMap<QChar, int> m_pieceOrderBlack;
    QMap<QChar, int> m_pieceOrderWhite;

    // 二歩のチェック
    // 先手あるいは後手の歩が同じ筋に複数存在する場合、二歩が存在すると判定する。
    void checkDoublePawn(const QVector<QChar>& boardData);

    // 各駒の数が最大数を超えていないかチェックする。
    void checkPieceCount(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);

    // 玉が一方の対局者につき一つ存在していることをチェックする。
    void checkKingPresence(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);

    // この関数は、将棋の盤面上の各駒が適切な位置にあるかどうかをチェックする。
    void checkCorrectPosition(const QVector<QChar>& boardData);

    // 先手および後手、各駒の種類ごとのbitboardを表す型を略称化
    using BoardStateArray = std::array<std::array<std::bitset<NUM_BOARD_SQUARES>, NUM_PIECE_TYPES>, NUM_PLAYERS>;

    // 先手、後手の2種類
    // 歩、香車、桂馬、銀、金、角、飛車、玉、と金、成香、成桂、成銀、馬、龍の14種類
    // の合計28種類の駒が存在するマスを表すbitboardを作成する。
    void generateBitboard(const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboard) const;

    // piecePlacedBitboardの標準出力する。
    void printBitboards(const BoardStateArray& piecePlacedBitboard) const;

    // 味方の各駒が存在するbitBoard中でindexで指定されたマスに味方の駒が存在するかどうかを判定する。
    bool isPieceOnSquare(int& index, const std::array<std::bitset<NUM_BOARD_SQUARES>, NUM_PIECE_TYPES>& turnBitboards) const;

    // 盤面データboardDataからtargetPieceに該当する駒が存在するマスを表すbitboardを各駒ごとに作成し、そのリストを返す。
    QVector<std::bitset<NUM_BOARD_SQUARES>> generateIndividualPieceBitboards(const QVector<QChar>& boardData, const QChar& targetPiece) const;

    // 先手の歩、先手の香車、先手の桂馬〜先手の龍、
    // 後手の歩、後手の香車、後手の桂馬〜後手の龍の28種類に分けて
    // 各駒ごとに駒が存在するマスを表すbitboardを作成する。
    // 例．平手で各40個の駒それぞれが存在するマスを示すbitboard
    // 先手の1七歩の場合
    // Piece: P
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 1
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    void generateAllPieceBitboards(QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const QVector<QChar>& boardData) const;

    // allPieceBitboardsの出力
    // 例．平手で各40個の駒それぞれが存在するマスを示すbitboard
    // 先手の1七歩の場合
    // Piece: P
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 1
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    void printAllPieceBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards) const;

    // 各個々の駒が移動できるマスを表すbitboardを作成する。
    // 例．平手で各40個の駒の移動可能なマスを示すbitboard
    // 先手の1七歩の場合、1六に移動できる。
    // Piece: P
    // 先手:
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 1
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    void generateAllIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const BoardStateArray& piecePlacedBitboard,
                                                   QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const;

    // allPieceAttackBitboardsの出力
    // 例．平手で各40個の駒の移動可能なマスを示すbitboard
    // 先手の1七歩の場合、1六に移動できる。
    // Piece: P
    // 先手:
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 1
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    void printIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                             const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const;

    // 盤上の駒を動かした場合の全指し手リスト生成
    void generateShogiMoveFromBitboards(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                        const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                                        QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const;

    // bitboardリストから各bitboardを取り出し、そのbitboardの内容を標準出力する。
    void printBitboardContent(const QVector<std::bitset<NUM_BOARD_SQUARES>>& bitboards) const;

    // 各駒が存在するマスの位置を示すsinglePieceBitboardから移動可能なマスの位置を示すattackBitboardを生成する。
    void generateSinglePieceAttackBitboard(std::bitset<MoveValidator::NUM_BOARD_SQUARES>& attackBitboard, const std::bitset<NUM_BOARD_SQUARES>& singlePieceBitboard,
                                           const Turn& turn, QChar pieceType, const BoardStateArray& piecePlacedBitboard) const;

    // 特定の種類の駒が攻撃可能なマスを表すビットボードを生成する。
    // continuousは、駒が連続して移動できるか（飛車、角、馬、龍など）を指定する。
    // enemyOccupiedStopは、敵の駒が存在するマスに到達した時点で駒の進行を止めるかを指定する（飛車、角、馬、龍など）。
    void generateAttackBitboard(std::bitset<NUM_BOARD_SQUARES>& attackBitboard, const Turn& turn,
                                const BoardStateArray& piecePlacedBitboard, const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard,
                                const QList<QPoint>& directions, bool continuous, bool enemyOccupiedStop) const;

    // 駒のインデックスを定義
    enum PieceIndex {
        P_IDX = 0, L_IDX, N_IDX, S_IDX, G_IDX, B_IDX, R_IDX, K_IDX, Q_IDX, M_IDX, O_IDX, T_IDX, C_IDX, U_IDX,
        p_IDX, l_IDX, n_IDX, s_IDX, g_IDX, b_IDX, r_IDX, k_IDX, q_IDX, m_IDX, o_IDX, t_IDX, c_IDX, u_IDX
    };

    // 手番の玉が王手されているかどうかを調べる。
    int isKingInCheck(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                       const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                       std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                      std::bitset<NUM_BOARD_SQUARES>& tmpBitboard);

    // bitboardの内容を標準出力する。
    void printSingleBitboard(const std::bitset<NUM_BOARD_SQUARES>& bitboard) const;

    // 王手されているときに王手を避ける指し手リスト候補を生成する。
    // 例.先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
    // 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる手を生成。
    // また、玉を移動させる手も生成するが王手が回避できていない手も含まれている。
    // 王手が回避できない手は、後の処理で除去している。
    // 1 Shogi move: From: (6, 7) To: (6, 6) Moving Piece: P Captured Piece:   Promotion: false
    // 2 Shogi move: From: (8, 9) To: (7, 7) Moving Piece: N Captured Piece:   Promotion: false
    // 3 Shogi move: From: (8, 8) To: (9, 8) Moving Piece: K Captured Piece:   Promotion: false
    // 4 Shogi move: From: (8, 8) To: (7, 8) Moving Piece: K Captured Piece:   Promotion: false
    void filterMovesThatBlockThreat(const Turn& turn, const QVector<ShogiMove>& allMovesList, const std::bitset<NUM_BOARD_SQUARES> &necessaryMovesBitboard,
                                    QVector<ShogiMove> &nonKingBlockingMoves) const;

    // 指し手リストから各指し手を標準出力する。
    void printShogiMoveList(const QVector<ShogiMove>& moveList) const;

    // 指し手リストmoveListの各指し手moveに対して指した直後の盤面データboardDataAfterMoveを作成し、
    // その局面で相手が盤上の駒を動かした際の手を生成して、その中に自玉に王手が掛かっていない手だけを
    // 合法手リストlegalMovesListに加える。
    void filterLegalMovesList(const Turn& turn, const QVector<ShogiMove>& moveList, const QVector<QChar>& boardData, QVector<ShogiMove>& legalMovesList);

    // 駒が置かれていないマスを表すbitboardを作成する。
    void generateEmptySquareBitboard(const QVector<QChar>& boardData, std::bitset<MoveValidator::NUM_BOARD_SQUARES>& emptySquareBitboard) const;

    // 歩以外の駒を駒台から打つ場合の指し手を生成する。
    void generateDropNonPawnMoves(QVector<ShogiMove>& dropMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const Turn& turn) const;

    // 歩を駒台から打つ場合の指し手を生成する。
    void generateDropPawnMoves(QVector<ShogiMove>& dropMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard, const Turn& turn) const;

    // 盤面データを9x9のマスに表示する。
    void printShogiBoard(const QVector<QChar>& boardData);

    // 歩が無い筋を1で表すbitboardを作成する。
    void generateBitboardForEmptyFiles(const Turn& turn, const QVector<QChar>& boardData, std::bitset<NUM_BOARD_SQUARES>& bitboard) const;

    // 駒が置かれていないマスを表すemptySquareBitboardと歩が無い筋を1で表すemptyFilePawnBitboardをAND演算した
    // emptySquaresNoPawnsFilesBitboardを作成する。
    void performAndOperation(const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const std::bitset<NUM_BOARD_SQUARES>& emptyFilePawnBitboard,
                             std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard) const;

    // 盤上の駒を動かした場合の全指し手リストを生成する。
    void generateShogiMoveFromBitboard(const QChar piece, const std::bitset<NUM_BOARD_SQUARES>& bitboard,
                                       const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                       QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const;

    // 指し手が合法手の中にあるかどうかをチェックする。
    // 合法手に一致すればtrueを返し、無ければfalseを返す。
    bool isMoveInList(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const;

    // 指し手が合法手の中にあるかどうかをチェックする。
    // 合法手に一致すればtrueを返し、無ければfalseを返す。
    LegalMoveStatus checkLegalMoveStatus(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const;

    // 盤面と持ち駒を引数として、局面を漢字で出力する。
    void printBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand) const;

    // 将棋盤内の駒を動かした場合の合法手がどうかを判定する。
    LegalMoveStatus isBoardMoveValid(const Turn& turn, const QVector<QChar>& boardData, const ShogiMove& currentMove,
                                     const int& numChecks, const QChar& piece, const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                     const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard, const std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                     QVector<ShogiMove>& legalMovesList);

    // 駒台の駒を打った手が合法手かどうかを判定する。
    bool isHandPieceMoveValid(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                              const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList);

    // 打った駒が歩かそれ以外の駒かを判別して合法手を生成する。
    bool generateLegalMovesForPiece(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                                    const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList);

    // 歩を打った場合の指し手を生成する。
    void generateDropMoveForPawn(QVector<ShogiMove>& legalMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                 const Turn& turn, const QVector<QChar>& boardData) const;

    void generateDropMoveForPiece(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                  const Turn& turn, const QChar& piece) const;

    // 成って指した手を生成する。
    void generateAllPromotions(ShogiMove& move, QVector<ShogiMove>& promotions) const;

    // 1手指した後の局面データを作成する。
    void applyMovesToBoard(const ShogiMove& move, const QVector<QChar>& boardData, QVector<QChar>& boardDataAfterMove) const;

    // 駒を成って指した直後の盤面データを作成する。
    void applyPromotionMovesToBoard(const ShogiMove& move, int& toIndex, QVector<QChar>& boardDataAfterMove) const;

    // 駒を不成で指した直後の盤面データを作成する。
    void applyStandardMovesToBoard(const ShogiMove& move, int& toIndex, QVector<QChar>& boardDataAfterMove) const;

    // 駒を持ち駒から打った後の持ち駒数を更新する。
    void decreasePieceCount(const ShogiMove& move, const QMap<QChar, int>& pieceStand, QMap<QChar, int>& pieceStandAfterMove);

    // 指し手が合法手かどうか判定する。
    LegalMoveStatus validateMove(const int& numChecks, const QVector<QChar>& boardData, const ShogiMove& currentMove, const Turn& turn, BoardStateArray& piecePlacedBitboards,
                                 const QMap<QChar, int>& pieceStand, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard, QVector<ShogiMove>& legalMovesList,
                                 const Turn& opponentTurn, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard);

    // 将棋盤内の駒を動かした場合の指し手のマスの位置を示すbitboardとそのマスの駒の移動可能なマスの位置を示すbitboardを作成し、
    // 指し手が合法手かどうか判定する。
    LegalMoveStatus generateBitboardAndValidateMove(const int& numChecks, const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboards, const ShogiMove& currentMove,
                                         const Turn& turn, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard, QVector<ShogiMove>& legalMovesList);

    // 自玉が王手されている場合、駒台の駒を打った場合の合法手かどうかを判定する。
    bool validateMoveWithChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard, QVector<ShogiMove>& legalMovesList);

    // 自玉が王手されていない場合、駒台の駒を打った場合の合法手かどうかを判定する。
    bool validateMoveWithoutChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                   const int& numChecks, QVector<ShogiMove>& legalMovesList, const Turn& opponentTurn);

    // 相手玉の前のマスに駒台の歩を打ったかどうかを判定する。
    bool isPawnInFrontOfKing(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData) const;

    // 盤面データと持ち駒のチェックを行う。
    // 盤面データと持ち駒が有効でない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    // 有効な場合はtrueを返し、無効な場合はfalseを返す。
    void validateBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand);

    // 指し手の筋の値を検証する。
    // 筋の値が有効でない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    void validateMoveFileValue(int fromSquareX);

    // 指し手の駒が盤面の駒と一致するかを検証する。
    // 指し手の駒が盤面の駒と一致しない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    // 一致する場合はtrueを返し、一致しない場合はfalseを返す。
    void validateMovingPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData);

    // 駒台の駒が有効かどうかを検証する。
    // 指し手が駒台から駒を打つ場合、その駒の数が正の数かをチェックする。
    // 無効な場合はエラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    void validatePieceStand(Turn turn, const ShogiMove& currentMove, const QMap<QChar, int>& pieceStand);

    // 駒台のエラー情報をログに出力し、エラーメッセージボックスを表示する。
    void logAndShowPieceStandError(const QString& errorMessage, QChar piece, const QMap<QChar, int>& pieceStand);

    // 取った駒が盤面の駒と一致するかを検証する。
    // 取った駒が盤面の移動先の駒と一致しない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    void validateCapturedPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData);

    // 複数の検証を一括して行う。
    // 盤面データと持ち駒、指し手の筋、指し手の駒、駒台の駒、取った駒が有効であるかを検証する。
    // 無効な場合はエラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    void validateMoveComponents(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove);
};

#endif // MOVEVALIDATOR_H
