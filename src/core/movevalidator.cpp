#include "movevalidator.h"
#include "shogimove.h"
#include "legalmovestatus.h"

#include <iostream>
#include <iomanip>
#include <QMap>
#include <array>
#include <QDebug>

// 指した手が合法手であるかどうかを判定するクラス
// コンストラクタ
MoveValidator::MoveValidator(QObject* parent) : QObject(parent)
{
    // 全ての駒の種類を1文字のアルファベットで表す。
    m_allPieces = {'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K', 'Q', 'M', 'O', 'T', 'C', 'U',
                   'p', 'l', 'n', 's', 'g', 'b', 'r', 'k', 'q', 'm', 'o', 't', 'c', 'u'};

    // 先手と後手の駒台の順序番号
    m_pieceOrderBlack = {{'P', 0}, {'L', 1}, {'N', 2}, {'S', 3}, {'G', 4}, {'B', 5}, {'R', 6}};
    m_pieceOrderWhite = {{'r', 2}, {'b', 3}, {'g', 4}, {'s', 5}, {'n', 6}, {'l', 7}, {'p', 8}};
}

// 二歩が存在するかどうかをチェックする。
// 先手あるいは後手の歩が同じ筋に複数存在する場合、二歩が存在すると判定する。
void MoveValidator::checkDoublePawn(const QVector<QChar>& boardData)
{
    // 筋
    for (int file = 0; file < BOARD_SIZE; ++file) {
        int pawnCountBlack = 0;  // 先手の歩 'P'
        int pawnCountWhite = 0;  // 後手の歩 'p'
        // 段
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            int index = rank * BOARD_SIZE + file;
            if (boardData[index] == 'P') ++pawnCountBlack;
            if (boardData[index] == 'p') ++pawnCountWhite;
        }

        // 先手あるいは後手の歩が同じ筋に複数存在する場合、二歩が存在すると判定する。
        if (pawnCountBlack > 1 || pawnCountWhite > 1) {
            const QString errorMessage = tr("An error occurred in MoveValidator::checkDoublePawn. There is a double pawn situation.");
            emit errorOccurred(errorMessage);
            return; // ★ 打ち切り
        }
    }
}

// 各駒の数が最大数を超えていないかチェックする。
void MoveValidator::checkPieceCount(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    // 各駒のカウント数
    QMap<QChar, int> currentPieceCount;

    // 将棋盤上の駒の数をカウントする。
    for (auto piece : boardData) {
        // 空白マス以外の駒の数をカウントする。
        if(piece != ' ') {
            currentPieceCount[piece]++;
        }
    }

    // 駒台の駒の数をカウントする。
    for (auto it = pieceStand.begin(); it != pieceStand.end(); ++it) {
        currentPieceCount[it.key()] += it.value();
    }

    // 各駒の最大数
    static const QMap<QChar, int> maxTotalPieceCount = {
        {'P', 18}, {'L', 4}, {'N', 4}, {'S', 4}, {'G', 4}, {'B', 2}, {'R', 2}, {'K', 1},
        {'p', 18}, {'l', 4}, {'n', 4}, {'s', 4}, {'g', 4}, {'b', 2}, {'r', 2}, {'k', 1}
    };

    // 各駒の数が最大数を超えていないかチェックする。
    for (auto it = maxTotalPieceCount.begin(); it != maxTotalPieceCount.end(); ++it) {
        // 各駒の数が最大数を超えていた場合
        if (currentPieceCount[it.key()] > it.value()) {
            const QString errorMessage = tr("An error occurred in MoveValidator::checkPieceCount. The number of pieces exceeds the maximum allowed.");
            emit errorOccurred(errorMessage);
            return; // ★ 打ち切り
        }
    }
}

// 玉が一方の対局者につき一つ存在していることをチェックする。
void MoveValidator::checkKingPresence(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    Q_UNUSED(pieceStand);  // 玉は持ち駒にできないため使用しない

    // 盤面上の先手と後手の玉の数をカウントする。
    int kingCountBlack = 0;  // 先手の玉 'K'
    int kingCountWhite = 0;  // 後手の玉 'k'

    for (const auto& piece : std::as_const(boardData)) {
        if (piece == 'K') ++kingCountBlack;
        if (piece == 'k') ++kingCountWhite;
    }

    // 先手と後手の玉が一つずつ存在していない場合、エラーになる。
    if ((kingCountBlack != 1) || (kingCountWhite != 1)) {
        const QString errorMessage = tr("An error occurred in MoveValidator::checkKingPresence. There is not exactly one king per player.");
        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }
}

// 将棋の盤面上の各駒が適切な位置にあるかどうかをチェックする。
void MoveValidator::checkCorrectPosition(const QVector<QChar>& boardData)
{
    // 将棋盤のマスの数だけ繰り返す。
    for (qsizetype i = 0; i < boardData.size(); ++i) {
        // 各マスの駒文字を取得する。
        QChar piece = boardData[i];

        // マスのインデックスから段番号を計算する。
        int rank = static_cast<int>(i) / BOARD_SIZE + 1;

        // 以下のルールを適用する。
        // - 先手の歩('P')と香車('L')は1段目に存在できない。
        // - 先手の桂馬('N')は1段目と2段目に存在できない。
        // - 後手の歩('p')と香車('l')は9段目に存在できない。
        // - 後手の桂馬('n')は8段目と9段目に存在できない。
        // これらのルールを満たさない場合、関数はfalseを返す。
        if ((piece == 'p' && rank == BOARD_SIZE) || (piece == 'l' && rank == BOARD_SIZE) ||
            (piece == 'n' && (rank == 8 || rank == BOARD_SIZE)) ||
            (piece == 'P' && rank == 1) || (piece == 'L' && rank == 1) ||
            (piece == 'N' && (rank == 1 || rank == 2))) {
            const QString errorMessage = tr("An error occurred in MoveValidator::checkCorrectPosition. A piece that should be promoted is in an incorrect position.");
            emit errorOccurred(errorMessage);
            return; // ★ 打ち切り
        }
    }
}

// 盤面データのチェックを実行する。
void MoveValidator::validateBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    // 二歩が存在する場合
    checkDoublePawn(boardData);

    // 各駒の数が最大数を超えている場合
    checkPieceCount(boardData, pieceStand);

    // 成るべき駒が不適切な位置にある場合
    checkCorrectPosition(boardData);

    // 一方の対局者につき一つの玉が存在していない場合
    checkKingPresence(boardData, pieceStand);
}

// 指し手の筋の値を検証する。
// 筋の値が有効でない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
void MoveValidator::validateMoveFileValue(int fromSquareX)
{
    // 筋の値が10より大きい場合は無効とする。
    if (fromSquareX > 10) {
        // エラーメッセージを作成する。
        const QString errorMessage = tr("An error occurred in MoveValidator::validateMoveFileValue. Validation Error: The file value of the move is incorrect.");

        // 無効な筋の値をログに出力する。
        qDebug() << "Move file value: " << fromSquareX;

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }
}

// 指し手の駒が盤面の駒と一致するかを検証する。
// 指し手の駒が盤面の駒と一致しない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
// 一致する場合はtrueを返す。
void MoveValidator::validateMovingPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData)
{
    // 指し手が将棋盤上の駒を動かす場合
    if (currentMove.fromSquare.x() < BOARD_SIZE) {
        // Y座標の境界チェック
        if (currentMove.fromSquare.y() < 0 || currentMove.fromSquare.y() >= BOARD_SIZE) {
            const QString errorMessage = tr("An error occurred in MoveValidator::validateMovingPiece. Validation Error: The rank value of the move is out of bounds.");
            qDebug() << "Move rank value: " << currentMove.fromSquare.y();
            emit errorOccurred(errorMessage);
            return;
        }

        // 指し手のマスのインデックスを計算する。
        int fromIndex = currentMove.fromSquare.y() * BOARD_SIZE + currentMove.fromSquare.x();

        // 盤面データのサイズチェック
        if (fromIndex < 0 || fromIndex >= boardData.size()) {
            const QString errorMessage = tr("An error occurred in MoveValidator::validateMovingPiece. Validation Error: The board index is out of bounds.");
            qDebug() << "Board index: " << fromIndex << ", Board data size: " << boardData.size();
            emit errorOccurred(errorMessage);
            return;
        }

        // 指し手の駒と盤面の駒が一致しない場合はエラーとする。
        if (currentMove.movingPiece != boardData[fromIndex]) {
            // エラーメッセージを作成する。
            const QString errorMessage = tr("An error occurred in MoveValidator::validateMovingPiece. Validation Error: The piece in the move does not match the piece on the square.");

            // 指し手の駒の情報をログに出力する。
            qDebug() << "Move piece: " << currentMove.movingPiece;

            // 盤面の駒の情報をログに出力する。
            qDebug() << "Piece on the square: " << boardData[fromIndex];

            emit errorOccurred(errorMessage);
            return; // ★ 打ち切り
        }
    }
}

// 駒台のエラー情報をログに出力し、エラーメッセージボックスを表示する。
void MoveValidator::logAndShowPieceStandError(const QString& errorMessage, QChar piece, const QMap<QChar, int>& pieceStand)
{
    // エラーメッセージをログに出力する。
    qWarning() << errorMessage;

    // 問題のある駒の情報をログに出力する。
    qDebug() << "Piece: " << piece;

    // 問題のある駒の駒台での数をログに出力する。
    qDebug() << "Number of pieces in the stand: " << pieceStand[piece];

    emit errorOccurred(errorMessage);
}

