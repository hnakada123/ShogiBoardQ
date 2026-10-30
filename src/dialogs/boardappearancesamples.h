#ifndef BOARDAPPEARANCESAMPLES_H
#define BOARDAPPEARANCESAMPLES_H

/// @file boardappearancesamples.h
/// @brief 外観ダイアログの見本の描画（駒・部品・配色・色の見本と、見本を並べる一覧）

#include <QHash>
#include <QIcon>
#include <QStyledItemDelegate>
#include <functional>

#include "boardappearancecatalog.h"
#include "boardcolors.h"
#include "boardvisuals.h"

class QListWidget;

/**
 * @brief 外観ダイアログ（BoardAppearanceDialog）に並べる見本の画像
 *
 * どれも渡された値だけから描き、外観を変えない。見本の文字の翻訳の文脈は "BoardAppearanceDialog"。
 */
namespace BoardAppearanceSamples {

/// 見本を並べる一覧（アイコンを格子に並べる。選んでも見本の色を変えない）
QListWidget* createGallery(QWidget* parent, const QString& objectName);
/// 見本を描く前の空の画像（項目の大きさを揃えるため全項目で共有する）
QIcon placeholder();
/// 駒のセット style の見本（王・歩・龍）
QIcon pieceSample(const QString& style);
/// 部品 component の見本 sample（将棋盤・駒台には駒のセット style の駒を置く）
QIcon componentSample(BoardAppearanceCatalog::Component component, const BoardAppearanceSample& sample,
                      const QString& style);
/// 盤・駒台・背景の配色 colors の見本（駒のセット style の駒を置く。配色のコンボに出す）
QIcon paletteSample(const BoardColors& colors, const QString& style, const BoardVisuals& visuals = BoardVisuals{});
/// 1色の見本（透明にできる色は市松模様の上に塗る。frame は枠の色）
QIcon colorSwatch(const QColor& color, const QColor& frame);

/**
 * @brief 一覧で画面に見えた項目の見本だけを描いて覚えておく
 *
 * 開くたびに全項目を描くと、駒のセットや部品の見本を増やすほどウィンドウを開くのが遅くなり、
 * メモリも増えるため。
 */
class LazySampleDelegate : public QStyledItemDelegate
{
public:
    using Render = std::function<QIcon(const QModelIndex&)>;

    LazySampleDelegate(Render render, QObject* parent);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// 描いた見本を捨てて、次に見えたときに描き直させる
    void clear() { m_samples.clear(); }

private:
    Render m_render;
    mutable QHash<int, QIcon> m_samples;
};

} // namespace BoardAppearanceSamples

#endif // BOARDAPPEARANCESAMPLES_H
