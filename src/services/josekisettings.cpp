/// @file josekisettings.cpp
/// @brief 定跡の設定のうち、宣言の表で表せない項目（以前の版の値の引き継ぎ）

#include "josekisettings.h"

namespace JosekiSettings {

ListSetting<int> josekiWindowColumnWidths()
{
    const ListSetting<int> widths{QStringLiteral("JosekiWindow/columnWidths"), "width"};
    // 比率の列を足す前の保存（列が1つ少ない）は、比率より前の列の幅だけを使う
    if (QList<int> saved = widths(); saved.size() == kJosekiWindowColumnCount - 1) {
        saved.resize(kJosekiWindowRatioColumn);
        widths.set(saved);
    }
    return widths;
}

} // namespace JosekiSettings
