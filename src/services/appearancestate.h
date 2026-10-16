#ifndef APPEARANCESTATE_H
#define APPEARANCESTATE_H

/// @file appearancestate.h
/// @brief 対局画面の外観（駒のセット・配色・見た目）をまとめた値

#include "boardcolors.h"
#include "boardvisuals.h"
#include "piecestylecatalog.h"

#include <QString>

/**
 * @brief 外観の3つの設定（駒のセットは PieceImageProvider、配色と見た目は BoardAppearance）を1つの値にする
 *
 * 外観ダイアログは操作のたびに current() で読み、値を変えて apply() で戻す。
 * 比べるときも値どうしで比べる。
 */
struct AppearanceState {
    QString pieceStyle = PieceStyleCatalog::standardId();
    BoardColors colors;
    BoardVisuals visuals;

    /// 今の外観
    static AppearanceState current();
    /// すべての盤面に反映して保存する（変わった設定だけが変更を知らせる）
    void apply() const;

    friend bool operator==(const AppearanceState& a, const AppearanceState& b)
    {
        return a.pieceStyle == b.pieceStyle && a.colors == b.colors && a.visuals == b.visuals;
    }
    friend bool operator!=(const AppearanceState& a, const AppearanceState& b) { return !(a == b); }
};

#endif // APPEARANCESTATE_H
