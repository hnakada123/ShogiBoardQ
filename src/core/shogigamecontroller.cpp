#include "shogigamecontroller.h"

#include <QPoint>
#include <QDebug>
#include "shogiboard.h"
#include "shogimove.h"
#include "movevalidator.h"
#include "playmode.h"
#include "shogiutils.h"
#include "legalmovestatus.h"

// 将棋の対局全体を管理し、盤面の初期化、指し手の処理、合法手の検証、対局状態の管理を行うクラス
// コンストラクタ
ShogiGameController::ShogiGameController(QObject* parent)
    : QObject(parent), m_board(nullptr), m_result(NoResult), m_currentPlayer(NoPlayer), m_promote(false)
{
}

// 将棋盤のポインタを取得する。
ShogiBoard* ShogiGameController::board() const
{
    return m_board;
}

// 新規対局の準備
// 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
// 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
void ShogiGameController::newGame(QString& initialSfenString)
{
    // 将棋盤を示すポインタm_boardの作成
    // 将棋盤の81マスに空白を代入し、駒台の駒を0にする。
    setupBoard();

    // 将棋盤と駒台を含むSFEN文字列で将棋盤全体を更新する場合、この関数を使う。
    // 将棋盤に入力で渡されるsfen形式の文字列に文法的に誤りが無いかチェックする。
    // 将棋盤と駒台のSFEN文字列を指定して将棋盤と駒台の駒の更新を行い、再描画する。
    // 入力は、将棋盤と駒台を含むSFEN文字列
    // "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    board()->setSfen(initialSfenString);

    // 対局結果をNoResultに設定する。
    setResult(NoResult);

    // 現在の手番をNoPlayerに設定する。
    setCurrentPlayer(NoPlayer);
}

// 将棋盤を示すポインタm_boardを作成する。
// 将棋盤の81マスに空白を代入し、駒台の駒を0にする。
void ShogiGameController::setupBoard()
{
    // 将棋盤の81マスに空白を代入し、駒台の駒を0にする。
    // 将棋盤を示すポインタm_boardを作成する。
    setBoard(new ShogiBoard(9, 9, this));
}

// 将棋盤のポインタを設定する。
void ShogiGameController::setBoard(ShogiBoard* board)
{
    // すでに設定されている場合は何もしない。
    if (board == m_board) return;

    // すでに設定されている場合は削除する。
    if (m_board) delete m_board;

    // 新しい将棋盤のポインタを設定する。
    m_board = board;

    // 将棋盤のポインタが設定されたことを通知する。
    emit boardChanged(m_board);
}

// 対局結果を設定する。
void ShogiGameController::setResult(Result value)
{
    //begin
    qDebug() << "in ShogiGameController::setResult";
    //end

    // すでに設定されている場合は何もしない。
    if (result() == value) return;

    // 対局結果を設定する。
    if (result() == NoResult) {
        m_result = value;

        emit gameOver(m_result);
    } else {
        m_result = value;
    }
}

// 成り・不成のフラグを返す。
bool ShogiGameController::promote() const
{
    return m_promote;
}

// 成り・不成のフラグを設定する。
void ShogiGameController::setPromote(bool newPromote)
{
    m_promote = newPromote;
}

// 現在の手番を設定する。
void ShogiGameController::setCurrentPlayer(const Player player)
{
    // 現在の手番と同じ手番の場合、何もしない。
    if (currentPlayer() == player) return;

    // 現在の手番を設定する。
    m_currentPlayer = player;

    // 現在の手番が変更されたことを通知する。
    emit currentPlayerChanged(m_currentPlayer);
}

