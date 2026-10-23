#ifndef SCORETEXT_H
#define SCORETEXT_H

/// @file scoretext.h
/// @brief 詰みの評価値の表記（思考の表・検討の表・棋譜解析の結果の表・評価値グラフで共通）

#include <QCoreApplication>
#include <QString>

#include "usiscore.h"

/**
 * @brief 詰みの評価値の表記
 *
 * 手数があれば「3手詰」（負は負けまでの手数で「-3手詰」）、手数の分からない詰み（"mate +"・"mate -"）は「詰み」。
 * どちらの勝ちかは使う側が添える（表は評価値の符号や形勢の列、評価値グラフは「先手勝ち・3手詰」）。
 */
class ScoreText
{
    Q_DECLARE_TR_FUNCTIONS(ScoreText)

public:
    /// 手数 plies の詰み（「3手詰」。列幅の見積もりにも使う）
    static QString mateIn(int plies);
    /// 手数の分からない詰み（「詰み」）
    static QString mateUnknown();
    /// 詰みの評価値（手数があれば mateIn(score.value)、無ければ mateUnknown()。詰みでなければ空）
    static QString mate(const UsiScore& score);
};

#endif // SCORETEXT_H
