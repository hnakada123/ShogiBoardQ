/// @file appearancestate.cpp
/// @brief 対局画面の外観をまとめた値の読み書き

#include "appearancestate.h"
#include "boardappearance.h"
#include "pieceimageprovider.h"

AppearanceState AppearanceState::current()
{
    AppearanceState state;
    state.pieceStyle = PieceImageProvider::instance().style();
    state.colors = BoardAppearance::instance().colors();
    state.visuals = BoardAppearance::instance().visuals();
    return state;
}

void AppearanceState::apply() const
{
    PieceImageProvider::instance().setStyle(pieceStyle);
    BoardAppearance::instance().setColors(colors);
    BoardAppearance::instance().setVisuals(visuals);
}
