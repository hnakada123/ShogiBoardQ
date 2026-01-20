#ifndef JISHOGICALCULATOR_H
#define JISHOGICALCULATOR_H

#include <QVector>
#include <QMap>
#include <QChar>
#include <QString>

// 持将棋（入玉宣言法）の点数計算を行うクラス
class JishogiCalculator
{
public:
    // 計算結果を格納する構造体
    struct PlayerScore {
        int totalPoints;      // 合計点数（盤上＋持ち駒）
        int declarationPoints; // 宣言点数（敵陣にある駒＋持ち駒）
        int piecesInEnemyTerritory; // 敵陣にある駒の数（玉を除く）
        bool kingInEnemyTerritory;  // 玉が敵陣にいるか
    };

    // 点数計算結果
    struct JishogiResult {
        PlayerScore sente; // 先手の点数
        PlayerScore gote;  // 後手の点数
    };

    // 盤面データと駒台データから点数を計算する
    static JishogiResult calculate(const QVector<QChar>& boardData,
                                   const QMap<QChar, int>& pieceStand);

    // 宣言条件を満たしているか（玉が敵陣、敵陣に10枚以上、王手がかかっていない）
    // kingInCheck: 玉が王手されているかどうか
    static bool meetsDeclarationConditions(const PlayerScore& score, bool kingInCheck);

    // 24点法での判定文字列を取得する
    // - 31点以上: 勝ち
    // - 24〜30点: 引き分け（持将棋成立）
    // - 24点未満または条件未達: 宣言失敗
    // kingInCheck: 玉が王手されているかどうか
    static QString getResult24(const PlayerScore& score, bool kingInCheck);

    // 27点法での判定文字列を取得する
    // - 先手: 28点以上で勝ち
    // - 後手: 27点以上で勝ち
    // isSente: true=先手, false=後手
    // kingInCheck: 玉が王手されているかどうか
    static QString getResult27(const PlayerScore& score, bool isSente, bool kingInCheck);

private:
    // 駒の点数を取得する（大駒: 5点、小駒: 1点）
    static int getPiecePoints(QChar piece);

    // 駒が大駒かどうかを判定する
    static bool isMajorPiece(QChar piece);

    // 先手の駒かどうかを判定する（大文字）
    static bool isSentePiece(QChar piece);

    // 後手の駒かどうかを判定する（小文字）
    static bool isGotePiece(QChar piece);
};

#endif // JISHOGICALCULATOR_H