// 指し手を漢字の文字列に変換する。
QString ShogiGameController::convertMoveToKanjiStr(const QString piece, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo)
{
    // 駒の漢字表記
    QString pieceKanji;

    // 「全、圭、杏」の場合はそれぞれ「成銀、成桂、成香」とする。
    if (piece == "全") {
        pieceKanji = "成銀";
    } else if (piece == "圭") {
        pieceKanji = "成桂";
    } else if (piece == "杏") {
        pieceKanji = "成香";
    } else {
        pieceKanji = piece;
    }

    // 指し手の漢字文字列
    QString moveStr;

    // 現在の手番に応じて指し手の漢字文字列を設定する。
    if (currentPlayer() == Player1) {
        moveStr = "▲";
    } else if (currentPlayer() == Player2) {
        moveStr = "△";
    }

    // 同じマスに駒を移動する場合
    if ((fileTo == previousFileTo) && (rankTo == previousRankTo)) {
        // 漢字文字列に「同」を追加する。
        moveStr += "同　" + pieceKanji;
    }
    // 同じマスに駒を移動しない場合
    else {
        // 漢字文字列に移動先のマスを追加する。
        moveStr += ShogiUtils::transFileTo(fileTo) + ShogiUtils::transRankTo(rankTo) + pieceKanji;
    }

    // 成る場合は「成」を追加する。
    if (m_promote) moveStr += "成";

    // 駒台から打つ場合（10は先手の駒台番号、11は後手の駒台番号）
    if ((fileFrom >= 10) && (fileFrom <= 11)) {
        // 漢字文字列に「打」を追加する。
        moveStr += "打";
    }
    // 駒台から打たない場合
    else {
        // 漢字文字列に移動元のマスを追加する。
        moveStr += "(" + QString::number(fileFrom) + QString::number(rankFrom) + ")";
    }

    // 指した先のマスを１手前の指し手として記憶しておく。
    previousFileTo = fileTo;
    previousRankTo = rankTo;

    // 漢字文字列を返す。
    return moveStr;
}

// 人間が指した場合に指し手で成る手と不成の手が合法手であるかを判定し、GUIのダイアログで対局者に
// 成るか成らないかを選択させて、その結果をcurrentMove.isPromotionに保存する。
// 指し手が合法であればtrue、不合法であればfalseを返す。
bool ShogiGameController::decidePromotion(PlayMode& playMode, MoveValidator& validator, const MoveValidator::Turn& turnMove,
                                     int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, QChar& piece,  ShogiMove& currentMove)
{
    // 駒台には指せない。
    if (fileTo >= 10) return false;

    if (isCurrentPlayerHumanControlled(playMode)) {
        // 一時的に成らないと設定
        currentMove.isPromotion = false;

        LegalMoveStatus legalMoveStatus = validator.isLegalMove(turnMove, board()->boardData(), board()->getPieceStand(), currentMove);

        // 成らない状態での合法手が存在すればtrueを返す。存在しなければfalseを返す。
        bool canMoveWithoutPromotion = legalMoveStatus.nonPromotingMoveExists;

        // 成る状態での合法手が存在すればtrueを返す。存在しなければfalseを返す。
        bool canMoveWithPromotion = legalMoveStatus.promotingMoveExists;

        // 成る指し手が可能な場合
        if (canMoveWithPromotion) {
            // 成らない指し手も可能な場合
            if (canMoveWithoutPromotion) {
                // 駒台上ではなく盤上の指し手である場合、対局者に成るかどうかを選択させる。
                if (fileFrom <= 9) {
                    // ダイアログを表示して対局者に成るかどうかを選択させる。
                    decidePromotionByDialog(piece, rankFrom, rankTo);

                    // ダイアログでの選択結果をcurrentMove.isPromotionに設定する。
                    currentMove.isPromotion = m_promote;

                    // 指し手が合法であればtrueを返す。
                    return true;
                }
            }
            // 必ず成る必要がある場合
            else {
                // 成らない指し手は不可能なので、trueを返す。
                currentMove.isPromotion = true;
            }
        } else {
            if (canMoveWithoutPromotion) {
                // 不成で指さなければならない。
                currentMove.isPromotion = false;
            } else {
                // どちらの指し手も不可能な場合、falseを返す。
                return false;
            }
        }
    }

    // 成る・不成のフラグを設定する。
    m_promote = currentMove.isPromotion;

    // 指し手が合法であればtrueを返す。
    return true;
}

// 人間が対局者に含まれているかどうかを判定する。
bool ShogiGameController::isCurrentPlayerHumanControlled(PlayMode& playMode)
{
    // 現在の手番を取得する。
    auto player = currentPlayer();

    // 対局者が人間同士であるか
    return (playMode == HumanVsHuman)
           // あるいは、手番が先手あるいは下手であり、かつ平手で人間対エンジンの場合
           || (player == Player1 && playMode == EvenHumanVsEngine)

           // あるいは、手番が後手あるいは上手であり、かつ平手でエンジン対人間の場合
           || (player == Player2 && playMode == EvenEngineVsHuman)

           // あるいは、手番が先手あるいは下手であり、かつ駒落ちで人間対エンジンの場合
           || (player == Player1 && playMode == HandicapHumanVsEngine)

           // あるいは、手番が後手あるいは上手であり、かつ駒落ちでエンジン対人間の場合
           || (player == Player2 && playMode == HandicapEngineVsHuman);
}

