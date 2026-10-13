#ifndef SHOGIENGINEINFOPARSER_H
#define SHOGIENGINEINFOPARSER_H

/// @file shogiengineinfoparser.h
/// @brief USIエンジンのinfo行パーサの定義


#include <QList>
#include <QString>
#include <QStringView>
#include "shogiboard.h"
#include "usimove.h"

/**
 * @brief USIエンジンが出力するinfo行を解析し、読み筋を漢字表記に変換するクラス
 *
 * info行のサブコマンド（depth, score cp, pv等）を解析し、
 * 思考を始めた局面の盤の写しの上で指し手を進めて、漢字表記の読み筋文字列を生成する。
 *
 */
class ShogiEngineInfoParser
{
public:
    ShogiEngineInfoParser() = default;

    /// 評価値の境界を表す列挙型
    enum class EvaluationBound {
        None,       ///< 境界なし（確定値）
        LowerBound, ///< 下限値（実際にはその値を上回る可能性がある）
        UpperBound  ///< 上限値（実際にはその値を下回る可能性がある）
    };

    EvaluationBound evaluationBound() const;

    /**
     * @brief 読み筋を日本語にするときの局面（思考を始めた局面）を設定する
     *
     * 読み筋の初手はこの局面の手番の手として書く。読めない SFEN なら読み筋を日本語にしない。
     */
    void setPosition(const QString& sfen);

    /// 局面に至った直前の手の移動先（読み筋の初手が同じマスへ行くなら「同」と書く。無効なマスなら書かない）
    void setPreviousMove(BoardSquare to);

    /**
     * @brief info行の読み筋を解析し、漢字表記の読み筋文字列を生成する
     *
     * USI形式の読み筋（例: "7g7h 2f2e 8e8f"）を解析し、
     * 漢字表記（例: "△７八馬(77)▲２五歩(26)△８六歩(85)"）に変換する。
     */
    void parseEngineOutputAndUpdateState(const QString& line);

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

    QString hashfull() const;

    /// GUIの「探索手」欄に表示する読み筋の先頭手
    QString searchedHand() const;
    QString searchedUsi() const { return m_searchedUsi; }

    void setScore(const QString& newScore);
    QString score() const;

private:
    QString m_depth;            ///< 探索深さ
    QString m_seldepth;         ///< 選択的探索深さ
    QString m_multipv;          ///< MultiPVのインデックス
    QString m_nodes;            ///< 探索ノード数
    QString m_nps;              ///< 1秒あたりの探索局面数
    QString m_time;             ///< 思考開始からの経過時間（ミリ秒）
    QString m_score;            ///< 評価値文字列
    QString m_scoreCp;          ///< 評価値（centipawn）
    QString m_scoreMate;        ///< 詰み手数
    QString m_pvKanjiStr;       ///< 漢字表記の読み筋文字列
    QString m_pvUsiStr;         ///< USI形式の読み筋文字列
    QString m_hashfull;         ///< ハッシュ使用率（千分率）
    QString m_searchedUsi;
    QString m_searchedHand;     ///< 探索手（読み筋の先頭手）

    EvaluationBound m_evaluationBound = EvaluationBound::None; ///< 評価値の境界種別

    ShogiBoard m_position;          ///< 思考を始めた局面
    bool m_hasPosition = false;     ///< m_position を設定できたか
    ShogiBoard m_pvBoard;           ///< 読み筋を進める盤（info 行ごとに m_position を写す）
    BoardSquare m_previousTo;       ///< 局面に至った直前の手の移動先

    /// board の上で指し手の動かす駒を返す（mover の駒でない、駒台にない、行き先が自分の駒などは None）
    static Piece movingPiece(const ShogiBoard& board, const UsiMove& move, Turn mover);

    /// info行のトークンを順に解析し、各サブコマンドの値を取得する（pv の位置を返す。なければ -1）
    qsizetype parseEngineInfoTokens(const QList<QStringView>& tokens);

    /// pvトークン（tokens の first 番目以降）を、盤の写しの上で指し手を進めながら漢字表記にする
    void parsePvAndSimulateMoves(const QList<QStringView>& tokens, qsizetype first);

    /// score cp / score mate / lowerbound / upperbound を解析する
    void parseScore(const QList<QStringView>& tokens, qsizetype index);

    /// currmoveサブコマンドの指し手を漢字表記に変換する
    QString convertCurrMoveToKanjiNotation(QStringView str) const;

    /// 1行ごとの解析結果を初期化する（再利用時の状態リーク防止）
    void clearParsedInfo();
};

#endif // SHOGIENGINEINFOPARSER_H
