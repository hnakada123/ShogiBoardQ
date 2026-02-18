#ifndef SHOGIENGINEINFOPARSER_H
#define SHOGIENGINEINFOPARSER_H

/// @file shogiengineinfoparser.h
/// @brief USIエンジンのinfo行パーサの定義


#include <QString>
#include <QObject>
#include "shogigamecontroller.h"

/**
 * @brief USIエンジンが出力するinfo行を解析し、読み筋を漢字表記に変換するクラス
 *
 * info行のサブコマンド（depth, score cp, pv等）を解析し、
 * 盤面コピー上で指し手をシミュレートして漢字表記の読み筋文字列を生成する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ShogiEngineInfoParser : public QObject
{
    Q_OBJECT

public:
    ShogiEngineInfoParser();

    /// 評価値の境界を表す列挙型
    enum class EvaluationBound {
        None,       ///< 境界なし（確定値）
        LowerBound, ///< 下限値（実際にはその値を上回る可能性がある）
        UpperBound  ///< 上限値（実際にはその値を下回る可能性がある）
    };

    EvaluationBound evaluationBound() const;
    void setEvaluationBound(EvaluationBound newEvaluationBound);

    static constexpr int BOARD_SIZE = 9;                    ///< 盤面の1辺のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE; ///< 将棋盤の総マス数
    static constexpr int STAND_FILE = 99;                   ///< 駒台を示す筋番号
    static constexpr int INFO_STRING_SPECIAL_CASE = -2;     ///< "(57.54%)"等の非指し手文字列を示す戻り値

    int previousFileTo() const;
    void setPreviousFileTo(int newPreviousFileTo);
    int previousRankTo() const;
    void setPreviousRankTo(int newPreviousRankTo);

    /**
     * @brief info行の読み筋を解析し、漢字表記の読み筋文字列を生成する
     *
     * USI形式の読み筋（例: "7g7h 2f2e 8e8f"）を解析し、
     * 漢字表記（例: "△７八馬(77)▲２五歩(26)△８六歩(85)"）に変換する。
     */
    void parseEngineOutputAndUpdateState(const QString& line, const ShogiGameController* algorithm,  QVector<QChar>& clonedBoardData,
                                         const bool isPondering);

    void setThinkingStartPlayer(ShogiGameController::Player player);
    ShogiGameController::Player thinkingStartPlayer() const;

    // --- 解析結果アクセサ ---

    QString depth() const;
    QString seldepth() const;

    /// pvで初手の異なる複数の読み筋のうち、何番目に良い読み筋かを返す
    QString multipv() const;

    QString nodes() const;
    QString nps() const;
    QString time() const;
    QString scoreCp() const;
    QString scoreMate() const;

    /// 漢字表記の読み筋文字列を返す
    QString pvKanjiStr() const;

    /// USI形式のスペース区切り読み筋文字列を返す
    QString pvUsiStr() const;

    /// 現在思考中の手（currmoveの漢字変換結果）
    QString currmove() const;

    QString hashfull() const;

    /// GUIの「探索手」欄に表示する読み筋の先頭手
    QString searchedHand() const;

    void setScoreMate(const QString& newScoremate);

    /// bestmoveの予想手を漢字表記に変換する
    QString convertPredictedMoveToKanjiString(const ShogiGameController* algorithm, QString& predictedOpponentMove, QVector<QChar>& clonedBoardData);

    /// 指し手を解析し、盤面コピーに適用する
    void parseAndApplyMoveToClonedBoard(const QString& str, QVector<QChar>& clonedBoardData);

    /// 盤面データをデバッグ出力する
    void printShogiBoard(const QVector<QChar>& boardData) const;

    void setScore(const QString& newScore);
    QString score() const;

signals:
    /// エラーを報告する（→ ErrorBus経由でMainWindow）
    void errorOccurred(const QString& errorMessage);

