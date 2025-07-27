#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

#include <QStringList>
#include <QPoint>
#include "shogimove.h"

// KIF形式の棋譜をSFEN形式に変換するクラス
class KifToSfenConverter
{
public:
    // コンストラクタ
    KifToSfenConverter();

    // 棋譜データのリストからSFEN形式の棋譜データを生成する。
    void convertKifToSfen(const QStringList& originalKifuLines, QStringList& extractedMoves, QStringList& moveDurations,
                          QStringList* sfenMovesList, QVector<ShogiMove>& gameMoves, QString& sentePlayerName, QString& gotePlayerName,
                          QString& uwatePlayerName, QString& shitatePlayerName, QString& handicapType);

private:
    // 指し手の行を表す正規表現
    static const QRegularExpression rKifMove;

    // 「同」が含まれる指し手の行を表す正規表現
    static const QRegularExpression rSameSpotMove;

    // 消費時間に関する行を表す正規表現
    static const QRegularExpression rMoveTime;

    // 指し手の終了を表す行を表す正規表現
    static const QRegularExpression rGameEnd;

    // KIF形式の棋譜データから指し手部分を抽出し、指し手のリストに格納する。
    QStringList extractMovesFromKifu(const QStringList& originalKifuLines) const;

    // 棋譜データのリストから手合割の文字列を検出して返す。
    QString findHandicapInKifu(const QStringList &originalKifuLines) const;

    // 指定された列の文字列からSFEN形式の文字列を生成する。連続する0の数をその数値で置き換える。
    QString convertToSfenString(const QString& fileString) const;

    // 指し手の文字列を全角から半角に変換し、漢数字も半角数字に変換する。
    QStringList convertMovesToHalfWidth(const QStringList& originalKifuLines) const;

    // 指し手全てSFEN形式に変換し、sfenMovesListに格納する。
    // kifuLinesWithHalfWidthChars 棋譜ファイル全体。ただし、指し手部分の漢字全角を半角、かつ漢数字を半角数字に変換。
    // kifuMovesAsNumbers 指し手部分。ただし、指し手部分の漢字全角を半角、かつ漢数字を半角数字に変換したもので同を数字に置き換えたもの。
    // moveDurations 指し手部分の消費時間
    void moveList(const QStringList& kifuLinesWithHalfWidthChars, QStringList& kifuMovesAsNumbers, QStringList& moveDurations);

    // 指定された行から消費時間を抽出して返す。
    QString extractTime(const QString& line) const;

    // 指し手に"同"の文字が含まれている場合、前の指し手と同じマスの位置を返す。
    QString handleSamePosition(QString& currentMove, QString& beforeStr) const;

    // 指し手の行が終了を表す行か判定して、指し手をリストに追加する。
    void appendMatchToEndList(const QString& line, QStringList& kifuMovesAsNumbers, const QString& currentMove) const;

    // 先手駒台の駒文字に対応する段番号を返す。
    int getRankForBlackPieceStand(const QString& pieceName) const;

    // 後手駒台の駒文字に対応する段番号を返す。
    int getRankForWhitePieceStand(const QString& pieceName) const;

    // SFEN形式の文字列内の数字を連続した'0'の列に置き換える。
    QString replaceNumbersWithZeros(const QString& sfen) const;

    // SFEN形式の文字列内の成駒を1文字のアルファベットに置換する。
    QString replacePromotedPieces(const QString& sfen) const;

    // 駒台から持ち駒を打った場合の移動先と移動元の筋と段を取得し、持ち駒の枚数を減らす処理を行う。
    // 駒台から持ち駒を打った場合の移動先と移動元の筋と段を取得し、持ち駒の枚数を減らす処理を行う。
    void processMoveFromStand(int& standX, int& standY, int& destinationX, int& destinationY, const QList<QString> &kifuMovesAsNumbers, int i, QChar board[9][9], int piecesCountByPlayerAndType[2][7], const QString& handicapType);

    // 盤内の駒を指した場合の移動先と移動元の筋と段を取得する。
    void processMoveFromBoard(int& sourceX, int& sourceY, int& destinationX, int& destinationY, const QList<QString> &kifuMovesAsNumbers, int i, QChar board[9][9], int piecesCountByPlayerAndType[2][7], const QString& handicapType);

