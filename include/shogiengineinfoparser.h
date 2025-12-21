#ifndef SHOGIENGINEINFOPARSER_H
#define SHOGIENGINEINFOPARSER_H

#include <QString>
#include <QObject>
#include "shogigamecontroller.h"

// エンジンは、infoコマンドによって思考中の情報を返す。
// info行を解析するクラス
class ShogiEngineInfoParser : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    ShogiEngineInfoParser();

    // 評価値の境界を表す列挙型
    enum class EvaluationBound {
        None,       // どちらの境界もない
        LowerBound, // 下限値（実際にはその値を上回る可能性がある）
        UpperBound  // 上限値（実際にはその値を下回る可能性がある）
    };

    // 評価値の境界を取得する。
    EvaluationBound evaluationBound() const;

    // 評価値の境界を設定する。
    void setEvaluationBound(EvaluationBound newEvaluationBound);

    // 盤面の1辺のマス数（列数と段数）
    static constexpr int BOARD_SIZE = 9;

    // 将棋盤のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    // 駒台を示す筋番号
    static constexpr int STAND_FILE = 99;

    // "info multipv 1 score cp 0 depth 32 pv 3c3d 6g6f 2b3c 7i6h 8b2b 6h6g 3a4b 8h7g (57.54%)"
    // の"(57.54%)"のような文字列が入力された場合が該当する。
    static constexpr int INFO_STRING_SPECIAL_CASE = -2;

    // 直前の指し手の筋を返す。
    int previousFileTo() const;

    // 直前の指し手の筋を設定する。
    void setPreviousFileTo(int newPreviousFileTo);

    // 直前の指し手の段を返す。
    int previousRankTo() const;

    // 直前の指し手の段を設定する。
    void setPreviousRankTo(int newPreviousRankTo);

    // 将棋エンジンから受信したinfo行の読み筋を日本語に変換する。
    // 例．
    // 「7g7h 2f2e 8e8f 2e2d 2c2d 8i7g 8f8g+」
    // 「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
    void parseEngineOutputAndUpdateState(QString& line, const ShogiGameController* algorithm,  QVector<QChar>& clonedBoardData,
                                         const bool isPondering);

    // 思考開始時の手番を設定する
    void setThinkingStartPlayer(ShogiGameController::Player player);

    // 思考開始時の手番を取得する
    ShogiGameController::Player thinkingStartPlayer() const;

    // 現在思考中の手の探索深さを取得する。
    QString depth() const;

    // 現在選択的に読んでいる手の探索深さを取得する。
    QString seldepth() const;

    // pvで初手の異なる複数の読み筋を返す時、それがn通りあれば、最も良い（評価値の高い）ものを返す。
    QString multipv() const;

    // 思考開始から探索したノード数を取得する。
    QString nodes() const;

    // 1秒あたりの探索局面数を取得する。
    QString nps() const;

    // 思考を開始してから経過した時間（単位はミリ秒）を取得する。
    QString time() const;

    // エンジンによる現在の評価値を取得する。
    QString scoreCp() const;

    // エンジンが詰みを発見した場合の詰み手数を取得する。
    QString scoreMate() const;

    // 現在の読み筋を漢字表示で表した文字列を取得する。
    QString pvKanjiStr() const;

    // USI形式の読み筋を取得する。
    QString pvUsiStr() const;

    // 現在思考中の手（思考開始局面から最初に指す手）を取得する。
    QString currmove() const;

    // エンジンが現在使用しているハッシュの使用率を取得する。
    QString hashfull() const;

    // GUIの「探索手」の欄に表示する読み筋の最初の文字列を取得する。
    QString searchedHand() const;

    // エンジンが詰みを発見した場合の詰み手数を設定する。
    void setScoreMate(const QString& newScoremate);

    QString convertPredictedMoveToKanjiString(const ShogiGameController* algorithm, QString& predictedOpponentMove, QVector<QChar>& clonedBoardData);

    // 指し手を解析し、その指し手に基づいてコピーした盤面データを更新する。
    void parseAndApplyMoveToClonedBoard(const QString& str, QVector<QChar>& clonedBoardData);

    // 盤面データを9x9のマスに表示する。
    void printShogiBoard(const QVector<QChar>& boardData) const;

    // 評価値の文字列を設定する。
    void setScore(const QString& newScore);

    // 評価値の文字列を取得する。
    QString score() const;

signals:
    // エラーを報告するためのシグナル
    void errorOccurred(const QString& errorMessage);