// 駒台の駒が有効かどうかを検証する。
// 指し手が駒台から駒を打つ場合、その駒の数が正の数かをチェックする。
// 無効な場合はエラーメッセージをログに出力し、エラーメッセージボックスを表示する。
// 有効な場合はtrueを返し、無効な場合はfalseを返す。
void MoveValidator::validatePieceStand(Turn turn, const ShogiMove& currentMove, const QMap<QChar, int>& pieceStand)
{
    // 先手の駒台から打つ場合
    if (currentMove.fromSquare.x() == BOARD_SIZE) {
        if (turn == BLACK) {
            // 先手の駒台の駒の数が正の数でない場合はエラーとする。
            if (pieceStand[currentMove.movingPiece] <= 0) {
                // エラーメッセージを作成
                const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. The number of pieces in the stand of the player moving is not positive.");

                // エラーメッセージと駒の情報をログに出力し、エラーメッセージボックスを表示する。
                logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
                return; // ★ 打ち切り
            }
        } else {
            // 後手なのに先手の駒台から駒を打とうとしている場合はエラーとする。
            const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. It's White's turn, but trying to drop a piece from Black's piece stand.");

            // エラーメッセージと駒の情報をログに出力し、エラーメッセージボックスを表示する。
            logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
            return; // ★ 打ち切り
        }
    }
    // 後手の駒台から打つ場合
    else if (currentMove.fromSquare.x() == BOARD_SIZE + 1) {
        if (turn == WHITE) {
            // 後手の駒台の駒の数が正の数でない場合はエラーとする。
            if (pieceStand[currentMove.movingPiece] <= 0) {
                // エラーメッセージを作成
                const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. The number of pieces on White's piece stand to drop is not positive.");

                // エラーメッセージと駒の情報をログに出力し、エラーメッセージボックスを表示する。
                logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
                return; // ★ 打ち切り
            }
        } else {
            // 先手なのに後手の駒台から駒を打とうとしている場合はエラーとする。
            const QString errorMessage = tr("An error occurred in MoveValidator::validatePieceStand. It's Black's turn, but trying to drop a piece from White's piece stand.");

            // エラーメッセージと駒の情報をログに出力し、エラーメッセージボックスを表示する。
            logAndShowPieceStandError(errorMessage, currentMove.movingPiece, pieceStand);
            return; // ★ 打ち切り
        }
    }
}

// 取った駒が盤面の駒と一致するかを検証する。
// 取った駒が盤面の移動先の駒と一致しない場合、エラーメッセージをログに出力し、エラーメッセージボックスを表示する。
// 一致する場合はtrueを返し、一致しない場合はfalseを返す。
void MoveValidator::validateCapturedPiece(const ShogiMove& currentMove, const QVector<QChar>& boardData)
{
    // 移動先のマスのインデックスを計算する。
    int toIndex = currentMove.toSquare.y() * BOARD_SIZE + currentMove.toSquare.x();

    // 取った駒と盤面の駒が一致しない場合はエラーとする。
    if (currentMove.capturedPiece != boardData[toIndex]) {
        // エラーメッセージを作成する。
        const QString errorMessage = tr("An error occurred in MoveValidator::validateCapturedPiece. The captured piece does not match the piece on the destination square of the board.");

        // エラーメッセージをログに出力する。
        qWarning() << errorMessage;

        // 取った駒の情報をログに出力する。
        qDebug() << "Captured piece: " << currentMove.capturedPiece;

        // 盤面の駒の情報をログに出力する。
        qDebug() << "Piece on the square: " << boardData[toIndex];

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }
}

// 複数の検証を一括して行う。
// 盤面データと持ち駒、指し手の筋、指し手の駒、駒台の駒、取った駒が有効であるかを検証する。
// 無効な場合はエラーメッセージをログに出力し、エラーメッセージボックスを表示する。
void MoveValidator::validateMoveComponents(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove)
{
    // 盤面データと持ち駒のチェックを行う。
    validateBoardAndPieces(boardData, pieceStand);

    // 指し手の筋の値を検証する。
    validateMoveFileValue(currentMove.fromSquare.x());

    // 指し手の駒が盤面の駒と一致するかを検証する。
    validateMovingPiece(currentMove, boardData);

    // 駒台の駒が有効かどうかを検証する。
    validatePieceStand(turn, currentMove, pieceStand);

    // 取った駒が盤面の駒と一致するかを検証する。
    validateCapturedPiece(currentMove, boardData);
}

