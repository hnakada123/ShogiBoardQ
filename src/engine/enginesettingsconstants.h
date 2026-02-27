#ifndef ENGINESETTINGSCONSTANTS_H
#define ENGINESETTINGSCONSTANTS_H

/// @file enginesettingsconstants.h
/// @brief エンジン設定のINIファイルキー定数の定義


/**
 * @brief エンジン設定の永続化に使用するINIファイルのキー名定数
 *
 */
namespace EngineSettingsConstants {

// --- 設定ファイル ---

static constexpr auto SettingsFileName = "ShogiBoardQ.ini"; ///< アプリケーション設定ファイル名

// --- エンジン登録 ---

static constexpr auto EnginesGroupName = "Engines";    ///< エンジン一覧のINIグループ名
static constexpr char EngineNameKey[] = "name";        ///< エンジン名キー
static constexpr char EnginePathKey[] = "path";        ///< エンジンファイルパスキー
static constexpr char EngineAuthorKey[] = "author";    ///< エンジン作者キー

// --- オプション属性 ---

static constexpr char EngineOptionNameKey[] = "name";           ///< オプション名キー
static constexpr char EngineOptionTypeKey[] = "type";           ///< 型（check, spin等）
static constexpr char EngineOptionDefaultKey[] = "default";     ///< デフォルト値キー
static constexpr char EngineOptionMinKey[] = "min";             ///< spin型の最小値
static constexpr char EngineOptionMaxKey[] = "max";             ///< spin型の最大値
static constexpr char EngineOptionValueKey[] = "value";         ///< 現在値キー
static constexpr char EngineOptionValueListKey[] = "valueList"; ///< combo型の選択肢リスト

// --- USIオプションタイプ文字列 ---

static constexpr char OptionTypeSpin[] = "spin";         ///< USIオプション型: 整数値
static constexpr char OptionTypeCheck[] = "check";       ///< USIオプション型: 真偽値
static constexpr char OptionTypeCombo[] = "combo";       ///< USIオプション型: 選択肢
static constexpr char OptionTypeButton[] = "button";     ///< USIオプション型: ボタン
static constexpr char OptionTypeFilename[] = "filename"; ///< USIオプション型: ファイルパス
static constexpr char OptionTypeString[] = "string";     ///< USIオプション型: 文字列
}

#endif // ENGINESETTINGSCONSTANTS_H
