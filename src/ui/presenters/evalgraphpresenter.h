#ifndef EVALGRAPHPRESENTER_H
#define EVALGRAPHPRESENTER_H

/// @file evalgraphpresenter.h
/// @brief 評価値グラフプレゼンタクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QtCore/QList>

class MatchCoordinator;  // 前方宣言（ポインタ使用のみ／重い依存を避ける）

namespace EvalGraphPresenter {

// 主エンジンの評価値(cp)を配列へ追記します。
// - scoreCp: 評価値の蓄積先（呼び出し側の m_scoreCp を渡す）
// - match  : エンジン参照用（nullptr 可。nullptr の場合は 0 を追記）
void appendPrimaryScore(QList<int>& scoreCp, MatchCoordinator* match);

// セカンダリエンジンの評価値(cp)を配列へ追記します。
// - scoreCp: 評価値の蓄積先（呼び出し側の m_scoreCp を渡す）
// - match  : エンジン参照用（nullptr 可。nullptr の場合は 0 を追記）
void appendSecondaryScore(QList<int>& scoreCp, MatchCoordinator* match);

} // namespace EvalGraphPresenter

#endif // EVALGRAPHPRESENTER_H
