#ifndef TSUMEPOSITIONPREVIEW_H
#define TSUMEPOSITIONPREVIEW_H

/// @file tsumepositionpreview.h
/// @brief 詰将棋問題一覧の初期局面の小さな表示

#include <QWidget>

#include "boardgeometry.h"
#include "boardstaticlayer.h"
#include "shogiposition.h"

/**
 * @brief 一覧専用の軽量な初期局面表示（操作用の盤や対局コントローラは作らない）
 *
 * 局面は core の Position、配置は盤と同じ BoardGeometry、盤・駒台の木肌は盤と同じ BoardStaticLayer で描く
 * （配色・木目・駒台の木目の設定も盤と同じ）。攻方（手番の側）を手前にする。
 */
class TsumePositionPreview : public QWidget
{
    Q_OBJECT
public:
    explicit TsumePositionPreview(const QString& sfen, QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    static constexpr int kHeaderHeight = 22;
    static constexpr int kSquareSize = 22;
    void appearanceChanged();

    Position m_position;
    BoardGeometry m_geometry;
    BoardStaticLayer m_staticLayer;
};

#endif