// ダイアログを表示して対局者に成るかどうかを選択させる。
void ShogiGameController::decidePromotionByDialog(QChar& piece, int& rankFrom, int& rankTo)
{
    auto player = currentPlayer();

    // 駒が成ることが可能な場合のみ処理を行う。
    if (isPromotablePiece(piece)) {
        // 先手あるいは下手の場合
        if (player == Player1) {
            // 駒が4段目から3段目に到達した場合、またはそれ以外の場合に成るかどうかを選択させる。
            // ただし、駒が4段目以上でかつ3段目を超える場合には選択させない。
            if (rankFrom < 4 || (rankFrom >= 4 && rankTo <= 3)) {
                emit showPromotionDialog();
            }
        }
        // 後手あるいは上手の場合
        else if (player == Player2) {
            // 駒が6段目から7段目に到達した場合、またはそれ以外の場合に成るかどうかを選択させる。
            // ただし、駒が6段目以下でかつ7段目を超える場合には選択させない。
            if (rankFrom > 6 || (rankFrom <= 6 && rankTo >= 7)) {
                emit showPromotionDialog();
            }
        }
    }
}

// 指定された駒が成ることが可能な駒であるかを判定する。
bool ShogiGameController::isPromotablePiece(QChar& piece)
{
    // 成ることが可能な駒のリスト
    static const QString promotablePieces = "PLNSBRplnsbr";

    // 成ることが可能な駒であるかを判定する。
    return promotablePieces.contains(piece);
}

// アルファベットの駒文字を漢字表記にする。
QString ShogiGameController::getPieceKanji(const QChar& piece)
{
    static const QMap<QChar, QString> pieceKanjiNames = {
        {' ', "　"},
        {'P', "歩"}, {'L', "香"}, {'N', "桂"}, {'S', "銀"}, {'G', "金"}, {'B', "角"}, {'R', "飛"}, {'K', "玉"},
        {'Q', "と"}, {'M', "杏"}, {'O', "圭"}, {'T', "全"}, {'C', "馬"}, {'U', "龍"},
        {'p', "歩"}, {'l', "香"}, {'n', "桂"}, {'s', "銀"}, {'g', "金"}, {'b', "角"}, {'r', "飛"}, {'k', "玉"},
        {'q', "と"}, {'m', "杏"}, {'o', "圭"}, {'t', "全"}, {'c', "馬"}, {'u', "龍"}
    };

    // 入力された駒をマップで検索する。
    auto it = pieceKanjiNames.find(piece);

    // 対応する漢字が見つかった場合
    if (it != pieceKanjiNames.end()) {
        // 対応する漢字を返す。
        return it.value();
    }
    // 対応する漢字が見つからなかった場合
    else {
        // エラーメッセージを表示する。
        const QString errorMessage = tr("An error occurred in ShogiGameController::getPieceKanji: The piece %1 is not found.").arg(piece);

        ShogiUtils::logAndThrowError(errorMessage);
    }

    return QString();
}

