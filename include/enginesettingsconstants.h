#ifndef ENGINESETTINGSCONSTANTS_H
#define ENGINESETTINGSCONSTANTS_H

namespace EngineSettingsConstants {
// 設定ファイル
static constexpr auto SettingsFileName = "ShogiBoardQ.ini";

// エンジングループ名
static constexpr auto EnginesGroupName = "Engines";

// エンジン名を指定するキー
static constexpr char EngineNameKey[] = "name";

// エンジンの実行ファイルパスを指定するキー
static constexpr char EnginePathKey[] = "path";

// オプション名を指定するキー
static constexpr char EngineOptionNameKey[] = "name";

// オプションの型（例：check, spin）を指定するキー
static constexpr char EngineOptionTypeKey[] = "type";

// オプションのデフォルト値を指定するキー
static constexpr char EngineOptionDefaultKey[] = "default";

// オプションの最小値（spin型の場合）を指定するキー
static constexpr char EngineOptionMinKey[] = "min";

// オプションの最大値（spin型の場合）を指定するキー
static constexpr char EngineOptionMaxKey[] = "max";

// オプションの現在値を指定するキー
static constexpr char EngineOptionValueKey[] = "value";

// combo型のオプションで選択可能な値のリストを指定するキー
static constexpr char EngineOptionValueListKey[] = "valueList";
}

#endif // ENGINESETTINGSCONSTANTS_H