private:
    // 現在思考中の手の探索深さ
    QString m_depth;

    // 現在選択的に読んでいる手の探索深さ
    QString m_seldepth;

    // pv: 現在の読み筋を返す。（pvというのは、principal variationの略）
    // pvで初手の異なる複数の読み筋を返す時、それがn通りあれば、最も良い（評価値の高い）ものを返す。
    QString m_multipv;

    // 思考開始から探索したノード数
    QString m_nodes;

    // 1秒あたりの探索局面数
    QString m_nps;

    // 思考を開始してから経過した時間（単位はミリ秒）
    QString m_time;

    // 評価値
    QString m_score;

    // info行のGUIに表示させたい任意の文字列
    QString m_string;

    // エンジンによる現在の評価値
    QString m_scoreCp;

    // エンジンが詰みを発見した場合の詰み手数
    QString m_scoreMate;

    // 現在の読み筋を漢字表示で表した文字列
    QString m_pvKanjiStr;

    // 現在の読み筋（USI形式、スペース区切り）
    QString m_pvUsiStr;

    // エンジンが現在使用しているハッシュの使用率
    QString m_hashfull;

    // GUIの「探索手」の欄に表示する読み筋の最初の文字列
    QString m_searchedHand;

    // 直前の指し手のマスの筋
    int m_previousFileTo;

    // 直前の指し手のマスの段
    int m_previousRankTo;

    // 駒台の段と駒の対応マップ
    QMap<int, QChar> m_pieceMap;

    // 駒とその成駒の対応マップ
    QMap<QChar, QChar> m_promoteMap;

    // 駒台の駒と段の対応マップ
    QMap<QChar, int> m_pieceCharToIntMap;

    // 駒文字と漢字の駒の対応マップ
    QMap<QChar, QString> m_pieceMapping;

    // フラグの変数名
    EvaluationBound m_evaluationBound;

    // 思考開始時の手番（info処理時に使用し、局面更新による影響を受けない）
    ShogiGameController::Player m_thinkingStartPlayer = ShogiGameController::Player1;

    // 指し手文字列の三角マークを返す。
    // 例．「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
    QString getMoveSymbol(const int moveIndex, const ShogiGameController* algorithm, const bool isPondering) const;

    // 直前の指し手と同じマスに指す場合、「同」を付ける。（同歩、同銀など）
    QString getFormattedMove(int fileTo, int rankTo, const QString& kanji) const;

    // 将棋エンジンから受信した読み筋を将棋の指し手の文字列に変換する。
    // 例．
    // 「7g7h 2f2e 8e8f 2e2d 2c2d 8i7g 8f8g+」
    // 「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
    QString convertMoveToShogiString(const QString& kanji, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo,
                                     const bool promote, const ShogiGameController* algorithm, const int moveIndex, const bool isPondering);

    // 段を示す文字を整数に変換する関数
    // 文字'a'から'i'までを1から9に変換する。
    int convertRankCharToInt(const QChar rankChar);

    // 駒文字を駒台の段番号に変換する。
    int convertPieceToStandRank(const QChar pieceChar);

    // 引数で与えられた文字が将棋盤のランクを表す文字（'a'から'i'）であるかどうかをチェックする。
    bool isBoardRankChar(const QChar rankChar) const;

    // 指し手を表す文字列から指し手のマスの座標と成るかどうかのフラグを取得する。
    int parseMoveString(const QString& moveStr, int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, bool& promote);

    // 駒文字から漢字の駒を返す。
    QString getPieceKanjiName(QChar symbol);

    // 指定した位置の駒を表す文字を返す。
    QChar getPieceCharacter(const QVector<QChar>& boardData, const int file, const int rank);

    // 将棋盤のマスに駒を配置する。
    bool setData(QVector<QChar>& boardData, const int file, const int rank, const QChar piece) const;

    // 駒を指定したマスへ移動する。配置データのみを更新する。
    void movePieceToSquare(QVector<QChar>& boardData, QChar movingPiece, int fileFrom, int rankFrom, int fileTo, int rankTo, bool promote) const;

    // 将棋エンジンから受信したinfo行を解析し、depthなどのサブコマンドの値を取得する。
    void parseEngineInfoTokens(const QStringList& tokens, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                               const bool isPondering);

    // pvの情報を解析し、それに基づいて盤面を更新する。
    int parsePvAndSimulateMoves(const QStringList& pvstr, const ShogiGameController* m_algorithm, QVector<QChar> copyBoardData,
                                const bool isPondering);

    // scoreサブコマンドの解析を行う。
    // score cp、score mate、およびそれらに関連する lowerbound、upperboundの処理を行う。
    void parseScore(const QStringList& tokens, int index);

    // 将棋エンジンからinfo currmove <move>が返された場合、その漢字の指し手に変換する。
    QString convertCurrMoveToKanjiNotation(const QString& str, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                           const bool isPondering);
};

#endif // SHOGIENGINEINFOPARSER_H
