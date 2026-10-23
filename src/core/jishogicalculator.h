#ifndef JISHOGICALCULATOR_H
#define JISHOGICALCULATOR_H

/// @file jishogicalculator.h
/// @brief 持将棋（入玉宣言法）の点数計算・判定ロジック

#include <QList>
#include <QMap>
#include <QString>
#include "shogiposition.h"
#include "shogitypes.h"

/**
 * @brief 持将棋（入玉宣言法）の点数計算と宣言の判定
 *
 * 盤面と持ち駒から宣言点数を数え、24点法・27点法で宣言の成否を判定する。点数の閾値はここだけで決める
 * （判定の説明の文言は、対局の判定 NyugyokuJudgement と点数のダイアログが作る）。
 */
class JishogiCalculator
{
public:
    // --- 計算結果 ---

    /// プレイヤーごとの点数情報
    struct PlayerScore {
        int totalPoints;           ///< 合計点数（盤上＋持ち駒）
        int declarationPoints;     ///< 宣言点数（敵陣にある駒＋持ち駒）
        int piecesInEnemyTerritory; ///< 敵陣にある駒の数（玉を除く）
        bool kingInEnemyTerritory; ///< 玉が敵陣にいるか
    };

    /// 先手・後手それぞれの計算結果
    struct JishogiResult {
        PlayerScore sente; ///< 先手の点数
        PlayerScore gote;  ///< 後手の点数
    };

    /// 持将棋の点数法
    enum class Rule {
        Points24,  ///< 24点法（31点以上で勝ち、24〜30点で持将棋）
        Points27,  ///< 27点法（先手28点・後手27点以上で勝ち）
    };

    /// 宣言の判定
    enum class Verdict {
        NotDeclarable,  ///< 宣言条件（玉が敵陣・敵陣に10枚以上・王手なし）を満たさない（宣言すれば負け）
        ShortOfPoints,  ///< 条件は満たすが点数が足りない（宣言すれば負け）
        Draw,           ///< 持将棋（24点法の24〜30点）
        Win,            ///< 宣言勝ち
    };

    // --- 公開API ---

    /// 局面（盤と持ち駒）から点数を計算する
    static JishogiResult calculate(const Position& position);

    /// 宣言条件を満たしているか（玉が敵陣、敵陣に10枚以上、王手がかかっていない）
    /// @param kingInCheck 玉が王手されているかどうか
    static bool meetsDeclarationConditions(const PlayerScore& score, bool kingInCheck);

    /// 宣言側（isSente）の宣言を rule で判定する
    static Verdict judge(const PlayerScore& score, Rule rule, bool isSente, bool kingInCheck);

    /// 宣言して負けにならない宣言点数の下限（24点法は持将棋の24点、27点法は勝ちの点数）
    static int requiredPoints(Rule rule, bool isSente);

    /// 勝ちに要る宣言点数（24点法は31点、27点法は先手28点・後手27点）
    static int winningPoints(Rule rule, bool isSente);

private:
    // --- 内部ヘルパー ---

    /// 駒の点数を取得する（大駒: 5点、小駒: 1点、玉: 0点）
    static int piecePoints(Piece piece);

    /// 駒が大駒（飛車・角・龍・馬）かどうかを判定する
    static bool isMajorPiece(Piece piece);

    /// 先手の駒かどうかを判定する（大文字）
    static bool isSentePiece(Piece piece);

    /// 後手の駒かどうかを判定する（小文字）
    static bool isGotePiece(Piece piece);
};

#endif // JISHOGICALCULATOR_H