private:
    QString m_depth;            ///< 探索深さ
    QString m_seldepth;         ///< 選択的探索深さ
    QString m_multipv;          ///< MultiPVのインデックス
    QString m_nodes;            ///< 探索ノード数
    QString m_nps;              ///< 1秒あたりの探索局面数
    QString m_time;             ///< 思考開始からの経過時間（ミリ秒）
    QString m_score;            ///< 評価値文字列
    QString m_string;           ///< info stringサブコマンドの値
    QString m_scoreCp;          ///< 評価値（centipawn）
    QString m_scoreMate;        ///< 詰み手数
    QString m_pvKanjiStr;       ///< 漢字表記の読み筋文字列
    QString m_pvUsiStr;         ///< USI形式の読み筋文字列
    QString m_hashfull;         ///< ハッシュ使用率（千分率）
    QString m_searchedHand;     ///< 探索手（読み筋の先頭手）
    int m_previousFileTo = 0;   ///< 直前の指し手の筋
    int m_previousRankTo = 0;   ///< 直前の指し手の段

    QMap<int, QChar> m_pieceMap;            ///< 駒台の段番号 → 駒文字
    QMap<QChar, QChar> m_promoteMap;        ///< 駒文字 → 成駒文字
    QMap<QChar, int> m_pieceCharToIntMap;   ///< 駒文字 → 駒台の段番号
    QMap<QChar, QString> m_pieceMapping;    ///< 駒文字 → 漢字の駒名

    EvaluationBound m_evaluationBound = EvaluationBound::None; ///< 評価値の境界種別

    /// 思考開始時の手番（info処理中は局面更新の影響を受けない）
    ShogiGameController::Player m_thinkingStartPlayer = ShogiGameController::Player1;

    /// 手番に応じた先手/後手マーク（▲/△）を返す
    QString getMoveSymbol(const int moveIndex, const ShogiGameController* algorithm, const bool isPondering) const;

    /// 直前と同じマスへの指し手なら「同」を付ける
    QString getFormattedMove(int fileTo, int rankTo, const QString& kanji) const;

    /**
     * @brief USI形式の指し手を漢字表記に変換する
     *
     * 例: "7g7h" → "△７八馬(77)"
     */
    QString convertMoveToShogiString(const QString& kanji, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo,
                                     const bool promote, const ShogiGameController* algorithm, const int moveIndex, const bool isPondering);

    /// 段文字（'a'〜'i'）を整数（1〜9）に変換する
    int convertRankCharToInt(const QChar rankChar);

    /// 駒文字を駒台の段番号に変換する
    int convertPieceToStandRank(const QChar pieceChar);

    /// 文字が盤面のランク（'a'〜'i'）かを判定する
    bool isBoardRankChar(const QChar rankChar) const;

    /**
     * @brief 指し手文字列からマス座標と成フラグを取得する
     * @return 0: 正常、-1: エラー、INFO_STRING_SPECIAL_CASE: 非指し手文字列
     */
    int parseMoveString(const QString& moveStr, int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, bool& promote);

    /// 駒文字（英字）から漢字の駒名を返す
    QString getPieceKanjiName(QChar symbol);

    /// 指定マスの駒文字を返す（file=STAND_FILEなら駒台から取得）
    QChar getPieceCharacter(const QVector<QChar>& boardData, const int file, const int rank);

    /// 盤面データの指定マスに駒を配置する
    bool setData(QVector<QChar>& boardData, const int file, const int rank, const QChar piece) const;

    /// 駒を移動して盤面コピーを更新する
    void movePieceToSquare(QVector<QChar>& boardData, QChar movingPiece, int fileFrom, int rankFrom, int fileTo, int rankTo, bool promote) const;

    /// info行のトークンを順に解析し、各サブコマンドの値を取得する
    void parseEngineInfoTokens(const QStringList& tokens, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                               const bool isPondering);

    /// pvトークンを順に解析し、盤面コピー上で指し手をシミュレートする
    int parsePvAndSimulateMoves(const QStringList& pvstr, const ShogiGameController* m_algorithm, QVector<QChar>& copyBoardData,
                                const bool isPondering);

    /// score cp / score mate / lowerbound / upperbound を解析する
    void parseScore(const QStringList& tokens, int index);

    /// currmoveサブコマンドの指し手を漢字表記に変換する
    QString convertCurrMoveToKanjiNotation(const QString& str, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                           const bool isPondering);

    /// 1行ごとの解析結果を初期化する（再利用時の状態リーク防止）
    void clearParsedInfo();
};

#endif // SHOGIENGINEINFOPARSER_H
