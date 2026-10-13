#ifndef SETTING_H
#define SETTING_H

/// @file setting.h
/// @brief 設定ファイルの1項目（キーと既定値）を1行で宣言する型

#include <QSettings>
#include <QVariant>

#include <utility>

#include "settingscommon.h"

/**
 * @brief 設定ファイル（ShogiBoardQ.ini）の1項目
 *
 * キーと既定値を宣言の表に1行で書き、読み書きはこの型を通す。
 * getter・setter の組を手で書かない。
 * @code
 * namespace GameSettings {
 * inline const Setting<int> kifuPaneFontSize{"FontSize/kifuPane", 10};
 * }
 * const int size = GameSettings::kifuPaneFontSize();   // 読む（未保存なら既定値）
 * GameSettings::kifuPaneFontSize.set(12);              // 書く
 * @endcode
 *
 * 値の範囲を絞る・形式を変換するなど処理が要る項目は、この型を使った関数として書く。
 */
template <typename T>
class Setting
{
public:
    Setting(const char* key, T defaultValue)
        : m_key(key), m_default(std::move(defaultValue))
    {
    }

    /// 保存した値（未保存なら既定値）
    T value() const { return valueOr(m_default); }
    T operator()() const { return value(); }

    /// 保存した値（未保存なら fallback）。既定値が翻訳文や実行時の場所で決まる項目に使う
    T valueOr(const T& fallback) const
    {
        return SettingsCommon::openSettings().value(QLatin1String(m_key), QVariant::fromValue(fallback)).template value<T>();
    }

    /// 値を保存する
    void set(const T& v) const { SettingsCommon::openSettings().setValue(QLatin1String(m_key), QVariant::fromValue(v)); }

    /// 保存した値を消す（以後は既定値を返す）
    void remove() const { SettingsCommon::openSettings().remove(QLatin1String(m_key)); }

    const char* key() const { return m_key; }
    const T& defaultValue() const { return m_default; }

private:
    const char* m_key;
    T m_default;
};

#endif // SETTING_H
