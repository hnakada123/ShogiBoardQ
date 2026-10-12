#ifndef SHOGIVIEWHIGHLIGHTING_H
#define SHOGIVIEWHIGHLIGHTING_H

/// @file shogiviewhighlighting.h
/// @brief 将棋盤面のハイライト・矢印・手番表示の責務を担うクラスの定義

#include "shogiview.h"

#include <QColor>
#include <QList>
#include <QPointer>
#include <QString>

#include <array>

class QFrame;
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
    const QList<ShogiView::Arrow>& arrows() const { return m_arrows; }
    void clearArrows();

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
    /// dirty（再描画範囲）に掛からないマスは描かない。
    void drawHighlights(QPainter& painter, const QRegion& dirty);
    void drawArrows(QPainter& painter, const ShogiViewLayout& layout);
    /// 矢印（線・矢じり・順位の丸印）が掛かる範囲。描き直す範囲を矢印の分だけに絞るために使う
    QRect arrowsPaintRect(const QList<ShogiView::Arrow>& arrows) const;

    // ──────────────── 状態アクセサ ────────────────
    bool blackActive() const { return m_blackActive; }

    // ──────────────── 起動時スタイル ────────────────
    void applyStartupTypography();
    void refreshBackgroundColors();

signals:
    void highlightsCleared();

private:
    void updateHighlightArea(const ShogiView::Highlight* hl);

    ShogiView* m_view;

    // ハイライト/矢印データ
    QList<ShogiView::Highlight*> m_highlights;
    QList<ShogiView::Arrow> m_arrows;

    // setHighlightStyle() による一時的な上書き。通常は盤面の配色設定を参照する。
    QColor m_highlightBg;
    QColor m_highlightFgOn;
    QColor m_highlightFgOff;
    bool   m_blackActive = true;
    bool   m_turnHighlightActive = false;

    Urgency m_urgency = Urgency::Normal;

    // 対局者の名前・時計・枠・手番の札のスタイルシート。手番の交代では何度も、時計の表示が
    // 変わるたび（1秒ごと）にも呼ばれるので、色の組が前回と同じなら前回の文字列を使う。
    struct CachedStyleSheet {
        std::array<QRgb, 3> colors{};
        int variant = -1;
        QString css;
        /// 色の組か種類が前回と違えば覚え直して true を返す（css を組み立て直す）
        bool update(const std::array<QRgb, 3>& newColors, int newVariant);
    };
    enum PlayerStyleTarget { NameStyle, ClockStyle, CardStyle, BadgeStyle, PlayerStyleCount };
    std::array<CachedStyleSheet, 2 * PlayerStyleCount> m_playerStyleSheets;  ///< 先手・後手の順
    std::array<QPointer<QFrame>, 2> m_playerCards;   ///< 先手・後手の枠
    std::array<QPointer<QLabel>, 2> m_turnBadges;    ///< 先手・後手の手番の札

    // ラベルスタイルヘルパ
    void refreshPlayerStyles();
    void setLabelStyle(QLabel* lbl, const QColor& fg, const QColor& bg, const QColor& border, bool bold,
                       CachedStyleSheet& cache);
    static QString toRgba(const QColor& c);
};

#endif // SHOGIVIEWHIGHLIGHTING_H
