#ifndef SHOGIVIEWHIGHLIGHTING_H
#define SHOGIVIEWHIGHLIGHTING_H

/// @file shogiviewhighlighting.h
/// @brief 将棋盤面のハイライト・矢印の責務を担うクラスの定義

#include <QColor>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <array>

#include "shogitypes.h"

class QPainter;
class QRegion;
class ShogiView;

/// 盤のハイライトの層。色と描く順は層で決まる（呼び出し側は色を渡さない）
enum class BoardHighlightLayer {
    LastMoveFrom,       ///< 直前の手の移動元
    LastMoveTo,         ///< 直前の手の移動先
    Selection,          ///< つまんでいる駒
    LegalDestinations,  ///< つまんでいる駒の合法な移動先
};

/// 盤面のハイライト描画・矢印描画を担当するクラス（対局者カードの手番表示は BoardChromeWidget）。
/// ShogiView が所有し、描画時やスタイル変更時に委譲する。ハイライトと矢印は ShogiView::highlighting() から
/// これに直接渡す（ShogiView は転送しない）
class ShogiViewHighlighting : public QObject
{
    Q_OBJECT

public:
    static constexpr int kLayerCount = 4;

    /// 盤上に描く矢印（検討・対局の読み・定跡手）
    struct Arrow {
        int fromFile = 0;   // 移動元の筋（1-9）、駒打ちの場合は0
        int fromRank = 0;   // 移動元の段（1-9）、駒打ちの場合は0
        int toFile = 0;     // 移動先の筋（1-9）
        int toRank = 0;     // 移動先の段（1-9）
        int priority = 0;   // 優先順位（1が最善手、2以上で数字を表示）
        Piece dropPiece = Piece::None;  // 駒打ちの場合の打つ側の駒、通常の移動は None
        QColor color = QColor(255, 0, 0, 200);  // 半透明の赤
        Qt::PenStyle penStyle = Qt::SolidLine;

        bool operator==(const Arrow& other) const
        {
            return fromFile == other.fromFile && fromRank == other.fromRank
                && toFile == other.toFile && toRank == other.toRank && priority == other.priority
                && dropPiece == other.dropPiece && color == other.color && penStyle == other.penStyle;
        }
        bool operator!=(const Arrow& other) const { return !(*this == other); }
    };

    explicit ShogiViewHighlighting(ShogiView* view, QObject* parent = nullptr);

    // ──────────────── ハイライト ────────────────
    /// 層のマスを置き換える（変わったマスだけ描き直す）
    void setHighlights(BoardHighlightLayer layer, const QList<QPoint>& squares);
    const QList<QPoint>& highlights(BoardHighlightLayer layer) const;

    // ──────────────── 矢印管理 ────────────────
    void setArrows(const QList<Arrow>& arrows);
    const QList<Arrow>& arrows() const { return m_arrows; }

    // ──────────────── 描画 ────────────────
    /// dirty（再描画範囲）に掛からないマスは描かない。
    void drawHighlights(QPainter& painter, const QRegion& dirty);
    void drawArrows(QPainter& painter);
    /// 矢印（線・矢じり・順位の丸印）が掛かる範囲。描き直す範囲を矢印の分だけに絞るために使う
    QRect arrowsPaintRect(const QList<Arrow>& arrows) const;

private:
    /// マスを描き直す（座標が盤の外なら盤全体）
    void updateSquares(const QList<QPoint>& squares);

    ShogiView* m_view;

    // ハイライト/矢印データ
    std::array<QList<QPoint>, kLayerCount> m_layers;
    QList<Arrow> m_arrows;
};

#endif // SHOGIVIEWHIGHLIGHTING_H
