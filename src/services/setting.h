#ifndef SETTING_H
#define SETTING_H

/// @file setting.h
/// @brief 設定ファイルの1項目（キーと既定値）を1行で宣言する型

#include <QByteArray>
#include <QList>
#include <QSettings>
#include <QString>
#include <QVariant>

#include <functional>
#include <initializer_list>
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
 * キーは実行時に組み立ててもよい（対局者ごとの項目など。宣言はキーの写しを持つ）。
 */
template <typename T>
class Setting
{
public:
    Setting(QByteArray key, T defaultValue)
        : m_key(std::move(key)), m_default(std::move(defaultValue))
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

    /// 値を保存しているか
    bool isSaved() const { return SettingsCommon::openSettings().contains(QLatin1String(m_key)); }

    /// 保存した値を消す（以後は既定値を返す）
    void remove() const { SettingsCommon::openSettings().remove(QLatin1String(m_key)); }

    const char* key() const { return m_key.constData(); }
    const T& defaultValue() const { return m_default; }

private:
    QByteArray m_key;
    T m_default;
};

/**
 * @brief 範囲付きの設定（読みも書きも qBound で範囲に収める）
 * @code
 * inline const BoundedSetting<int> pieceSoundVolume{"PieceSound/volume", 30, 0, 100};
 * @endcode
 */
template <typename T>
class BoundedSetting : public Setting<T>
{
public:
    BoundedSetting(QByteArray key, T defaultValue, T minimum, T maximum)
        : Setting<T>(std::move(key), std::move(defaultValue)), m_min(std::move(minimum)), m_max(std::move(maximum))
    {
    }

    T value() const { return qBound(m_min, Setting<T>::value(), m_max); }
    T operator()() const { return value(); }
    void set(const T& v) const { Setting<T>::set(qBound(m_min, v, m_max)); }

    const T& minimum() const { return m_min; }
    const T& maximum() const { return m_max; }

private:
    T m_min;
    T m_max;
};

/**
 * @brief 構造体1つを複数のキーに保存する設定の、項目1つ（キーとメンバの対応）
 */
template <typename T>
struct SettingField {
    QByteArray key;
    std::function<QVariant(const T&)> read;
    std::function<void(T&, const QVariant&)> write;

    /// メンバ member をキー key に保存する項目（キーは実行時に組み立ててもよい）
    template <typename M>
    static SettingField of(QByteArray key, M T::* member)
    {
        return {std::move(key), [member](const T& object) { return QVariant::fromValue(object.*member); },
                [member](T& object, const QVariant& value) { object.*member = value.template value<M>(); }};
    }
};

/**
 * @brief 構造体1つを、項目ごとのキーに保存する設定
 *
 * 既定値は構造体の既定値（T{}）。保存していない項目は既定値のまま。normalize を渡すと、
 * 読んだ値と書く値をそれで整える（範囲に収めるなど）。
 * @code
 * inline const StructSetting<PieceSoundTone> pieceSoundTone{
 *     {SettingField<PieceSoundTone>::of("PieceSound/pitch", &PieceSoundTone::pitchSemitones), ...},
 *     &PieceSoundTone::clamped};
 * @endcode
 * グループ名だけ違う組（対局者ごとの項目など）は、グループ名からこの値を作る関数を1つ書き、
 * 組ごとにその関数で宣言する（JosekiSettings::playOptions1・playOptions2）。
 */
template <typename T>
class StructSetting
{
public:
    using Normalize = std::function<T(const T&)>;

    StructSetting(std::initializer_list<SettingField<T>> fields, Normalize normalize = Normalize())
        : m_fields(fields), m_normalize(std::move(normalize))
    {
    }

    T value() const
    {
        const QSettings& s = SettingsCommon::openSettings();
        T object{};
        for (const SettingField<T>& field : m_fields) {
            const QString key = QLatin1String(field.key);
            if (s.contains(key)) field.write(object, s.value(key));
        }
        return m_normalize ? m_normalize(object) : object;
    }
    T operator()() const { return value(); }

    void set(const T& object) const
    {
        QSettings& s = SettingsCommon::openSettings();
        const T normalized = m_normalize ? m_normalize(object) : object;
        for (const SettingField<T>& field : m_fields) s.setValue(QLatin1String(field.key), field.read(normalized));
    }

    /// 項目 key を保存しているか
    static bool isSaved(const char* key) { return SettingsCommon::openSettings().contains(QLatin1String(key)); }

private:
    QList<SettingField<T>> m_fields;
    Normalize m_normalize;
};

/**
 * @brief 値の並びを配列（`<配列のキー>/size` と `<配列のキー>/<番号>/<項目のキー>`）で保存する設定
 *
 * 配列のキーは実行時に決めてもよい（表の組ごとの列幅など）。
 */
template <typename T>
class ListSetting
{
public:
    ListSetting(QString arrayKey, const char* itemKey) : m_arrayKey(std::move(arrayKey)), m_itemKey(itemKey) {}

    QList<T> value() const
    {
        QSettings& s = SettingsCommon::openSettings();
        QList<T> values;
        const int size = s.beginReadArray(m_arrayKey);
        for (int i = 0; i < size; ++i) {
            s.setArrayIndex(i);
            values.append(s.value(QLatin1String(m_itemKey)).template value<T>());
        }
        s.endArray();
        return values;
    }
    QList<T> operator()() const { return value(); }

    void set(const QList<T>& values) const
    {
        QSettings& s = SettingsCommon::openSettings();
        s.beginWriteArray(m_arrayKey, static_cast<int>(values.size()));
        for (qsizetype i = 0; i < values.size(); ++i) {
            s.setArrayIndex(static_cast<int>(i));
            s.setValue(QLatin1String(m_itemKey), QVariant::fromValue(values.at(i)));
        }
        s.endArray();
    }

private:
    QString m_arrayKey;
    const char* m_itemKey;
};

/**
 * @brief 構造体の並びを配列で保存する設定（各要素の項目は SettingField で対応させる）
 */
template <typename T>
class StructListSetting
{
public:
    StructListSetting(const char* arrayKey, std::initializer_list<SettingField<T>> fields)
        : m_arrayKey(arrayKey), m_fields(fields)
    {
    }

    QList<T> value() const
    {
        QSettings& s = SettingsCommon::openSettings();
        QList<T> values;
        const int size = s.beginReadArray(QLatin1String(m_arrayKey));
        for (int i = 0; i < size; ++i) {
            s.setArrayIndex(i);
            T object{};
            for (const SettingField<T>& field : m_fields) {
                const QString key = QLatin1String(field.key);
                if (s.contains(key)) field.write(object, s.value(key));
            }
            values.append(object);
        }
        s.endArray();
        return values;
    }
    QList<T> operator()() const { return value(); }

    void set(const QList<T>& values) const
    {
        QSettings& s = SettingsCommon::openSettings();
        s.beginWriteArray(QLatin1String(m_arrayKey), static_cast<int>(values.size()));
        for (qsizetype i = 0; i < values.size(); ++i) {
            s.setArrayIndex(static_cast<int>(i));
            for (const SettingField<T>& field : m_fields) s.setValue(QLatin1String(field.key), field.read(values.at(i)));
        }
        s.endArray();
    }

private:
    const char* m_arrayKey;
    QList<SettingField<T>> m_fields;
};

#endif // SETTING_H
