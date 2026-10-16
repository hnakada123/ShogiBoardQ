#ifndef BOARDSTATICLAYER_H
#define BOARDSTATICLAYER_H

/// @file boardstaticlayer.h
/// @brief 盤面のうち局面によらない部分を画像にして貼るクラスの定義

#include "boardcolors.h"
#include "boardvisuals.h"

#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>

class QPainter;
class ShogiViewLayout;

/**
 * @brief 背景・盤の木肌と縁・影・罫線・星・駒台の木肌を描いた画像
 *
 * これらは局面によらず変わらないため、画像にしておいて貼る。
 * 影の角丸の塗りや罫線のアンチエイリアスは盤全体の大きさで計算されるので、
 * 駒を動かすたびに描き直すと全面の再描画が重くなる。
 * 寸法・配色・質感・物理画素に対する位置が変わったときだけ作り直す。
 * 盤を回転すると駒台の位置が入れ替わる。回転を戻したときや、拡大して縮小し戻したときに
 * 作り直さないよう、1つ前の条件の画像も残しておく。
 */
class BoardStaticLayer
{
public:
    /// 画像を貼る（条件が変わっていれば作り直してから貼る）
    void draw(QPainter& painter, QSize widgetSize, const ShogiViewLayout& layout, int files, int ranks,
              const BoardColors& colors, const BoardVisuals& visuals);
    /// 今の画像の識別子。作り直すと変わり、1つ前の画像に戻すと元の値に戻る
    qint64 cacheKey() const { return m_image.cacheKey(); }

private:
    /// 画像を作った条件
    struct Key {
        QSize size;
        qreal dpr = 0;
        QPointF phase;      // 物理画素に対する描画位置の端数
        QRectF surface;
        QRect blackStand;
        QRect whiteStand;
        QSize fieldSize;
        QPoint offset;
        QColor background;
        QColor board;
        QColor stand;
        QColor grid;
        bool woodGrain = false;
        bool standWoodGrain = false;
        bool operator==(const Key& other) const;
    };

    static void paint(QPainter& painter, const Key& key, const ShogiViewLayout& layout, int files, int ranks);

    QPixmap m_image;
    Key m_key;
    // 1つ前の条件で作った画像。盤の回転・拡大と縮小を行き来するときに作り直さず使い回す
    QPixmap m_spareImage;
    Key m_spareKey;
};

#endif // BOARDSTATICLAYER_H