// 指し手が合法手かどうかを判定する。
LegalMoveStatus MoveValidator::isLegalMove(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, ShogiMove& currentMove)
{
    // 複数の検証を一括して行う。
    // 盤面データと持ち駒、指し手の筋、指し手の駒、駒台の駒、取った駒が有効であるかを検証する。
    // 無効な場合はエラーメッセージをログに出力し、エラーメッセージボックスを表示する。
    validateMoveComponents(turn, boardData, pieceStand, currentMove);

    // 先手、後手の2種類
    // 歩、香車、桂馬、銀、金、角、飛車、玉、と金、成香、成桂、成銀、馬、龍の14種類
    // の合計28種類の駒が存在するマスを表すbitboardを作成する。
    BoardStateArray piecePlacedBitboards;
    generateBitboard(boardData, piecePlacedBitboards);

    // 先手の歩、先手の香車、先手の桂馬〜先手の龍、
    // 後手の歩、後手の香車、後手の桂馬〜後手の龍の28種類に分けて
    // 各駒ごとに駒が存在するマスを表すbitboardを作成する。
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    generateAllPieceBitboards(allPieceBitboards, boardData);

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
    //begin
    // printAllPieceBitboards(allPieceBitboards);
    //end

    // 各個々の駒が移動できるマスを表すbitboardを作成する。
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;
    generateAllIndividualPieceAttackBitboards(allPieceBitboards, piecePlacedBitboards, allPieceAttackBitboards);

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
    //begin
    // printIndividualPieceAttackBitboards(allPieceBitboards, allPieceAttackBitboards);
    //end

    // necessaryMovesBitboard
    // 例．先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
    // 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる。
    // 後手の王手を回避するために玉以外の先手の駒の移動可能なマスを表すbitboard
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 1 0
    // 0 0 0 0 0 0 1 0 0
    // 0 0 0 0 0 1 0 0 0
    // 0 0 0 0 1 0 0 0 0
    // 0 0 0 1 0 0 0 0 0
    // 0 0 1 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    std::bitset<NUM_BOARD_SQUARES> necessaryMovesBitboard;

    // 玉を除いた攻撃範囲のマスを表すbitboard
    // 上の例の場合、後手2二角を0にしたもの
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 1 0 0
    // 0 0 0 0 0 1 0 0 0
    // 0 0 0 0 1 0 0 0 0
    // 0 0 0 1 0 0 0 0 0
    // 0 0 1 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    std::bitset<NUM_BOARD_SQUARES> attackWithoutKingBitboard;

    // 相手の手番に仮設定
    Turn opponentTurn = (turn == BLACK) ? WHITE : BLACK;

    // 局面がすでに相手玉に王手をかけている状態かどうかを調べる。
    // numOpponentChecks = 0: 手番の玉は王手されていない。
    // numOpponentChecks = 1: 手番の玉は相手の1つの駒から王手されている。
    // numOpponentChecks = 2: 手番の玉は相手の駒から両王手されている。
    int numOpponentChecks = isKingInCheck(opponentTurn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    // 王手をかけている局面でさらに指すのはおかしいのでエラーとなる。
    if (numOpponentChecks) {
        // 局面がすでに王手をかけた状態になっている。さらに指すのはおかしい。
        const QString errorMessage = tr("An error occurred in MoveValidator::isLegalMove. Validation Error: The position is already in check. Making another move is incorrect.");

        // 手番の玉が王手されているかどうかの判定値
        qDebug() << "numOpponentChecks = " << numOpponentChecks;

        // 局面を漢字で出力する。
        printBoardAndPieces(boardData, pieceStand);

        // エラーメッセージを表示する。
        emit errorOccurred(errorMessage);

        LegalMoveStatus legalMoveStatus;
        legalMoveStatus.promotingMoveExists = false;
        legalMoveStatus.nonPromotingMoveExists = false;
        return legalMoveStatus; // ★ 打ち切り
    }

    // 手番の玉が王手されているかどうかを調べる。
    // numchecks = 0: 手番の玉は王手されていない。
    // numchecks = 1: 手番の玉は相手の1つの駒から王手されている。
    // numchecks = 2: 手番の玉は相手の駒から両王手されている。
    int numChecks = isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    //begin
    // qDebug() << "numChecks = " << numChecks;
    // qDebug() << (turn == BLACK ? "先手" : "後手") << "の玉が王手されている: " << (numChecks ? "はい" : "いいえ");
    //end

    // 盤上の駒を動かして指す合法手リスト
    QVector<ShogiMove> legalMovesList;

    //begin
    // std::cout << "Current Shogi move: " << currentMove << std::endl;
    //end

    // 指し手が合法手かどうか判定する。
    // 合法手ならtrue、合法手でないならfalseを返す。
    return validateMove(numChecks, boardData, currentMove, turn, piecePlacedBitboards, pieceStand, necessaryMovesBitboard, legalMovesList,
                        opponentTurn, attackWithoutKingBitboard);
}

// 将棋盤内の駒を動かした場合の合法手がどうかを判定する。
LegalMoveStatus MoveValidator::isBoardMoveValid(const Turn& turn, const QVector<QChar>& boardData, const ShogiMove& currentMove,
                                                const int& numChecks, const QChar& piece, const std::bitset<NUM_BOARD_SQUARES>& attackBitboard,
                                                const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard, const std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                                QVector<ShogiMove>& legalMovesList)
{
    // 指定された駒を動かした場合の全指し手リスト生成
    QVector<ShogiMove> allBoardPieceMovesList;

    generateShogiMoveFromBitboard(piece, pieceBitboard, attackBitboard, allBoardPieceMovesList, boardData);

    // 考慮すべき指し手のリストを保存する。王手されていない場合は全ての手を、
    // 王手されている場合は王手を防ぐ可能性のある手だけを保存する。
    QVector<ShogiMove> relevantMovesList;

    if (numChecks) {
        // 手番の玉は王手されている（両王手も含む）。
        // 王手をかけられた側は、片方の駒を玉以外で取るか、あるいは合駒をしても、もう一方の駒で玉を取られてしまうので、
        // 応手としては玉を動かすしかない。
        if (numChecks == 2 && currentMove.movingPiece != ((turn == BLACK) ? 'K' : 'k')) {           
            // エラー発生。指し手の駒が玉ではない。
            const QString errorMessage = tr("An error occurred in MoveValidator::isBoardMoveValid. The piece in the move is not a king.");

            // 指し手の駒
            qDebug() << "Piece: " << currentMove.movingPiece;

            emit errorOccurred(errorMessage);
            LegalMoveStatus legalMoveStatus;
            legalMoveStatus.promotingMoveExists = false;
            legalMoveStatus.nonPromotingMoveExists = false;
            return legalMoveStatus; // ★ 打ち切り
        }

        // 王手されているときに王手を避ける指し手リスト候補を生成する。
        // 例.先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
        // 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる手を生成。
        // また、玉を移動させる手も生成するが王手が回避できていない手も含まれている。
        // 王手が回避できない手は、後の処理で除去している。
        // 1 Shogi move: From: (6, 7) To: (6, 6) Moving Piece: P Captured Piece:   Promotion: false
        // 2 Shogi move: From: (8, 9) To: (7, 7) Moving Piece: N Captured Piece:   Promotion: false
        // 3 Shogi move: From: (8, 8) To: (9, 8) Moving Piece: K Captured Piece:   Promotion: false
        // 4 Shogi move: From: (8, 8) To: (7, 8) Moving Piece: K Captured Piece:   Promotion: false
        filterMovesThatBlockThreat(turn, allBoardPieceMovesList, necessaryMovesBitboard, relevantMovesList);
    } else {
        // 手番の玉は王手されていない。
        relevantMovesList = allBoardPieceMovesList;
    }

    // 合法手リストを作成する。
    filterLegalMovesList(turn, relevantMovesList, boardData, legalMovesList);

    // 指し手が合法手の中にあるかどうかをチェックする。
    // 合法手に一致すればtrueを返し、無ければfalseを返す。
    return checkLegalMoveStatus(currentMove, legalMovesList);
}

// 駒台の駒を打った手が合法手かどうかを判定する。
bool MoveValidator::isHandPieceMoveValid(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                                         const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList)
{
    switch(numChecks) {
    case 0:
        // 自玉に王手がかかっていない。
        // 打った駒が歩かそれ以外の駒かを判別して合法手を生成する。
        if (!generateLegalMovesForPiece(turn, boardData, pieceStand, currentMove, emptySquareBitboard, legalMovesList)) {
            // エラーが発生した場合、falseを返す。
            return false;
        }

        break;

    case 1: {
        // 自玉に王手がかかっている。ただし両王手ではない。
        // 合法手の候補リスト
        QVector<ShogiMove> candidateLegalMovesList;

        // 打った駒が歩かそれ以外の駒かを判別して合法手を生成する。
        if (!generateLegalMovesForPiece(turn, boardData, pieceStand, currentMove, emptySquareBitboard, candidateLegalMovesList)) {
            // エラーが発生した場合、falseを返す。
            return false;
        }

        // 合法手リストを作成する
        filterLegalMovesList(turn, candidateLegalMovesList, boardData, legalMovesList);

        break;
    }

    case 2: {
        // エラー発生。両王手の場合、駒台から駒を打つ手はあり得ない。
        const QString errorMessage = tr("An error occurred in MoveValidator::isHandPieceMoveValid. In the case of double check, dropping a piece from the piece stand is not possible.");

        emit errorOccurred(errorMessage);
        return false; // ★ 打ち切り
    }

    default: {
        // エラー発生。手番の玉に王手をかけている駒の数が3枚以上あり、おかしい。
        const QString errorMessage = tr("An error occurred in MoveValidator::isHandPieceMoveValid. The number of pieces putting the player's king in check is 3 or more.");

        qDebug() << "Number of pieces putting the player's king in check: " << numChecks;

        // エラーメッセージを表示する。
        emit errorOccurred(errorMessage);
        return false; // ★ 打ち切り
    }
    }

    // 指し手が合法手の中にあるかどうかをチェックする。
    // 合法手に一致すればtrueを代入し、無ければfalseを代入する。
    return isMoveInList(currentMove, legalMovesList);
}

// 打った駒が歩かそれ以外の駒かを判別して合法手を生成する。
bool MoveValidator::generateLegalMovesForPiece(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand, const ShogiMove& currentMove,
                                               const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, QVector<ShogiMove>& legalMovesList)
{
    // 打った駒
    QChar dropPiece = currentMove.movingPiece;

    // 駒台に駒が存在しない場合は、エラーを出力して処理を終了する。
    if (pieceStand[dropPiece] <= 0) {
        // エラー発生。打った駒が駒台に存在しない。
        const QString errorMessage = tr("An error occurred in MoveValidator::generateLegalMovesForPiece. The piece to be dropped does not exist in the piece stand.");

        // 打った駒
        qDebug() << "Piece: " << dropPiece;

        // エラーメッセージを表示する。
        emit errorOccurred(errorMessage);
        return false; // ★ 打ち切り
    }

    switch (dropPiece.toUpper().toLatin1()) {
    case 'P':
        // 歩を打った場合の指し手を生成する。
        generateDropMoveForPawn(legalMovesList, pieceStand, emptySquareBitboard, turn, boardData);

        break;

    case 'L':
    case 'N':
    case 'S':
    case 'G':
    case 'B':
    case 'R':
        // 歩以外の駒を打った場合の指し手を生成する。
        generateDropMoveForPiece(legalMovesList, pieceStand, emptySquareBitboard, turn, dropPiece);

        break;

    default:
        // エラー発生。打った駒を表す文字がおかしい。
        const QString errorMessage = tr("An error occurred in MoveValidator::generateLegalMovesForPiece. The character representing the piece to be dropped is incorrect.");

        // 打った駒
        qDebug() << "Piece: " << dropPiece;

        // エラーメッセージを表示する。
        emit errorOccurred(errorMessage);
        return false; // ★ 打ち切り
    }

    return true;
}

 // 歩を打った場合の指し手を生成する。
void MoveValidator::generateDropMoveForPawn(QVector<ShogiMove>& legalMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                            const Turn& turn, const QVector<QChar>& boardData) const
{
    // 歩が無い筋を1で表すbitboard
    std::bitset<NUM_BOARD_SQUARES> emptyFilePawnBitboard;

    // 歩が無い筋を1で表すbitboardの作成
    generateBitboardForEmptyFiles(turn, boardData, emptyFilePawnBitboard);

    // 駒が置かれていないマスを表すemptySquareBitboardと歩が無い筋を1で表すemptyFilePawnBitboardをAND演算したbitboard
    std::bitset<NUM_BOARD_SQUARES> emptySquaresNoPawnsFilesBitboard;

    // 駒が置かれていないマスを表すemptySquareBitboardと歩が無い筋を1で表すemptyFilePawnBitboardをAND演算した
    // emptySquaresNoPawnsFilesBitboardを作成する。
    performAndOperation(emptySquareBitboard, emptyFilePawnBitboard, emptySquaresNoPawnsFilesBitboard);

    // 歩を駒台から打つ場合の指し手を生成し、全指し手リストlegalMovesListに追加する。
    generateDropPawnMoves(legalMovesList, pieceStand, emptySquaresNoPawnsFilesBitboard, turn);
}

// 指定局面での合法手を生成する。
int MoveValidator::generateLegalMoves(const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand)
{
    // 盤面データと持ち駒のチェック
    validateBoardAndPieces(boardData, pieceStand);

    // 先手、後手の2種類
    // 歩、香車、桂馬、銀、金、角、飛車、玉、と金、成香、成桂、成銀、馬、龍の14種類
    // の合計28種類の駒が存在するマスを表すbitboardを作成する。
    BoardStateArray piecePlacedBitboards;
    generateBitboard(boardData, piecePlacedBitboards);

    // piecePlacedBitboardsの出力
    // 例．平手の場合、1〜9筋の7段目に先手の歩が9枚並んでいる。
    // 先手:
    // Piece type 歩:
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 1 1 1 1 1 1 1 1 1
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    //begin
    // printBitboards(piecePlacedBitboards);
    //end

    // 先手の歩、先手の香車、先手の桂馬〜先手の龍、
    // 後手の歩、後手の香車、後手の桂馬〜後手の龍の28種類に分けて
    // 各駒ごとに駒が存在するマスを表すbitboardを作成する。
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    generateAllPieceBitboards(allPieceBitboards, boardData);

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
    //begin
    // printAllPieceBitboards(allPieceBitboards);
    //end

    // 各個々の駒が移動できるマスを表すbitboardを作成する。
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;
    generateAllIndividualPieceAttackBitboards(allPieceBitboards, piecePlacedBitboards, allPieceAttackBitboards);

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
    //begin
    // printIndividualPieceAttackBitboards(allPieceBitboards, allPieceAttackBitboards);
    //end

    // 盤上の駒を動かした場合の全指し手リスト生成
    QVector<ShogiMove> allBoardPieceMovesList;

    // 盤上の駒を動かした場合の全指し手リスト生成
    // allPieceBitboardsは、手番のbitboardのみ必要
    // allPieceAttackBitboardsは、手番のbitboardのみ必要
    generateShogiMoveFromBitboards(turn, allPieceBitboards, allPieceAttackBitboards, allBoardPieceMovesList, boardData);

    // 例．先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
    // 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる。
    // 後手の王手を回避するために玉以外の先手の駒の移動可能なマスを表すbitboard
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 1 0
    // 0 0 0 0 0 0 1 0 0
    // 0 0 0 0 0 1 0 0 0
    // 0 0 0 0 1 0 0 0 0
    // 0 0 0 1 0 0 0 0 0
    // 0 0 1 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> necessaryMovesBitboard;

    // 玉を除いた攻撃範囲のマスを表すbitboard
    // 上の例の場合、後手2二角を0にしたもの
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    // 0 0 0 0 0 0 1 0 0
    // 0 0 0 0 0 1 0 0 0
    // 0 0 0 0 1 0 0 0 0
    // 0 0 0 1 0 0 0 0 0
    // 0 0 1 0 0 0 0 0 0
    // 0 0 0 0 0 0 0 0 0
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> attackWithoutKingBitboard;

    // 相手の手番に仮設定
    Turn opponentTurn = (turn == BLACK) ? WHITE : BLACK;

    // 局面がすでに相手玉に王手をかけている状態かどうかを調べる。
    // numOpponentChecks = 0: 手番の玉は王手されていない。
    // numOpponentChecks = 1: 手番の玉は相手の1つの駒から王手されている。
    // numOpponentChecks = 2: 手番の玉は相手の駒から両王手されている。
    int numOpponentChecks = isKingInCheck(opponentTurn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    // 王手をかけている局面でさらに指すのはおかしいのでエラーとなる。
    if (numOpponentChecks) {
        // エラー発生。局面がすでに王手をかけた状態になっている。さらに指すのはおかしい。
        const QString errorMessage = tr("An error occurred in MoveValidator::generateLegalMoves. The position is already in check. Making another move is incorrect.");

        printBoardAndPieces(boardData, pieceStand);

        // エラーメッセージを表示する。
        emit errorOccurred(errorMessage);
        return 0; // ★ 打ち切り
    }

    // 手番の玉が王手されているかどうかを調べる。
    // numChecks = 0: 手番の玉は王手されていない。
    // numChecks = 1: 手番の玉は相手の1つの駒から王手されている。
    // numChecks = 2: 手番の玉は相手の駒から両王手されている。
    int numChecks = isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);

    //begin
    // qDebug() << "numChecks = " << numChecks;
    // qDebug() << (turn == BLACK ? "先手" : "後手") << "の玉が王手されている: " << (numChecks ? "はい" : "いいえ");
    //end

    // 盤上の駒を動かして指す合法手リスト
    QVector<ShogiMove> legalMovesList;

    // 駒が置かれていないマスを表すbitboard
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> emptySquareBitboard;

    if (numChecks) {
        // 自玉が王手されている場合
        // 王手されているときに王手を避ける指し手リスト
        QVector<ShogiMove> kingBlockingMovesList;

        // 王手されているときに王手を避ける指し手リスト候補を生成する。
        // 例.先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
        // 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる手を生成。
        // また、玉を移動させる手も生成するが王手が回避できていない手も含まれている。
        // 王手が回避できない手は、後の処理で除去している。
        // 1 Shogi move: From: (6, 7) To: (6, 6) Moving Piece: P Captured Piece:   Promotion: false
        // 2 Shogi move: From: (8, 9) To: (7, 7) Moving Piece: N Captured Piece:   Promotion: false
        // 3 Shogi move: From: (8, 8) To: (9, 8) Moving Piece: K Captured Piece:   Promotion: false
        // 4 Shogi move: From: (8, 8) To: (7, 8) Moving Piece: K Captured Piece:   Promotion: false
        filterMovesThatBlockThreat(turn, allBoardPieceMovesList, necessaryMovesBitboard, kingBlockingMovesList);

        // 指し手リストkingBlockingMovesListの各指し手moveに対して指した直後の盤面データを作成し、
        // その局面で相手が盤上の駒を動かした際の手を生成して、その中に自玉に王手が掛かっていない手だけを
        // 合法手リストlegalMovesListに加える。
        filterLegalMovesList(turn, kingBlockingMovesList, boardData, legalMovesList);

        if (numChecks == 2) {
            // 相手から両王手が掛けられた場合、基本的に自玉を直接動かすことでしか対処できない。
            // 合い駒（自分の手番で駒を打って王手を防ぐ）で防ごうとすると、相手の次の手番でその合い駒を取られてしまい、
            // 再度王手となってしまう。それだけでなく、合い駒を打つことはルールで禁止されている。
            // よって打つ手は生成できない。
            // 全指し手リストの出力
            //begin
            // qDebug() << "";
            // printShogiBoard(boardData);
            // qDebug() << "全指し手リスト";
            // printShogiMoveList(legalMovesList);
            //end
            return static_cast<int>(legalMovesList.size());
        }

        // 駒台から駒を打てるマスは玉と王手を掛けている駒のマスを除いた攻撃範囲のマスのみ
        emptySquareBitboard = attackWithoutKingBitboard;
    } else {
        // 自玉が王手されていない場合
        // 指し手リストallBoardPieceMovesListの各指し手moveに対して指した直後の盤面データを作成し、
        // その局面で相手が盤上の駒を動かした際の手を生成して、その中に自玉に王手が掛かっていない手だけを
        // 合法手リストlegalMovesListに加える。
        filterLegalMovesList(turn, allBoardPieceMovesList, boardData, legalMovesList);

        // 駒が置かれていないマスを表すbitboardを作成する。
        generateEmptySquareBitboard(boardData, emptySquareBitboard);
    }

    // 歩以外の駒を駒台から打つ場合の指し手を生成し、全指し手リストlegalMovesListに追加する。
    generateDropNonPawnMoves(legalMovesList, pieceStand, emptySquareBitboard, turn);

    // 歩を打った場合の指し手を生成し、legalMovesListに追加する。
    generateDropMoveForPawn(legalMovesList, pieceStand, emptySquareBitboard, turn, boardData);

    // 全指し手リストの出力
    //begin
    // qDebug() << "";
    // printShogiBoard(boardData);
    // qDebug() << "全指し手リスト";
    // printShogiMoveList(legalMovesList);
    //end

    return static_cast<int>(legalMovesList.size());
}

// 駒が置かれていないマスを表すemptySquareBitboardと歩が無い筋を1で表すemptyFilePawnBitboardをAND演算した
// emptySquaresNoPawnsFilesBitboardを作成する。
void MoveValidator::performAndOperation(const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const std::bitset<NUM_BOARD_SQUARES>& emptyFilePawnBitboard,
                         std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard) const
{
    emptySquaresNoPawnsFilesBitboard = emptySquareBitboard & emptyFilePawnBitboard;

    //begin
    // printSingleBitboard(emptySquaresNoPawnsFilesBitboard);
    //end

}

// 指定手番の玉が王手されているかどうかを判定する。
// 戻り値: 王手の数（0=王手なし, 1=王手, 2=両王手）
int MoveValidator::checkIfKingInCheck(const Turn& turn, const QVector<QChar>& boardData)
{
    // 先手、後手の2種類
    // 歩、香車、桂馬、銀、金、角、飛車、玉、と金、成香、成桂、成銀、馬、龍の14種類
    // の合計28種類の駒が存在するマスを表すbitboardを作成する。
    BoardStateArray piecePlacedBitboards;
    generateBitboard(boardData, piecePlacedBitboards);

    // 先手の歩、先手の香車、先手の桂馬〜先手の龍、
    // 後手の歩、後手の香車、後手の桂馬〜後手の龍の28種類に分けて
    // 各駒ごとに駒が存在するマスを表すbitboardを作成する。
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    generateAllPieceBitboards(allPieceBitboards, boardData);

    // 各個々の駒が移動できるマスを表すbitboardを作成する。
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;
    generateAllIndividualPieceAttackBitboards(allPieceBitboards, piecePlacedBitboards, allPieceAttackBitboards);

    // 王手を回避するために必要な手を表すbitboard
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> necessaryMovesBitboard;

    // 玉を除いた攻撃範囲のマスを表すbitboard
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> attackWithoutKingBitboard;

    // 手番の玉が王手されているかどうかを調べる。
    return isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);
}

