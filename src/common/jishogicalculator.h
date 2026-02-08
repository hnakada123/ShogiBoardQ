#ifndef JISHOGICALCULATOR_H
#define JISHOGICALCULATOR_H

/// @file jishogicalculator.h
/// @brief 持将棋（入玉宣言法）の点数計算・判定ロジック
/// @todo remove コメントスタイルガイド適用済み

#include <QVector>
#include <QMap>
#include <QChar>
#include <QString>

/**
 * @brief 持将棋（入玉宣言法）の点数計算を行うユーティリティクラス
 *
 * 盤面と持ち駒から24点法・27点法の宣言判定を行う。
 *
 * @todo remove コメントスタイルガイド適用済み
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

    // --- 公開API ---

    /// 盤面データと駒台データから点数を計算する
    static JishogiResult calculate(const QVector<QChar>& boardData,
                                   const QMap<QChar, int>& pieceStand);

    /// 宣言条件を満たしているか（玉が敵陣、敵陣に10枚以上、王手がかかっていない）
    /// @param kingInCheck 玉が王手されているかどうか
    static bool meetsDeclarationConditions(const PlayerScore& score, bool kingInCheck);

    /// 24点法での判定文字列を取得する
    /// - 31点以上: 勝ち
    /// - 24〜30点: 引き分け（持将棋成立）
    /// - 24点未満または条件未達: 宣言失敗
    /// @param kingInCheck 玉が王手されているかどうか
    static QString getResult24(const PlayerScore& score, bool kingInCheck);

    /// 27点法での判定文字列を取得する
    /// - 先手: 28点以上で勝ち
    /// - 後手: 27点以上で勝ち
    /// @param isSente true=先手, false=後手
    /// @param kingInCheck 玉が王手されているかどうか
    static QString getResult27(const PlayerScore& score, bool isSente, bool kingInCheck);

private:
    // --- 内部ヘルパー ---

    /// 駒の点数を取得する（大駒: 5点、小駒: 1点、玉: 0点）
    static int getPiecePoints(QChar piece);

    /// 駒が大駒（飛車・角・龍・馬）かどうかを判定する
    static bool isMajorPiece(QChar piece);

    /// 先手の駒かどうかを判定する（大文字）
    static bool isSentePiece(QChar piece);

    /// 後手の駒かどうかを判定する（小文字）
    static bool isGotePiece(QChar piece);
};

#endif // JISHOGICALCULATOR_H