    // 成・不成どちらで指したのかを返し、リストに追加する。
    bool appendPromotionInfo(const QStringList& kifu, const int moveIndex) const;

    // 指し手全てをSFEN形式に変換し、指定された引数に格納する。
    void makeSfenStr(const QStringList& kifuMovesAsNumbers, QStringList* sfenMovesList, QString handicapSFEN, QVector<ShogiMove>& gameMoves, const QString& handicapType);

    // 盤面の状態、手番、持ち駒をSFEN形式の文字列に変換する。
    QString convertBoardToSfenString(const QChar board[9][9], const int turnCount, const QString& inHandPieces, const QString& handicapType) const;

    // 先手の指し手の漢字をアルファベットの駒文字に変換する。
    QChar translateKanjiToBlackCharacter(const QString& kanji) const;

    // 後手の指し手の漢字をアルファベットの駒文字に変換する。
    QChar translateKanjiToWhiteCharacter(const QString& kanji) const;

    // 駒文字に対応する持ち駒の枚数を1増やす。
    void incrementPieceCount(const QChar piece, int piecesCountByPlayerAndType[2][7]) const;

    // 駒文字に対応する持ち駒の枚数を1減らす。
    void decrementPieceCount(const QChar piece, int piecesCountByPlayerAndType[2][7]) const;

    // 持ち駒の数をSFEN形式の文字列で表現する。
    QString createSfenForPiecesInHand(const int piecesCountByPlayerAndType[2][7]) const;

    // 手合割のSFEN文字列を2次元配列に変換する。
    void convertSfenToMatrix(const QString& handicapSFEN, QChar board[9][9]) const;

    // 指定された駒落ちの種類に基づいて対応するSFEN文字列を返す。
    QString getHandicapSFENString(const QString handicapType) const;

    // 棋譜データのリストから先手の対局者の文字列を検出して返す。
    QString findSentePlayerInKifu(const QStringList &originalKifuLines) const;

    // 棋譜データのリストから後手の対局者の文字列を検出して返す。
    QString findGotePlayerInKifu(const QStringList &originalKifuLines) const;

    // 棋譜データのリストから上手の対局者の文字列を検出して返す。
    QString findUwatePlayerInKifu(const QStringList &originalKifuLines) const;

    // 棋譜データのリストから下手の対局者の文字列を検出して返す。
    QString findShitatePlayerInKifu(const QStringList &originalKifuLines) const;

    // 棋譜データのリストから指定された正規表現にマッチする文字列を検出して返す。
    QString findPatternInKifu(const QStringList& originalKifuLines, const QRegularExpression& pattern) const;

    // 移動元と移動先のマスの筋と段を取得し、移動先のマスに変換された駒文字をセットする。
    void setMoveSourceAndDestination(int& standX, int& standY, int& destinationX, int& destinationY, const QString& move, QChar board[9][9],
                                     bool isPlayingAsBlack, const QString& handicapType) const;

    // 移動先と移動元のマスの筋と段の番号を抽出する。
    void extractMoveCoordinates(const QRegularExpressionMatch& match, int& sourceX, int& sourceY, int& destinationX, int& destinationY) const;

    // 駒を移動させ、対応する文字に変換する。
    void moveAndTranslatePiece(const QString& pieceKanji, int destX, int destY, QChar board[9][9], int moveIndex, const QString& handicapType) const;

    // 先手あるいは下手で移動元の駒台のマスの筋、段番号を取得し、移動先のマスに変換された駒文字をセットする。
    void setBlackPlayerMoveFromStand(int& standX, int& standY, int& destinationX, int& destinationY, const QString& pieceKanji, QChar board[9][9]) const;

    // 後手あるいは上手で移動元の駒台のマスの筋、段番号を取得し、移動先のマスに変換された駒文字をセットする。
    void setWhitePlayerMoveFromStand(int& standX, int& standY, int& destinationX, int& destinationY, const QString& pieceKanji, QChar board[9][9]) const;
};

#endif // KIFTOSFENCONVERTER_H
