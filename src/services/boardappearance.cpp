#include "boardappearance.h"
#include "appsettings.h"

BoardAppearance& BoardAppearance::instance()
{
    static BoardAppearance appearance;
    return appearance;
}

BoardColors BoardAppearance::colors() const
{
    return AppSettings::boardColors();
}

void BoardAppearance::setColors(const BoardColors& colors)
{
    const BoardColors normalized = colors.normalized();
    if (normalized == this->colors()) return;
    AppSettings::setBoardColors(normalized);
    emit colorsChanged();
}
