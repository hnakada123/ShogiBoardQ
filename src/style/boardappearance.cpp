/// @file boardappearance.cpp
/// @brief 対局画面の外観の持ち主の実装

#include "boardappearance.h"
#include "appearancesettings.h"
#include "piecestylecatalog.h"

BoardAppearance& BoardAppearance::instance()
{
    static BoardAppearance appearance;
    return appearance;
}

QString BoardAppearance::pieceStyle() const
{
    return AppearanceSettings::pieceStyle();
}

void BoardAppearance::setPieceStyle(const QString& style)
{
    if (!PieceStyleCatalog::contains(style) || style == pieceStyle()) return;
    AppearanceSettings::setPieceStyle(style);
    emit pieceStyleChanged();
}

BoardColors BoardAppearance::colors() const
{
    return AppearanceSettings::boardColors();
}

void BoardAppearance::setColors(const BoardColors& colors)
{
    const BoardColors normalized = colors.normalized();
    if (normalized == this->colors()) return;
    AppearanceSettings::setBoardColors(normalized);
    emit colorsChanged();
}

BoardVisuals BoardAppearance::visuals() const
{
    return AppearanceSettings::boardVisuals();
}

void BoardAppearance::setVisuals(const BoardVisuals& visuals)
{
    const auto normalized = visuals.normalized();
    if (normalized == this->visuals()) return;
    AppearanceSettings::setBoardVisuals(normalized);
    emit visualsChanged();
}

AppearanceState BoardAppearance::state() const
{
    return {pieceStyle(), colors(), visuals()};
}

void BoardAppearance::apply(const AppearanceState& state)
{
    setPieceStyle(state.pieceStyle);
    setColors(state.colors);
    setVisuals(state.visuals);
}
