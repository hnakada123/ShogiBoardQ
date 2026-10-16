#ifndef ENGINESETTINGSCONSTANTS_H
#define ENGINESETTINGSCONSTANTS_H

/// @file enginesettingsconstants.h
/// @brief エンジン設定のINIファイルキー定数の定義

#include <QString>

/**
 * @brief エンジン設定の永続化に使用するINIファイルのキー名定数
 *
 * 保存形式を読み書きするのは EngineProfileStore だけ。オプションの型の文字列は USI の受信行の解釈でも使う。
 */
namespace EngineSettingsConstants {

// --- エンジン登録 ---

static constexpr auto EnginesGroupName = "Engines";    ///< エンジン一覧のINIグループ名
static constexpr char EngineNameKey[] = "name";        ///< エンジン名キー
static constexpr char EnginePathKey[] = "path";        ///< エンジンファイルパスキー
static constexpr char EngineAuthorKey[] = "author";    ///< エンジン作者キー

// --- エンジンごとの設定 ---

/// エンジンごとの設定（USIオプションの配列と GUI の設定）を置く親グループ
static constexpr char EngineOptionsGroupName[] = "EngineOptions";

/**
 * @brief エンジンごとの設定を置くグループ（"EngineOptions/<エスケープした名前>"）
 *
 * エンジンの id name をそのままグループ名にすると、ほかの設定のグループ（General など）と衝突し、
 * "/" を含む名前は入れ子のグループになる。"%"・"/"・"\" をエスケープして親グループの下に置く。
 */
inline QString engineOptionsGroup(const QString& engineName)
{
    QString escaped;
    escaped.reserve(engineName.size());
    for (const QChar ch : engineName) {
        if (ch == QLatin1Char('%')) escaped += QStringLiteral("%25");
        else if (ch == QLatin1Char('/')) escaped += QStringLiteral("%2F");
        else if (ch == QLatin1Char('\\')) escaped += QStringLiteral("%5C");
        else escaped += ch;
    }
    return QLatin1String(EngineOptionsGroupName) + QLatin1Char('/') + escaped;
}

// エンジンごとのグループ内に保存するGUI設定（USIオプション配列とは独立）
static constexpr char EngineGuiPonderEnabledKey[] = "gui/ponderEnabled";
static constexpr char EngineSendUnreportedPonderKey[] = "gui/sendUnreportedPonderOption";

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