// 相手の手番をSFEN形式の手番bまたはwで取得する。
QString ShogiGameController::getNextPlayerSfen()
{
    // 相手のSFEN形式の手番
    QString nextPlayerColorSfen;

    // 現在の手番に応じて相手の手番を設定する。
    switch (currentPlayer()) {
    // 先手、下手の場合
    case Player1:
        nextPlayerColorSfen = "w";

        break;

    // 後手、上手の場合
    case Player2:
        nextPlayerColorSfen = "b";

        break;

    // それ以外の場合は、エラーにする。
    default:
        // エラーメッセージを表示する。
        const QString errorMessage = tr("An error occurred in ShogiGameController::getNextPlayerSfen: Invalid player state.");

        qDebug() << "currentPlayer() = " << currentPlayer();

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // 相手のSFEN形式の手番を返す。
    return nextPlayerColorSfen;
}

// 合法手判定に関するクラスで利用するための手番を設定する。
MoveValidator::Turn ShogiGameController::getCurrentTurnForValidator(MoveValidator& validator)
{
    // 現在の手番が先手あるいは下手の場合
    if (currentPlayer() == Player1) {
        return validator.BLACK;
    }
    // 現在の手番が後手あるいは上手の場合
    else {
        return validator.WHITE;
    }
}

bool ShogiGameController::validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, PlayMode& playMode, int& moveNumber,
                                     QStringList* m_sfenRecord, QVector<ShogiMove>& gameMoves)
{
    // 指し手の移動元と移動先のマスの筋と段を取得する。
    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo = outTo.x();
    int rankTo = outTo.y();

    //begin
    qDebug() << "in ShogiGameController::validateAndMove";
    qDebug() << "playMode = " << playMode;
    qDebug() << "promote = " << m_promote;
    qDebug() << "fileFrom = " << fileFrom;
    qDebug() << "rankFrom = " << rankFrom;
    qDebug() << "fileTo = " << fileTo;
    qDebug() << "rankTo = " << rankTo;
    qDebug() << "currentPlayer() = " << currentPlayer();
    //end

    // 合法手判定に関するクラス
    MoveValidator validator;

    // 合法手判定に関するクラスで利用するための手番を設定する。
    MoveValidator::Turn turn = getCurrentTurnForValidator(validator);

    // 合成手判定で使用する指し手データを生成する。
    // 移動元と移動先のマスの筋と段を指定してマスの座標を生成する。
    QPoint fromPoint(fileFrom - 1, rankFrom - 1);
    QPoint toPoint(fileTo - 1, rankTo - 1);

    // 指した駒文字を取得する。
    QChar movingPiece = board()->getPieceCharacter(fileFrom, rankFrom);

    // 指した先の取られた駒文字を取得する。
    QChar capturedPiece = board()->getPieceCharacter(fileTo, rankTo);

    // 合法手判定に関するクラスで利用するための指し手データを生成する。
    ShogiMove currentMove(fromPoint, toPoint, movingPiece, capturedPiece, m_promote);

    // 人間が指した場合に指し手で成る手と不成の手が合法手であるかを判定し、GUIのダイアログで対局者に
    // 成るか成らないかを選択させて、その結果をcurrentMove.isPromotionに保存する。
    // 指し手が合法であればtrue、不合法であればfalseを返す。
    if (!decidePromotion(playMode, validator, turn, fileFrom, rankFrom, fileTo, rankTo, movingPiece, currentMove)) {
        // decidePromotion関数実施後に必要な処理
        // 駒のドラッグを終了する。
        emit endDragSignal();

        // 指そうとした手が合法手でない場合は、指せないことを示すfalseを返す。
        return false;
    } else {
        // 駒のドラッグを終了する。
        emit endDragSignal();
    }

    // 現在の指し手を追加保存する。
    gameMoves.append(currentMove);

    // 将棋盤と駒台の駒数を更新する。
    board()->updateBoardAndPieceStand(movingPiece, capturedPiece, fileFrom, rankFrom, fileTo, rankTo, m_promote);

    // 相手の手番をSFEN形式の手番bまたはwで取得する。
    QString nextPlayerColorSfen = getNextPlayerSfen();

    // SFEN形式の文字列に変換し、その文字列を追加保存する。
    board()->addSfenRecord(nextPlayerColorSfen, moveNumber, m_sfenRecord);

    // 指し手の駒を漢字で取得する。
    QString kanjiPiece = getPieceKanji(movingPiece);

    // 指し手の文字列を漢字で取得する。
    record = convertMoveToKanjiStr(kanjiPiece, fileFrom, rankFrom, fileTo, rankTo);

    // 手番を変える。
    setCurrentPlayer(currentPlayer() == Player1 ? Player2 : Player1);

    // 指そうとした手が合法手である場合は、指せることを示すtrueを返す。
    return true;
}

// 手番を変える。
void ShogiGameController::changeCurrentPlayer()
{
    setCurrentPlayer(currentPlayer() == Player1 ? Player2 : Player1);
}

// 局面編集で駒移動を行い、局面を更新する。
bool ShogiGameController::editPosition(const QPoint& outFrom, const QPoint& outTo)
{
    // 指し手の移動元と移動先のマスの筋と段を取得する。
    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo = outTo.x();
    int rankTo = outTo.y();

    //begin
    // 持ち駒を出力する。
    // board()->printPieceStand();
    //end

    // 移動元のマスの駒文字を取得する。
    QChar source = board()->getPieceCharacter(fileFrom, rankFrom);

    // 移動先のマスの駒文字を取得する。
    QChar dest = board()->getPieceCharacter(fileTo, rankTo);

    // 移動先の駒文字が大文字の場合、先手あるいは下手の駒である。
    if (source.isUpper()) {
        // 手番を先手あるいは下手に設定する。
        setCurrentPlayer(Player1);
    }
    // 移動先の駒文字が小文字の場合、後手の駒である。
    else {
        // 手番を後手あるいは上手に設定する。
        setCurrentPlayer(Player2);
    }

    // 駒台に載せる場合、移動元と移動先の駒文字が一致しない場合は指せない。
    if ((fileTo >= 10) && (source.toUpper() != dest.toUpper())) return false;

    // 禁じ手をチェックする。
    if (!checkMovePiece(source, dest, fileFrom, fileTo)) return false;

    // 歩、桂馬、香車を指した場合に成が必須になる時のフラグの設定
    setMandatoryPromotionFlag(fileTo, rankTo, source);

    // 将棋盤と駒台の駒数を更新する。
    board()->updateBoardAndPieceStand(source, dest, fileFrom, rankFrom, fileTo, rankTo, m_promote);

    // 駒の移動が可能な場合、trueを返す。
    return true;
}