// 歩が無い筋を1で表すbitboardを作成する。
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

    //begin
    // printSingleBitboard(emptyFilePawnBitboard);
    //end
}

// 先手、後手の2種類
// 歩、香車、桂馬、銀、金、角、飛車、玉、と金、成香、成桂、成銀、馬、龍の14種類
// の合計28種類の駒が存在するマスを表すbitboardを作成する。
void MoveValidator::generateBitboard(const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboard) const
{
    for (qsizetype i = 0; i < boardData.size(); ++i) {
        QChar piece = boardData.at(i);
        Turn turnPiece = piece.isUpper() ? BLACK : WHITE;
        piece = piece.toUpper();

        switch (piece.unicode()) {
        // 歩
        case 'P':
            piecePlacedBitboard[turnPiece][PAWN].set(static_cast<size_t>(i), true);
            break;
        // 香車
        case 'L':
            piecePlacedBitboard[turnPiece][LANCE].set(static_cast<size_t>(i), true);
            break;
        // 桂馬
        case 'N':
            piecePlacedBitboard[turnPiece][KNIGHT].set(static_cast<size_t>(i), true);
            break;
        // 銀
        case 'S':
            piecePlacedBitboard[turnPiece][SILVER].set(static_cast<size_t>(i), true);
            break;
        // 金
        case 'G':
            piecePlacedBitboard[turnPiece][GOLD].set(static_cast<size_t>(i), true);
            break;
        // 角
        case 'B':
            piecePlacedBitboard[turnPiece][BISHOP].set(static_cast<size_t>(i), true);
            break;
        // 飛車
        case 'R':
            piecePlacedBitboard[turnPiece][ROOK].set(static_cast<size_t>(i), true);
            break;
        // 玉
        case 'K':
            piecePlacedBitboard[turnPiece][KING].set(static_cast<size_t>(i), true);
            break;
        // と金
        case 'Q':
            piecePlacedBitboard[turnPiece][PROMOTED_PAWN].set(static_cast<size_t>(i), true);
            break;
        // 成香
        case 'M':
            piecePlacedBitboard[turnPiece][PROMOTED_LANCE].set(static_cast<size_t>(i), true);
            break;
        // 成桂
        case 'O':
            piecePlacedBitboard[turnPiece][PROMOTED_KNIGHT].set(static_cast<size_t>(i), true);
            break;
        // 成銀
        case 'T':
            piecePlacedBitboard[turnPiece][PROMOTED_SILVER].set(static_cast<size_t>(i), true);
            break;
        // 馬
        case 'C':
            piecePlacedBitboard[turnPiece][HORSE].set(static_cast<size_t>(i), true);
            break;
        // 龍
        case 'U':
            piecePlacedBitboard[turnPiece][DRAGON].set(static_cast<size_t>(i), true);
            break;
        }
    }
}

