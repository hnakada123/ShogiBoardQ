#ifndef BOARDAPPEARANCE_H
#define BOARDAPPEARANCE_H

/// @file boardappearance.h
/// @brief 対局画面の外観（駒のセット・配色・見た目）の持ち主

#include "appearancestate.h"
#include "boardcolors.h"
#include "boardvisuals.h"
#include <QObject>
#include <QString>

/**
 * @brief すべての盤面が共有する外観の正本
 *
 * 駒のセット・配色・見た目（木目・駒の影・駒の大きさ）を設定（AppearanceSettings）に保存し、
 * 変わったものだけを知らせる。外観ダイアログは state() で読み、値を変えて apply() で戻す。
 * 駒の画像は PieceImageProvider が今の駒のセットから引く。
 */
class BoardAppearance : public QObject
{
    Q_OBJECT
public:
    static BoardAppearance& instance();

    /// 駒のセット（PieceStyleCatalog の ID）
    QString pieceStyle() const;
    /// 知らない ID は無視する
    void setPieceStyle(const QString& style);
    BoardColors colors() const;
    void setColors(const BoardColors& colors);
    BoardVisuals visuals() const;
    void setVisuals(const BoardVisuals& visuals);

    /// 今の外観
    AppearanceState state() const;
    /// 外観をまとめて変える（変わったものだけが変更を知らせる）
    void apply(const AppearanceState& state);

signals:
    void pieceStyleChanged();
    void colorsChanged();
    void visualsChanged();

private:
    BoardAppearance() = default;
};

#endif // BOARDAPPEARANCE_H
