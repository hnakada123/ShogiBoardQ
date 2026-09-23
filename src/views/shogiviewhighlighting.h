#ifndef SHOGIVIEWHIGHLIGHTING_H
#define SHOGIVIEWHIGHLIGHTING_H

/// @file shogiviewhighlighting.h
/// @brief 将棋盤面のハイライト・矢印・手番表示の責務を担うクラスの定義

#include "shogiview.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QPixmap>

class QPainter;
class QLabel;

/// 盤面のハイライト描画・矢印描画・手番ラベルスタイル管理を担当するクラス。
/// ShogiView が値メンバとして所有し、描画時やスタイル変更時に委譲する。
class ShogiViewHighlighting : public QObject
{
    Q_OBJECT

public:
    explicit ShogiViewHighlighting(ShogiView* view, QObject* parent = nullptr);

    using Urgency = ShogiView::Urgency;

    // ──────────────── ハイライト管理 ────────────────
    // ハイライト配列は非所有。寿命は呼び出し側（Controller/Dialog）が管理する。
    void addHighlight(ShogiView::Highlight* hl);
    void removeHighlight(ShogiView::Highlight* hl);
    void removeHighlightAllData();
    ShogiView::Highlight* highlight(int index) const { return m_highlights.at(index); }
    int highlightCount() const { return static_cast<int>(m_highlights.size()); }

    // ──────────────── 矢印管理 ────────────────
    void setArrows(const QList<ShogiView::Arrow>& arrows);
    void clearArrows();
    void clearDropPieceCache();

    // ──────────────── 手番ハイライト ────────────────
    void setActiveSide(bool blackTurn);
    void setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff);
    void clearTurnHighlight();
    void applyTurnHighlight(bool blackActive);
    void setActiveIsBlack(bool activeIsBlack);

    // ──────────────── 緊急度表示 ────────────────
    void setUrgencyVisuals(Urgency u);
    void applyClockUrgency(qint64 activeRemainMs);
    Urgency urgency() const { return m_urgency; }

    // ──────────────── 描画 ────────────────
    void drawHighlights(QPainter& painter, const ShogiViewLayout& layout);
    void drawArrows(QPainter& painter, const ShogiViewLayout& layout);

    // ──────────────── 状態アクセサ ────────────────
    bool blackActive() const { return m_blackActive; }

    // ──────────────── 起動時スタイル ────────────────
    void applyStartupTypography();
    void refreshBackgroundColors();

signals:
    void highlightsCleared();

private:
    ShogiView* m_view;

    // ハイライト/矢印データ
    QList<ShogiView::Highlight*> m_highlights;
    QList<ShogiView::Arrow> m_arrows;

    // 駒打ち矢印の駒画像キャッシュ（キー: 駒文字+サイズ）
    mutable QHash<quint64, QPixmap> m_arrowDropPieceCache;

    // setHighlightStyle() による一時的な上書き。通常は盤面の配色設定を参照する。
    QColor m_highlightBg;
    QColor m_highlightFgOn;
    QColor m_highlightFgOff;
    bool   m_blackActive = true;
    bool   m_turnHighlightActive = false;

    Urgency m_urgency = Urgency::Normal;

    // ラベルスタイルヘルパ
    void refreshPlayerStyles();
    void setLabelStyle(QLabel* lbl, const QColor& fg, const QColor& bg, const QColor& border, bool bold);
    static QString toRgba(const QColor& c);
};

#endif // SHOGIVIEWHIGHLIGHTING_H
