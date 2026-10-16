#ifndef SHOGIVIEWHIGHLIGHTING_H
#define SHOGIVIEWHIGHLIGHTING_H

/// @file shogiviewhighlighting.h
/// @brief 将棋盤面のハイライト・矢印の責務を担うクラスの定義

#include "shogiview.h"

#include <QList>
#include <QPoint>
#include <array>

class QPainter;

/// 盤のハイライトの層。色と描く順は層で決まる（呼び出し側は色を渡さない）
enum class BoardHighlightLayer {
    LastMoveFrom,       ///< 直前の手の移動元
    LastMoveTo,         ///< 直前の手の移動先
    Selection,          ///< つまんでいる駒
    LegalDestinations,  ///< つまんでいる駒の合法な移動先
};

/// 盤面のハイライト描画・矢印描画を担当するクラス（対局者カードの手番表示は BoardChromeWidget）。
/// ShogiView が値メンバとして所有し、描画時やスタイル変更時に委譲する。
class ShogiViewHighlighting : public QObject
{
    Q_OBJECT

public:
    static constexpr int kLayerCount = 4;

    explicit ShogiViewHighlighting(ShogiView* view, QObject* parent = nullptr);

    // ──────────────── ハイライト ────────────────
    /// 層のマスを置き換える（変わったマスだけ描き直す）
    void setHighlights(BoardHighlightLayer layer, const QList<QPoint>& squares);
    /// 全ての層を消す
    void clearHighlights();
    const QList<QPoint>& highlights(BoardHighlightLayer layer) const;

    // ──────────────── 矢印管理 ────────────────
    void setArrows(const QList<ShogiView::Arrow>& arrows);
    const QList<ShogiView::Arrow>& arrows() const { return m_arrows; }
    void clearArrows();

    // ──────────────── 描画 ────────────────
    /// dirty（再描画範囲）に掛からないマスは描かない。
    void drawHighlights(QPainter& painter, const QRegion& dirty);
    void drawArrows(QPainter& painter);
    /// 矢印（線・矢じり・順位の丸印）が掛かる範囲。描き直す範囲を矢印の分だけに絞るために使う
    QRect arrowsPaintRect(const QList<ShogiView::Arrow>& arrows) const;

private:
    /// マスを描き直す（座標が盤の外なら盤全体）
    void updateSquares(const QList<QPoint>& squares);

    ShogiView* m_view;

    // ハイライト/矢印データ
    std::array<QList<QPoint>, kLayerCount> m_layers;
    QList<ShogiView::Arrow> m_arrows;
};

#endif // SHOGIVIEWHIGHLIGHTING_H
