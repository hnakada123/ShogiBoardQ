#ifndef SHOGIVIEWHIGHLIGHTING_H
#define SHOGIVIEWHIGHLIGHTING_H

/// @file shogiviewhighlighting.h
/// @brief 将棋盤面のハイライト・矢印の責務を担うクラスの定義

#include "shogiview.h"

#include <QColor>
#include <QList>

class QPainter;

/// 盤面のハイライト描画・矢印描画を担当するクラス（対局者カードの手番表示は BoardChromeWidget）。
/// ShogiView が値メンバとして所有し、描画時やスタイル変更時に委譲する。
class ShogiViewHighlighting : public QObject
{
    Q_OBJECT

public:
    explicit ShogiViewHighlighting(ShogiView* view, QObject* parent = nullptr);

    // ──────────────── ハイライト管理 ────────────────
    // ハイライト配列は非所有。寿命は呼び出し側（Controller/Dialog）が管理する。
    void addHighlight(ShogiView::Highlight* hl);
    void removeHighlight(ShogiView::Highlight* hl);
    void removeHighlightAllData();
    ShogiView::Highlight* highlight(int index) const { return m_highlights.at(index); }
    int highlightCount() const { return static_cast<int>(m_highlights.size()); }

    // ──────────────── 矢印管理 ────────────────
    void setArrows(const QList<ShogiView::Arrow>& arrows);
    const QList<ShogiView::Arrow>& arrows() const { return m_arrows; }
    void clearArrows();

    // ──────────────── 描画 ────────────────
    /// dirty（再描画範囲）に掛からないマスは描かない。
    void drawHighlights(QPainter& painter, const QRegion& dirty);
    void drawArrows(QPainter& painter, const ShogiViewLayout& layout);
    /// 矢印（線・矢じり・順位の丸印）が掛かる範囲。描き直す範囲を矢印の分だけに絞るために使う
    QRect arrowsPaintRect(const QList<ShogiView::Arrow>& arrows) const;

signals:
    void highlightsCleared();

private:
    void updateHighlightArea(const ShogiView::Highlight* hl);

    ShogiView* m_view;

    // ハイライト/矢印データ
    QList<ShogiView::Highlight*> m_highlights;
    QList<ShogiView::Arrow> m_arrows;
};

#endif // SHOGIVIEWHIGHLIGHTING_H
