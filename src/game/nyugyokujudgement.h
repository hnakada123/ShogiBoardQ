#ifndef NYUGYOKUJUDGEMENT_H
#define NYUGYOKUJUDGEMENT_H

/// @file nyugyokujudgement.h
/// @brief 入玉宣言の判定（人間の宣言とエンジンの bestmove win で共通）

#include <QCoreApplication>
#include <QString>
#include <optional>

#include "jishogicalculator.h"

class Position;

/**
 * @brief 盤面から入玉宣言の成否を判定し、結果の説明を作る
 *
 * 宣言条件（玉が敵陣・敵陣に玉以外の駒が10枚以上・王手がかかっていない）と宣言点数を、
 * 対局ダイアログで選んだ持将棋ルール（24点法・27点法）で判定する。判定は core の
 * JishogiCalculator::judge が行い、ここは結果の説明の文言だけを作る。
 */
class NyugyokuJudgement
{
    Q_DECLARE_TR_FUNCTIONS(NyugyokuJudgement)

public:
    struct Result {
        bool success = false;  ///< 宣言勝ち、または持将棋（引き分け）
        bool isDraw = false;   ///< 持将棋（24点法の24〜30点）
        QString resultText;    ///< 「宣言勝ち」「持将棋（引き分け）」「宣言失敗（負け）」
        QString message;       ///< 宣言条件の判定を含む結果の説明
    };

    /// 対局ダイアログで選んだ持将棋ルール（「なし」なら nullopt）
    static std::optional<JishogiCalculator::Rule> configuredRule();
    /// 対局ダイアログの設定値（0: なし、1: 24点法、2: 27点法）を持将棋ルールにする
    static std::optional<JishogiCalculator::Rule> ruleFromSetting(int value);

    /// 宣言側（declarerIsSente）の入玉宣言を盤面から判定する。
    /// handicapNames なら結果の説明で宣言側を下手・上手と呼ぶ
    static Result judge(const Position& position, bool declarerIsSente, JishogiCalculator::Rule rule,
                        bool handicapNames = false);

    /// エンジンの宣言を、設定した持将棋ルールで判定する。
    /// 「なし」の場合は宣言を取り消せないため、27点法（CSAの標準）で判定してその旨を説明に加える。
    static Result judgeEngineDeclaration(const Position& position, bool declarerIsSente,
                                         bool handicapNames = false);

    /// 宣言側の呼び名（先手・後手、駒落ちで handicapNames なら下手・上手）
    static QString declarerName(bool declarerIsSente, bool handicapNames);

    /// 結果ダイアログのタイトル
    static QString resultTitle();
};

#endif // NYUGYOKUJUDGEMENT_H