// piecePlacedBitboardを標準出力する。
void MoveValidator::printBitboards(const BoardStateArray& piecePlacedBitboard) const
{
    // 将棋の各駒の名称
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

                    // std::bitsetのtest()メンバ関数は、指定されたインデックスのビットが1(true)であるか、
                    // 0(false)であるかをチェックするために使用される。test()関数は、指定されたインデックスに
                    // あるビットがセットされている場合に true を返し、そうでない場合に false を返す。
                    // このコードにおいて、QString::number(bitboard.test(static_cast<size_t>(index))) + ' ')は、
                    // bitboardのindex位置にあるビットがセットされているかどうかを出力する。
                    // セットされていれば1が出力され、セットされていなければ0が出力される。
                    // これにより、駒のあるマスと無いマスを1と0で表示することができる。
                    bitboardRow.append(QString::number(bitboard.test(static_cast<size_t>(index))) + ' ');
                }
                qDebug() << bitboardRow;
            }
            qDebug() << "";
        }
    }
}

// 自分の各駒が存在するbitboard中でindexで指定されたマスに味方の駒が存在するかどうかを判定する。
bool MoveValidator::isPieceOnSquare(int index, const std::array<std::bitset<NUM_BOARD_SQUARES>, NUM_PIECE_TYPES>& turnBitboards) const
{
    for (int pieceType = 0; pieceType < NUM_PIECE_TYPES; ++pieceType) {
        if (turnBitboards[static_cast<size_t>(pieceType)].test(static_cast<size_t>(index))) {
            return true;
        }
    }

    return false;
}

// 盤面データboardDataからtargetPieceに該当する駒が存在するマスを表すbitboardを各駒ごとに作成し、そのリストを返す。
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
void MoveValidator::generateAllPieceBitboards(QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards, const QVector<QChar>& boardData) const
{
    allPieceBitboards.clear();

    for (qsizetype i = 0; i < m_allPieces.size(); ++i) {
        const QChar& targetPiece = m_allPieces.at(i);
        allPieceBitboards.append(generateIndividualPieceBitboards(boardData, targetPiece));
    }
}

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
void MoveValidator::printIndividualPieceAttackBitboards(const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                                        const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards) const
{
    for (qsizetype i = 0; i < allPieceBitboards.size(); ++i) {
        //begin
        // QChar piece = allPieces.at(i);
        // Turn turnPiece = piece.isUpper() ? BLACK : WHITE;
        // qDebug() << "attack";
        // qDebug() << "Piece: " << piece;
        // qDebug() << (turnPiece == 0 ? "先手:" : "後手:");
        //end
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

// bitboardリストから各bitboardを取り出し、そのbitboardの内容を標準出力する。
void MoveValidator::printBitboardContent(const QVector<std::bitset<NUM_BOARD_SQUARES>>& bitboards) const
{
    for (const auto& bitboard : std::as_const(bitboards)) {
        printSingleBitboard(bitboard);
    }
}

// bitboardの内容を標準出力する。
// bitboardの内容を標準出力する。
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

// 指し手リストから各指し手を標準出力する。
void MoveValidator::printShogiMoveList(const QVector<ShogiMove>& moveList) const
{
    int i = 0;
    for (const auto& move : std::as_const(moveList)) {
        std::cout << std::setw(3) << std::setfill(' ') << ++i << " Shogi move: " << move << std::endl;
    }
}

// 盤上の駒を動かした場合の全指し手リスト生成
void MoveValidator::generateShogiMoveFromBitboards(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                                   const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                                                   QVector<ShogiMove>& allMovesList, const QVector<QChar>& boardData) const
{
    int start = (turn == BLACK) ? P_IDX : p_IDX;
    int end = (turn == BLACK) ? U_IDX + 1 : u_IDX + 1;

    for (int i = start; i < end; ++i) {
        QChar piece = m_allPieces.at(i);

        //begin
        // qDebug() << "Piece: " << piece;
        // qDebug() << (turn == BLACK ? "先手:" : "後手:");
        // printBitboardContent(allPieceBitboards[i]);
        //end

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

                        bool disallowedMove =
                                (piece == 'P' && rank == 0) ||
                                (piece == 'p' && rank == 8) ||
                                (piece == 'L' && rank == 0) ||
                                (piece == 'l' && rank == 8) ||
                                (piece == 'N' && (rank == 0 || rank == 1)) ||
                                (piece == 'n' && (rank == 7 || rank == 8));

                        // 成らない手の生成
                        if (!disallowedMove) {
                            allMovesList.append(move);
                        }

                        // 成る手の生成
                        generateAllPromotions(move, allMovesList);
                    }
                }
            }
        }
    }
    //begin
    // printShogiBoard(boardData);
    // printShogiMoveList(allMovesList);
    //end
}

// 各駒が存在するマスの位置を示すsinglePieceBitboardから移動可能なマスの位置を示すattackBitboardを生成する。
void MoveValidator::generateSinglePieceAttackBitboard(std::bitset<MoveValidator::NUM_BOARD_SQUARES>& attackBitboard,
                                                      const std::bitset<MoveValidator::NUM_BOARD_SQUARES>& singlePieceBitboard,
                                                      const Turn& turn, QChar pieceType, const BoardStateArray& piecePlacedBitboard) const
{
    // singlePieceBitboardの全てのビットを反復処理する。
    for (int index = 0; index < NUM_BOARD_SQUARES; ++index) {
        if (singlePieceBitboard.test(static_cast<size_t>(index))) {
            switch (pieceType.toUpper().toLatin1()) {
            case 'P':
                // 歩が存在するマスを表すbitboardから歩が移動可能なマスを表すbitboardを生成する。
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, 0}}, false, false);
                }
                break;
            case 'L':
                // 香車が存在するマスを表すbitboardから香車が移動可能なマスを表すbitboardを生成する。
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}}, true, true);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, 0}}, true, true);
                }
                break;
            case 'N':
                // 桂馬が存在するマスを表すbitboardから桂馬が移動可能なマスを表すbitboardを生成する。
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-2, -1}, {-2, 1}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{2, -1}, {2, 1}}, false, false);
                }
                break;
            case 'S':
                // 銀が存在するマスを表すbitboardから銀が移動可能なマスを表すbitboardを生成する。
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
                // 金（と金、成香、成桂、成銀）が存在するマスを表すbitboardから金（と金、成香、成桂、成銀）が移動可能なマスを表すbitboardを生成する。
                if (turn == BLACK) {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, 0}}, false, false);
                } else {
                    generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{1, -1}, {1, 0}, {1, 1}, {0, -1}, {0, 1}, {-1, 0}}, false, false);
                }
                break;
            case 'B':
                // 角が存在するマスを表すbitboardから角が移動可能なマスを表すbitboardを生成する。
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}, true, true);
                break;
            case 'R':
                // 飛車が存在するマスを表すbitboardから飛車が移動可能なマスを表すbitboardを生成する。
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}, true, true);
                break;
            case 'K':
                // 玉が存在するマスを表すbitboardから玉が移動可能なマスを表すbitboardを生成する。
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}}, false, false);
                break;
            case 'C':
                // 馬が存在するマスを表すbitboardから馬が移動可能なマスを表すbitboardを生成する。
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}, true, true);
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}, false, false);
                break;
            case 'U':
                // 龍が存在するマスを表すbitboardから歩が移動可能なマスを表すbitboardを生成する。
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, 0}, {0, -1}, {0, 1}, {1, 0}}, true, true);
                generateAttackBitboard(attackBitboard, turn, piecePlacedBitboard, singlePieceBitboard, {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}, false, false);
                break;
            default:
                break;
            }
        }
    }
}

