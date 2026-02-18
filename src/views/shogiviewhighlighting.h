#ifndef SHOGIVIEWHIGHLIGHTING_H
#define SHOGIVIEWHIGHLIGHTING_H

/// @file shogiviewhighlighting.h
/// @brief 将棋盤面のハイライト・矢印・手番表示の責務を担うクラスの定義

#include "shogiview.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QVector>

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
    void addHighlight(ShogiView::Highlight* hl);
    void removeHighlight(ShogiView::Highlight* hl);
    void removeHighlightAllData();
    ShogiView::Highlight* highlight(int index) const { return m_highlights.at(index); }
    int highlightCount() const { return static_cast<int>(m_highlights.size()); }

    // ──────────────── 矢印管理 ────────────────
    void setArrows(const QVector<ShogiView::Arrow>& arrows);
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

signals:
    void highlightsCleared();

private:
    ShogiView* m_view;

    // ハイライト/矢印データ
    QList<ShogiView::Highlight*> m_highlights;
    QVector<ShogiView::Arrow> m_arrows;

    // 駒打ち矢印の駒画像キャッシュ（キー: 駒文字+サイズ）
    mutable QHash<quint64, QPixmap> m_arrowDropPieceCache;

    // 手番ハイライト色
    QColor m_highlightBg    = QColor(255, 255, 0);
    QColor m_highlightFgOn  = QColor(0, 0, 255);
    QColor m_highlightFgOff = QColor(51, 51, 51);
    bool   m_blackActive    = true;

    // 緊急度配色
    QColor m_urgencyBgWarn10 = QColor(255, 193, 7);
    QColor m_urgencyFgWarn10 = QColor(32, 32, 32);
    QColor m_urgencyBgWarn5  = QColor(229, 57, 53);
    QColor m_urgencyFgWarn5  = QColor(255, 255, 255);

    // 色定数
    const QColor kTurnBg   = QColor(255, 255, 0);
    const QColor kTurnFg   = QColor(0, 64, 255);

    const QColor kWarn10Bg = QColor(255, 255, 0);
    const QColor kWarn10Fg = QColor(0, 0, 255);

    const QColor kWarn5Bg  = QColor(255, 255, 0);
    const QColor kWarn5Fg  = QColor(255, 0, 0);

    const QColor kWarn10Border = QColor(0, 0, 255);
    const QColor kWarn5Border  = QColor(255, 0, 0);

    Urgency m_urgency = Urgency::Normal;

    // ラベルスタイルヘルパ
    void setLabelStyle(QLabel* lbl, const QColor& fg, const QColor& bg,
                       int borderPx, const QColor& borderColor, bool bold);
    static QString toRgb(const QColor& c);
};

#endif // SHOGIVIEWHIGHLIGHTING_H
