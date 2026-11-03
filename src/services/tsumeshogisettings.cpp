/// @file tsumeshogisettings.cpp
/// @brief 詰将棋の設定のうち、宣言の表で表せない項目（以前の版の値の引き継ぎ）

#include "tsumeshogisettings.h"

namespace TsumeshogiSettings {

QString lastCollectionFile()
{
    const QString file = collectionPreferences().lastFile;
    if (!file.isEmpty()) return file;
    // 以前の版は、解く画面の設定に開いたファイルを書いていた
    const Setting<QString> legacy{"TsumePlay/file", QString()};
    return legacy();
}

void upgradeLegacyGeneratorTargetMoves()
{
    const BoundedSetting<int>& target = tsumeshogiGeneratorTargetMoves;
    if (!target.isSaved()) return;
    const int saved = target();
    const int odd = GeneratorLimits::oddTargetMoves(saved);
    if (odd != saved) target.set(odd);
}

} // namespace TsumeshogiSettings