// 特定の種類の駒が攻撃可能なマスを表すビットボードを生成する。
// continuousは、駒が連続して移動できるか（飛車、角、馬、龍など）を指定する。
// enemyOccupiedStopは、敵の駒が存在するマスに到達した時点で駒の進行を止めるかを指定する（飛車、角、馬、龍など）。
void MoveValidator::generateAttackBitboard(std::bitset<NUM_BOARD_SQUARES>& attackBitboard, const Turn& turn,
                                           const BoardStateArray& piecePlacedBitboard, const std::bitset<NUM_BOARD_SQUARES>& pieceBitboard,
                                           const QList<QPoint>& directions, bool continuous, bool enemyOccupiedStop) const
{
    // 盤面の全てのマスを確認します。
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;

            // 特定の種類の駒がそのマスに存在するかどうかをチェックする。
            if (pieceBitboard.test(static_cast<size_t>(index))) {
                // 駒が移動可能な全ての方向について確認する。
                for (const auto& direction : std::as_const(directions)) {
                    int currentRank = rank;
                    int currentFile = file;
                    int opponent = 1 - turn;

                    // 駒が移動可能な限り、指定された方向に進む。
                    while (true) {
                        currentRank += direction.x();
                        currentFile += direction.y();

                        // 移動先のマスが盤面内にあるかを確認する。
                        if (currentRank >= 0 && currentRank < BOARD_SIZE &&
                            currentFile >= 0 && currentFile < BOARD_SIZE) {
                            int targetIndex = currentRank * BOARD_SIZE + currentFile;

                            // 味方の駒がある場所には移動できないため、その場合はループを抜ける。
                            if (isPieceOnSquare(targetIndex, piecePlacedBitboard[turn])) {
                                break;
                            }

                            // 空きマスか相手の駒がある場所に移動する。
                            attackBitboard.set(static_cast<size_t>(targetIndex), true);

                            // 対戦相手の駒がある場合はその駒を取ることができるが、その先には進めないためループを抜ける。
                            if (isPieceOnSquare(targetIndex, piecePlacedBitboard[static_cast<size_t>(opponent)])) {
                                if (enemyOccupiedStop) break;
                            }

                            // 連続で動かない場合（飛車や角など）は、ここでループを抜ける。
                            if (!continuous) {
                                break;
                            }
                        } else {
                            // 盤の外に出たらループを抜けます。
                            break;
                        }
                    }
                }
            }
        }
    }
    //begin
    // printSingleBitboard(attackBitboard);
    //end
}

// 王手されているときに王手を避ける指し手リスト候補を生成する。
// 例.先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
// 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる手を生成。
// また、玉を移動させる手も生成するが王手が回避できていない手も含まれている。
// 王手が回避できない手は、後の処理で除去している。
// 1 Shogi move: From: (6, 7) To: (6, 6) Moving Piece: P Captured Piece:   Promotion: false
// 2 Shogi move: From: (8, 9) To: (7, 7) Moving Piece: N Captured Piece:   Promotion: false
// 3 Shogi move: From: (8, 8) To: (9, 8) Moving Piece: K Captured Piece:   Promotion: false
// 4 Shogi move: From: (8, 8) To: (7, 8) Moving Piece: K Captured Piece:   Promotion: false
void MoveValidator::filterMovesThatBlockThreat(const Turn& turn, const QVector<ShogiMove>& allMovesList, const std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                               QVector<ShogiMove>& kingBlockingMovesList) const
{
    kingBlockingMovesList.clear();
    QChar kingPiece = (turn == BLACK) ? 'K' : 'k';

    for (const auto &move : allMovesList) {
        // マスの番号を9x9ボードに基づいて計算します。
        int squareIndex = move.toSquare.y() * BOARD_SIZE + move.toSquare.x();

        // そのマスがビットボードで1になっているかどうかを確認します。
        // また、動かす駒が玉の場合も手を追加します。
        if (necessaryMovesBitboard[static_cast<size_t>(squareIndex)] || move.movingPiece == kingPiece) {
            kingBlockingMovesList.append(move);
        }
    }
    //begin
    // printShogiMoveList(kingBlockingMovesList);
    //end
}

// 手番の玉が王手されているかどうかを調べる。
int MoveValidator::isKingInCheck(const Turn& turn, const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceBitboards,
                                  const QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>>& allPieceAttackBitboards,
                                  std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                  std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard)
{
    // bitboardの初期化
    necessaryMovesBitboard.reset();
    attackWithoutKingBitboard.reset();

    int kingIndex = turn == BLACK ? K_IDX : k_IDX;

    // 玉が盤上に存在しない不正な局面の場合はエラー
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
                //begin
                // qDebug() << "piece = " << allPieces[i];
                //end
                // 玉を除いた攻撃範囲のマスを表すbitboard
                attackWithoutKingBitboard = attackBitboard & ~kingBitboard;

                // 玉を除いた攻撃範囲のマスを表すbitboardと王手をかけている相手の駒の位置を表すbitboardをOR演算する。
                // これにより王手を防ぐために駒を置きたいマスと王手を掛けている相手の駒の位置のマスを表すbitboardが生成できる。
                // このnecessaryMovesBitboardは、numChecksが1のときのみ必要なbitboardで
                // 両王手すなわちnumChecksが2のときは、意味のないbitboardになってしまう。
                necessaryMovesBitboard = attackWithoutKingBitboard | allPieceBitboards[i][j];
                ++numChecks;
            }
        }
    }

    if (numChecks > 2) {
        // エラー発生。手番の玉は相手の3つ以上の駒から同時に王手されている。
        const QString errorMessage = tr("An error occurred in MoveValidator::isKingInCheck. The player's king is checked by three or more pieces at the same time.");

        emit errorOccurred(errorMessage);
        return numChecks;
    }

    //begin
    // qDebug() << "numChecks = " << numChecks;
    // printSingleBitboard(attackWithoutKingBitboard);
    // printSingleBitboard(necessaryMovesBitboard);
    //end
    return numChecks;
}

// 指し手リストmoveListの各指し手moveに対して指した直後の盤面データboardDataAfterMoveを作成し、
// その局面で相手が盤上の駒を動かした際の手を生成して、その中に自玉に王手が掛かっていない手だけを
// 合法手リストlegalMovesListに加える。
void MoveValidator::filterLegalMovesList(const Turn& turn, const QVector<ShogiMove>& moveList, const QVector<QChar>& boardData, QVector<ShogiMove>& legalMovesList)
{
    // 合法手リストのクリア
    legalMovesList.clear();

    // 指した直後の盤面データ
    QVector<QChar> boardDataAfterMove;

    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceBitboards;
    QVector<QVector<std::bitset<NUM_BOARD_SQUARES>>> allPieceAttackBitboards;

    // 指し手リストのループ
    for (const auto& move : std::as_const(moveList)) {
        // 1手指した後の局面データを用意する。
        applyMovesToBoard(move, boardData, boardDataAfterMove);

        // 先手の歩、先手の香車、先手の桂馬〜先手の龍、
        // 後手の歩、後手の香車、後手の桂馬〜後手の龍の28種類に分けて
        // 各駒ごとに駒が存在するマスを表すbitboardを作成する。
        generateAllPieceBitboards(allPieceBitboards, boardDataAfterMove);
        //begin
        // printAllPieceBitboards(allPieceBitboards);
        //end

        // 先手、後手の2種類
        // 歩、香車、桂馬、銀、金、角、飛車、玉、と金、成香、成桂、成銀、馬、龍の14種類
        // の合計28種類の駒が存在するマスを表すbitboardを作成する。
        BoardStateArray bitboard;
        generateBitboard(boardDataAfterMove, bitboard);

        // 各個々の駒が移動できるマスを表すbitboardを作成する。
        generateAllIndividualPieceAttackBitboards(allPieceBitboards, bitboard, allPieceAttackBitboards);
        //begin
        // printIndividualPieceAttackBitboards(allPieceBitboards, allPieceAttackBitboards);
        //end

        // 手番の指定
        Turn opponentTurn = (turn == BLACK) ? WHITE : BLACK;
        QVector<ShogiMove> allMovesList;

        // 盤上の駒を動かした場合の全指し手リスト生成
        generateShogiMoveFromBitboards(opponentTurn, allPieceBitboards, allPieceAttackBitboards, allMovesList, boardDataAfterMove);

        // necessaryMovesBitboard
        // 例．先手8八玉が後手2二角から王手を受けている場合、2二、3三、4四、5五、6六、7七に
        // 玉以外の先手の駒をそのマスに指すことができれば後手の王手を回避できる。
        // 後手の王手を回避するために玉以外の先手の駒の移動可能なマスを表すbitboard
        // 0 0 0 0 0 0 0 0 0
        // 0 0 0 0 0 0 0 1 0
        // 0 0 0 0 0 0 1 0 0
        // 0 0 0 0 0 1 0 0 0
        // 0 0 0 0 1 0 0 0 0
        // 0 0 0 1 0 0 0 0 0
        // 0 0 1 0 0 0 0 0 0
        // 0 0 0 0 0 0 0 0 0
        // 0 0 0 0 0 0 0 0 0
        std::bitset<NUM_BOARD_SQUARES> necessaryMovesBitboard;

        // 玉を除いた攻撃範囲のマスを表すbitboard
        std::bitset<NUM_BOARD_SQUARES> attackWithoutKingBitboard;

        // 手番の玉が王手されているかどうかを調べる。
        // numchecks = 0: 手番の玉は王手されていない。
        // numchecks = 1: 手番の玉は相手の1つの駒から王手されている。
        // numchecks = 2: 手番の玉は相手の駒から両王手されている。
        int numchecks = isKingInCheck(turn, allPieceBitboards, allPieceAttackBitboards, necessaryMovesBitboard, attackWithoutKingBitboard);
        //begin
        // qDebug() << (turn == BLACK ? "先手" : "後手") << "の玉が王手されている: " << (numchecks ? "はい" : "いいえ");
        //end

        // 玉が王手状態でない場合、そのmoveをlegalMovesListに追加
        if (!numchecks) {
            legalMovesList.append(move);
        }
    }
}

