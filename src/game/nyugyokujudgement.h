#ifndef NYUGYOKUJUDGEMENT_H
#define NYUGYOKUJUDGEMENT_H

/// @file nyugyokujudgement.h
/// @brief 入玉宣言の判定（人間の宣言とエンジンの bestmove win で共通）

#include <QCoreApplication>
#include <QString>

class ShogiBoard;

/**
 * @brief 盤面から入玉宣言の成否を判定し、結果の説明を作る
 *
 * 宣言条件（玉が敵陣・敵陣に玉以外の駒が10枚以上・王手がかかっていない）と
 * 宣言点数を、対局ダイアログで選んだ持将棋ルール（24点法・27点法）で判定する。
 */
class NyugyokuJudgement
{
    Q_DECLARE_TR_FUNCTIONS(NyugyokuJudgement)

public:
    /// 持将棋ルール（対局ダイアログの設定値と同じ）
    enum Rule { RuleNone = 0, Rule24 = 1, Rule27 = 2 };

    struct Result {
        bool success = false;  ///< 宣言勝ち、または持将棋（引き分け）
        bool isDraw = false;   ///< 持将棋（24点法の24〜30点）
        QString resultText;    ///< 「宣言勝ち」「持将棋（引き分け）」「宣言失敗（負け）」
        QString message;       ///< 宣言条件の判定を含む結果の説明
    };

    /// 対局ダイアログで選んだ持将棋ルール
    static int configuredRule();

    /// 宣言側（declarerIsSente）の入玉宣言を盤面から判定する。rule は Rule24 または Rule27
    static Result judge(const ShogiBoard& board, bool declarerIsSente, int rule);

    /// エンジンの宣言を、設定した持将棋ルールで判定する。
    /// 「なし」の場合は宣言を取り消せないため、27点法（CSAの標準）で判定してその旨を説明に加える。
    static Result judgeEngineDeclaration(const ShogiBoard& board, bool declarerIsSente);

    /// 結果ダイアログのタイトル
    static QString resultTitle();
};

#endif // NYUGYOKUJUDGEMENT_H