// 禁じ手をチェックする。
bool ShogiGameController::checkMovePiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const
{
    // 二歩のところには歩は指せない。
    if (!checkTwoPawn(source, fileFrom, fileTo)) return false;

    // 味方の駒があるところには指せない。
    if (!checkWhetherAllyPiece(source, dest, fileFrom, fileTo)) return false;

    // 駒台から指そうとした場合、駒台の駒数が0以下の時は、駒台の駒は無いので指せない。
    if (!checkNumberStandPiece(source, fileFrom)) return false;

    // 先手の駒台の駒から後手の駒台の異種駒には、駒は移せない。
    // また、後手の駒台の駒から先手の駒台の異種駒には、駒は移せない。
    if (!checkFromPieceStandToPieceStand(source, dest, fileFrom, fileTo)) return false;

    // 王、玉は、相手の駒で取ることはできない。
    if (!checkGetKingOpponentPiece(source, dest)) return false;

    // 禁じ手のチェックはOKで指せる。
    return true;
}

// 駒台から指そうとした場合、駒台の駒数が0以下の時は、駒台の駒は無いので指せない。
bool ShogiGameController::checkNumberStandPiece(const QChar source, const int fileFrom) const
{
    return board()->isPieceAvailableOnStand(source, fileFrom);
}

// 指そうとした場所に味方の駒があるかどうかチェックする。
bool ShogiGameController::checkWhetherAllyPiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const
{
    if (fileTo < 10) {
        // 選択した駒と指そうとした先のマスの駒が大文字の場合、味方の駒になる。
        if (source.isUpper() && dest.isUpper()) {
            if ((fileFrom < 10) && (fileTo > 9)) {
                return true;
            } else {
                return false;
            }
        }
        // 選択した駒と指そうとした先のマスの駒が小文字の場合、相手にとっては味方の駒になる。
        if (source.isLower() && dest.isLower()) {
            if ((fileFrom < 10) && (fileTo > 9)) {
                return true;
            } else {
                return false;
            }
        }
    }

    // それ以外は指せる。
    return true;
}

// 二歩になるかどうかをチェックする。
bool ShogiGameController::checkTwoPawn(const QChar source, const int fileFrom, const int fileTo) const
{
    // 筋が同じマスに移動させる場合は、二歩にならないので指せる。
    if (fileFrom == fileTo) return true;

    // 移動元の駒が先手あるいは下手の歩である場合
    if (source == 'P') {
        // 移動先が将棋盤内の場合
        if (fileTo < 10) {
            // 段
            for (int rank = 1; rank <= 9; rank++) {
                // 移動先の段に先手あるいは下手の歩が存在している場合は、二歩になるので指せない。
                if (board()->getPieceCharacter(fileTo, rank) == 'P') return false;
            }
        }
    }

    // 移動元の駒が後手あるいは上手の歩である場合
    if (source == 'p') {
        // 移動先が将棋盤内の場合
        if (fileTo < 10) {
            // 段
            for (int rank = 1; rank <= 9; rank++) {
                // 移動先の段に後手あるいは上手の歩が存在している場合は、二歩になるので指せない。
                if (board()->getPieceCharacter(fileTo, rank) == 'p') return false;
            }
        }
    }

    // 二歩にならないので指せる。
    return true;
}