// 駒が置かれていないマスを表すbitboardを作成する。
void MoveValidator::generateEmptySquareBitboard(const QVector<QChar>& boardData, std::bitset<MoveValidator::NUM_BOARD_SQUARES>& emptySquareBitboard) const
{
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;
            // 駒が置かれていないマスを１とする
            if (boardData[index] == ' ') {
                emptySquareBitboard.set(static_cast<size_t>(index));
            }
        }
    }
    //begin
    // printSingleBitboard(emptySquareBitboard);
    //end
}

// 駒台から駒を打つ場合の指し手を生成する。
void MoveValidator::generateDropMoveForPiece(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard,
                                             const Turn& turn, const QChar& piece) const
{
    int count = pieceStand[piece];

    // その駒が駒台に1つ以上存在する場合
    if (count > 0) {
        // ビットボードを走査して空きマス（1）を探す。
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                int index = rank * BOARD_SIZE + file;

                // そのマスが空いている場合
                if (emptySquareBitboard.test(static_cast<size_t>(index))) {
                    // 駒の種類とランクによる制限を考慮して、その駒を打つ手を生成する。
                    bool canDrop = true;

                    if ((piece == 'L' && rank == 0) || (piece == 'N' && rank < 2)) canDrop = false;
                    else if ((piece == 'l' && rank == BOARD_SIZE - 1) || (piece == 'n' && rank >= BOARD_SIZE - 2)) canDrop = false;

                    if (canDrop) {
                        // 駒台を示す座標
                        int fromFile = turn == BLACK ? BLACK_HAND_FILE : WHITE_HAND_FILE;
                        int fromRank = turn == BLACK ? m_pieceOrderBlack[piece] : m_pieceOrderWhite[piece];
                        allMovesList.append(ShogiMove(QPoint(fromFile, fromRank), QPoint(file, rank), piece, ' ', false));
                    }
                }
            }
        }
    }
    //begin
    // printShogiMoveList(allMovesList);
    //end
}

// 歩以外の駒を駒台から打つ場合の指し手を生成する。
void MoveValidator::generateDropNonPawnMoves(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquareBitboard, const Turn& turn) const
{
    QList<QChar> pieces = turn == BLACK ? QList<QChar>({'L', 'N', 'S', 'G', 'B', 'R'}) : QList<QChar>({'l', 'n', 's', 'g', 'b', 'r'});

    // 手番の持ち駒を一つずつ確認する。
    for (auto it = pieces.cbegin(); it != pieces.cend(); ++it) {
        generateDropMoveForPiece(allMovesList, pieceStand, emptySquareBitboard, turn, *it);
    }
    //begin
    // printShogiMoveList(allMovesList);
    //end
}

// 歩を駒台から打つ場合の指し手を生成する。
void MoveValidator::generateDropPawnMoves(QVector<ShogiMove>& allMovesList, const QMap<QChar, int>& pieceStand, const std::bitset<NUM_BOARD_SQUARES>& emptySquaresNoPawnsFilesBitboard,
                                          const Turn& turn) const
{
    QChar pawn = turn == BLACK ? 'P' : 'p';
    int count = pieceStand[pawn];

    // 駒が駒台に1つ以上存在する場合
    if (count > 0) {
        // ビットボードを走査して空きマス（1）を探す。
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            for (int file = BOARD_SIZE - 1; file >= 0; --file) {
                int index = rank * BOARD_SIZE + file;

                // そのマスが空いている場合
                if (emptySquaresNoPawnsFilesBitboard.test(static_cast<size_t>(index))) {
                    // 駒の種類とランクによる制限を考慮して、その駒を打つ手を生成する。
                    bool canDrop = true;
                    if (pawn == 'P' && rank == 0) canDrop = false;
                    else if (pawn == 'p' && rank == BOARD_SIZE - 1) canDrop = false;

                    if (canDrop) {
                        // 駒台を示す座標
                        int fromFile = turn == BLACK ? BLACK_HAND_FILE : WHITE_HAND_FILE;
                        int fromRank = turn == BLACK ? m_pieceOrderBlack[pawn] : m_pieceOrderWhite[pawn];
                        allMovesList.append(ShogiMove(QPoint(fromFile, fromRank), QPoint(file, rank), pawn, ' ', false));
                    }
                }
            }
        }
    }
    //begin
    // printShogiMoveList(allMovesList);
    //end
}

// 盤面データを9x9のマスに表示する。
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

// 盤上の駒を動かした場合の全指し手リストを生成する。
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

                // 成らない手の生成
                if (!disallowedMove) {
                    allMovesList.append(move);
                }

                // 成る手の生成
                generateAllPromotions(move, allMovesList);
            }
        }
    }

    //begin
    // printShogiBoard(boardData);
    // printShogiMoveList(allMovesList);
    //end
}

// 指し手が合法手の中にあるかどうかをチェックする。
// 合法手に一致すればtrueを返し、無ければfalseを返す。
bool MoveValidator::isMoveInList(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const
{
    for (const auto& move : std::as_const(movesList)) {
        if (currentMove == move) {
            return true;
        }
    }

    return false;
}

// 指し手が合法手の中にあるかどうかをチェックし、legalMoveStatusに結果を格納する。
// 合法手に一致すればtrueを代入し、無ければfalseを代入する。
LegalMoveStatus MoveValidator::checkLegalMoveStatus(const ShogiMove& currentMove, const QVector<ShogiMove>& movesList) const
{
    LegalMoveStatus legalMoveStatus;

    // 不成の指し手が存在するかどうかを確認する。
    legalMoveStatus.nonPromotingMoveExists = isMoveInList(currentMove, movesList);

    // 成りの指し手を生成する。
    ShogiMove promotingMove = currentMove;
    promotingMove.isPromotion = true;

    // 成りの指し手が合法手リストに存在するかどうかを確認する。
    legalMoveStatus.promotingMoveExists = isMoveInList(promotingMove, movesList);

    return legalMoveStatus;
}

// 盤面と持ち駒を引数として、局面を漢字で出力する。
void MoveValidator::printBoardAndPieces(const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand) const
{
    QString row;

    // 駒の表示順序を保持するための追加のデータ構造
    static const QVector<QChar> pieceOrder = {'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K', 'k', 'r', 'b', 'g', 's', 'n', 'l', 'p'};

    // 駒とその表示名を関連付けるマップ
    static const QMap<QChar, QString> pieceKanjiNames = {
        {' ', " 　"}, {'P', " 歩"}, {'L', " 香"}, {'N', " 桂"}, {'S', " 銀"}, {'G', " 金"}, {'B', " 角"}, {'R', " 飛"}, {'K', " 玉"},
        {'Q', " と"}, {'M', " 杏"}, {'O', " 圭"}, {'T', " 全"}, {'C', " 馬"}, {'U', " 龍"},
        {'p', "v歩"}, {'l', "v香"}, {'n', "v桂"}, {'s', "v銀"}, {'g', "v金"}, {'b', "v角"}, {'r', "v飛"}, {'k', "v玉"},
        {'q', "vと"}, {'m', "v杏"}, {'o', "v圭"}, {'t', "v全"}, {'c', "v馬"}, {'u', "v龍"}
    };

    // 先に後手の持ち駒を表示します。
    QString gotePieces = "持ち駒：";
    for (const auto& piece : std::as_const(pieceOrder)) {
        if (piece.isLower()) {
            gotePieces += pieceKanjiNames[piece] + " " + QString::number(pieceStand[piece]) + " ";
        }
    }
    qDebug() << gotePieces;

    // 段の漢字
    static const QVector<QString> rowKanjiNames = {"一", "二", "三", "四", "五", "六", "七", "八", "九"};

    // 次に盤面を表示します。
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

    // 最後に先手の持ち駒を表示します。
    QString sentePieces = "持ち駒：";
    for (const auto& piece : std::as_const(pieceOrder)) {
        if (piece.isUpper()) {
            sentePieces += pieceKanjiNames[piece] + " " + QString::number(pieceStand[piece]) + " ";
        }
    }
    qDebug() << sentePieces;
}

// 成って指した手を生成する。
void MoveValidator::generateAllPromotions(ShogiMove& move, QVector<ShogiMove>& promotions) const
{
    // 駒が成れるかどうかを判断するための条件
    bool canPromote = false;
    int fromRank = move.fromSquare.y();
    int toRank = move.toSquare.y();

    if (move.movingPiece == 'P' || move.movingPiece == 'L' || move.movingPiece == 'N' || move.movingPiece == 'S' ||
        move.movingPiece == 'B' || move.movingPiece == 'R') {
        // 先手の場合
        if (toRank <= 2 || fromRank <= 2) {
            canPromote = true;
        }
    } else if (move.movingPiece == 'p' || move.movingPiece == 'l' || move.movingPiece == 'n' || move.movingPiece == 's' ||
               move.movingPiece == 'b' || move.movingPiece == 'r') {
        // 後手の場合
        if (toRank >= 6 || fromRank >= 6) {
            canPromote = true;
        }
    }

    // 成れる場合、成る手を生成
    if (canPromote) {
        move.isPromotion = true;
        promotions.append(move);
        move.isPromotion = false;
    }
}

// 1手指した後の局面データを作成する。
void MoveValidator::applyMovesToBoard(const ShogiMove& move, const QVector<QChar>& boardData, QVector<QChar>& boardDataAfterMove) const
{
    int fromIndex = move.fromSquare.y() * MoveValidator::BOARD_SIZE + move.fromSquare.x();
    int toIndex = move.toSquare.y() * MoveValidator::BOARD_SIZE + move.toSquare.x();

    // 移動前の盤面情報を1手指した後の盤面情報にコピー
    boardDataAfterMove = boardData;

    // 移動前のマスの駒を空に更新
    boardDataAfterMove[fromIndex] = ' ';

    // 移動後のマスに自分の駒を更新
    if (move.isPromotion) {
        // 駒を成って指した直後の盤面データを作成する。
        applyPromotionMovesToBoard(move, toIndex, boardDataAfterMove);
    } else {
        // 駒を不成で指した直後の盤面データを作成する。
        applyStandardMovesToBoard(move, toIndex, boardDataAfterMove);
    }
}

// 駒を持ち駒から打った後の持ち駒数を更新する。
void MoveValidator::decreasePieceCount(const ShogiMove& move, const QMap<QChar, int>& pieceStand, QMap<QChar, int>& pieceStandAfterMove)
{
    // 移動前の持ち駒情報を1手指した後の持ち駒情報にコピーする。
    pieceStandAfterMove = pieceStand;

    // 打った駒文字
    QChar piece = move.movingPiece;

    // 駒がマップに存在するかチェックする。
    if (pieceStand.contains(piece)) {
        // 駒の数が1以上なら、1つ減らす。
        if (pieceStandAfterMove[piece] > 0) {
            pieceStandAfterMove[piece]--;
        } else {
            // 駒の数が0なら、エラーメッセージを出力する。
            const QString errorMessage = tr("An error occurred in MoveValidator::decreasePieceCount. There is no piece of %1.").arg(piece);

            emit errorOccurred(errorMessage);
            return; // ★ 打ち切り
        }
    } else {
        // 駒がマップに存在しない場合、エラーメッセージを出力する。
        const QString errorMessage = tr("An error occurred in MoveValidator::decreasePieceCount. There is no piece of %1.").arg(piece);

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }
}

// 駒を成って指した直後の盤面データを作成する。
void MoveValidator::applyPromotionMovesToBoard(const ShogiMove& move, int toIndex, QVector<QChar>& boardDataAfterMove) const
{
    // 駒を成駒に変換するマップ
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

// 駒を不成で指した直後の盤面データを作成する。
void MoveValidator::applyStandardMovesToBoard(const ShogiMove& move, int toIndex, QVector<QChar>& boardDataAfterMove) const
{
    boardDataAfterMove[toIndex] = move.movingPiece;
}

// 指し手が合法手かどうか判定する。
LegalMoveStatus MoveValidator::validateMove(const int& numChecks, const QVector<QChar>& boardData, const ShogiMove& currentMove, const Turn& turn, BoardStateArray& piecePlacedBitboards,
                                 const QMap<QChar, int>& pieceStand, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard, QVector<ShogiMove>& legalMovesList,
                                 const Turn& opponentTurn, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard)
{
    if (currentMove.fromSquare.x() < BOARD_SIZE) {
        // 将棋盤内の駒を動かした場合の指し手のマスの位置を示すbitboardとそのマスの駒の移動可能なマスの位置を示すbitboardを作成し、
        // 指し手が合法手かどうか判定する。
        return generateBitboardAndValidateMove(numChecks, boardData, piecePlacedBitboards, currentMove, turn, necessaryMovesBitboard, legalMovesList);
    } else {
        LegalMoveStatus legalMoveStatus;
        legalMoveStatus.promotingMoveExists = false;

        if (numChecks) {
            // 自玉が王手されている場合
            // 駒台の駒を打った場合の合法手かどうかの判定
            legalMoveStatus.nonPromotingMoveExists = validateMoveWithChecks(currentMove, turn, boardData, pieceStand, numChecks, attackWithoutKingBitboard, legalMovesList);
        } else {
            // 自玉が王手されていない場合
            // 駒台の駒を打った場合の合法手かどうかの判定
            legalMoveStatus.nonPromotingMoveExists = validateMoveWithoutChecks(currentMove, turn, boardData, pieceStand, numChecks, legalMovesList, opponentTurn);
        }

        return legalMoveStatus;
    }
}

// 将棋盤内の駒を動かした場合の指し手のマスの位置を示すbitboardとそのマスの駒の移動可能なマスの位置を示すbitboardを作成し、
// 指し手が合法手かどうか判定する。
LegalMoveStatus MoveValidator::generateBitboardAndValidateMove(const int& numChecks, const QVector<QChar>& boardData, BoardStateArray& piecePlacedBitboards, const ShogiMove& currentMove,
                                                    const Turn& turn, std::bitset<NUM_BOARD_SQUARES>& necessaryMovesBitboard,
                                                    QVector<ShogiMove>& legalMovesList)
{
    // 指し手のマスのインデックス
    int fromIndex = currentMove.fromSquare.y() * BOARD_SIZE + currentMove.fromSquare.x();

    // 指し手のマスの駒文字
    QChar piece = boardData[fromIndex];

    // 指し手のマスの位置を示すbitboardの生成
    std::bitset<NUM_BOARD_SQUARES> pieceBitboard;
    pieceBitboard.set(static_cast<size_t>(fromIndex), true);

    // 駒が移動できるマスを表すbitboard
    std::bitset<NUM_BOARD_SQUARES> attackBitboard;

    // 駒が移動できるマスを表すbitboardを作成する。
    generateSinglePieceAttackBitboard(attackBitboard, pieceBitboard, turn, piece, piecePlacedBitboards);

    // 将棋盤内の駒を動かした場合の指し手が合法手かどうか判定する。
    return isBoardMoveValid(turn, boardData, currentMove, numChecks, piece, attackBitboard, pieceBitboard, necessaryMovesBitboard, legalMovesList);
}

// 自玉が王手されている場合、駒台の駒を打った場合の合法手かどうかを判定する。
bool MoveValidator::validateMoveWithChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                                      const int& numChecks, const std::bitset<NUM_BOARD_SQUARES>& attackWithoutKingBitboard, QVector<ShogiMove>& legalMovesList)
{
    return isHandPieceMoveValid(turn, boardData, pieceStand, currentMove, numChecks,  attackWithoutKingBitboard, legalMovesList);
}

// 自玉が王手されていない場合、駒台の駒を打った場合の合法手かどうかの判定
bool MoveValidator::validateMoveWithoutChecks(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData, const QMap<QChar, int>& pieceStand,
                                              const int& numChecks, QVector<ShogiMove>& legalMovesList, const Turn& opponentTurn)
{
    // 自玉が王手されていない場合
    // 駒が置かれていないマスを表すbitboard
    std::bitset<MoveValidator::NUM_BOARD_SQUARES> emptySquareBitboard;

    // 駒が置かれていないマスを表すbitboardを作成する。
    generateEmptySquareBitboard(boardData, emptySquareBitboard);

    // 駒台の駒を打った場合の合法手かどうかの判定
    bool isLegal = isHandPieceMoveValid(turn, boardData, pieceStand, currentMove, numChecks,  emptySquareBitboard, legalMovesList);

    // 打った手が合法手でかつ打った駒が歩の場合、打ち歩詰めになっているかを以降の処理で判定する。
    // 合法手があり、かつ打った歩が玉の前のマスの場合
    //begin
    // qDebug() << "isLegal: " << isLegal;
    // qDebug() << "isPawnInFrontOfKing: " << isPawnInFrontOfKing(currentMove, turn, boardData);
    //end
    if ((isLegal) && (isPawnInFrontOfKing(currentMove, turn, boardData))) {
        // 指した直後の盤面データ
        QVector<QChar> boardDataAfterMove;

        // 持ち駒
        QMap<QChar, int> pieceStandAfterMove;

        // 打った局面に進める。
        // 1手指した後の局面データを用意する。
        applyMovesToBoard(currentMove, boardData, boardDataAfterMove);

        // 駒を持ち駒から打った後の持ち駒数を更新する。
        decreasePieceCount(currentMove, pieceStand, pieceStandAfterMove);

        //begin
        // printBoardAndPieces(boardDataAfterMove, pieceStandAfterMove);
        //end

        // 手番を相手に設定し、こちらが歩を相手の合法手数を計算する。
        if (generateLegalMoves(opponentTurn, boardDataAfterMove, pieceStandAfterMove)) {
            // 合法手があれば打ち歩詰めではないので指し手はOK
            return true;
        } else {
            // 合法手が無ければ打ち歩詰めなので指し手はNG
            qDebug() << tr("An error occurred in MoveValidator::validateMoveWithoutChecks. Dropping a pawn to give checkmate is not allowed.");

            return false;
        }
    }

    return isLegal;
}

// 玉の前のマスに駒台の歩を打ったかどうかを判定する。
bool MoveValidator::isPawnInFrontOfKing(const ShogiMove& currentMove, const Turn& turn, const QVector<QChar>& boardData) const
{
    // 手番に応じた玉の文字と歩の進行方向を設定
    QChar opponentKingPiece = (turn == BLACK) ? 'k' : 'K';
    int direction = (turn == BLACK) ? +1 : -1;

    // 段と筋をスキャン
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            int index = rank * BOARD_SIZE + file;

            // 現在のマスに相手玉がいるかチェック
            if (boardData[index] == opponentKingPiece) {
                // 歩のマスが相手玉の1つ手前（後手の場合は1つ後ろ）のマスに一致しているか、
                // および、移動駒が歩であることを確認
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

    // どの条件も満たさない場合は、falseを返す。
    return false;
}