// 歩、桂馬、香車を指した場合に成りが必須になる時のフラグを設定する。
void ShogiGameController::setMandatoryPromotionFlag(const int fileTo, const int rankTo, const QChar source)
{
    // 成る・不成のフラグを初期値として不成に設定する。
    m_promote = false;

    // 指そうとしたところが駒台の場合は不成。
    if (fileTo >= 10) {
        return;
    }

    // 先手あるいは下手の歩を一段目に指す場合は、必ず成る。
    if ((source == 'P' && rankTo == 1) ||
        // 先手あるいは下手の香車を一段目に指す場合は、必ず成る。
        (source == 'L' && rankTo == 1) ||
        // 先手あるいは下手の桂馬を一、二段目に指す場合は、必ず成る。
        (source == 'N' && rankTo <= 2)) {
        m_promote = true;

        return;
    }

    // 後手あるいは上手の歩を九段目に指す場合は、必ず成る。
    if ((source == 'p' && rankTo == 9) ||
        // 後手あるいは上手の香車を九段目に指す場合は、必ず成る。
        (source == 'l' && rankTo == 9) ||
        // 後手あるいは上手の桂馬を八、九段目に指す場合は、必ず成る。
        (source == 'n' && rankTo >= 8)) {
        m_promote = true;

        return;
    }
}

// 対局結果を設定する。
void ShogiGameController::gameResult()
{
    // 後手あるいは上手の場合
    if (currentPlayer() == Player2) {
        // 先手あるいは下手の勝ちに設定する。
        setResult(Player1Wins);
    }
    // 先手あるいは下手の場合
    else if (currentPlayer() == Player1) {
        // 後手あるいは上手の勝ちに設定する。
        setResult(Player2Wins);
    }
}

// 編集局面モードの際、右クリックで駒を成・不成に変換する。
void ShogiGameController::switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const
{
    // 動かす駒
    QChar source = board()->getPieceCharacter(fileFrom, rankFrom);

    // 動かす駒が先手と金であった時
    if (source == 'Q') {
        // 段
        for (int rank = 1; rank <= 9; rank++) {
            // と金のある段に歩が存在していた場合は、と金は歩にできない。（二歩）
            if (board()->getPieceCharacter(fileFrom, rank) == 'P') return;
        }
    }

    // 動かす駒が後手と金であった時
    if (source == 'q') {
        // 段
        for (int rank = 1; rank <= 9; rank++) {
            // と金のある段に歩が存在していた場合は、と金は歩にできない。（二歩）
            if (board()->getPieceCharacter(fileFrom, rank) == 'p') return;
        }
    }

    // 成・不成の駒に変換
    board()->promoteOrDemotePiece(fileFrom, rankFrom);
}

// 駒台から駒台に駒を移動することが可能かどうかをチェックする。
// 先手の駒台の駒から後手の駒台の異種駒には、駒は移せない。
// また、後手の駒台の駒から先手の駒台の異種駒には、駒は移せない。
bool ShogiGameController::checkFromPieceStandToPieceStand(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const
{
    // 先手の駒台から後手の駒台に駒を移す。
    if ((fileFrom == 10) && (fileTo == 11)) {
        // dest を大文字に変換
        QChar c = dest.toUpper();
        // 同じ種類の駒の時
        if (source == c) {
            // 移せる。
            return true;
        // 同じ種類の駒でない時
        } else {
            // 移せない。
            return false;
        }
    }
    // 後手の駒台から先手の駒台に駒を移す。
    if ((fileFrom == 11) && (fileTo == 10)) {
        // dest を小文字に変換
        QChar c = dest.toLower();
        // 同じ種類の駒の時
        if (source == c) {
            // 移せる。
            return true;
        // 同じ種類の駒でない時
        } else {
            // 移せない。
            return false;
        }
    }

    // 移せる。
    return true;
}

// 王、玉は、相手の駒で取ることはできないので、取れない場合はfalseを返す。
bool ShogiGameController::checkGetKingOpponentPiece(const QChar source, const QChar dest) const
{
    // 後手の駒で先手王を取ろうとした場合
    if ((dest == 'K') && source.isLower()) return false;

    // 先手の駒で後手玉を取ろうとした場合
    if ((dest == 'k') && source.isUpper()) return false;

    // 取れる。
    return true;
}

// 局面編集後の局面をSFEN形式に変換し、リストに追加する。
void ShogiGameController::updateSfenRecordAfterEdit(QStringList* m_sfenRecord)
{
    // 仮の手数を設定する。board()->addSfenRecord関数内で2手足して初手になるよう帳尻を合わせている。
    int moveIndex = -1;

    // 相手の手番をSFEN形式の手番bに設定する。
    QString nextTurn = "b";

    // 現在の盤面をSFEN形式に変換し、その文字列をリストに追加する。
    board()->addSfenRecord(nextTurn, moveIndex, m_sfenRecord);
}
