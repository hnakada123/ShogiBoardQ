#ifndef SETTINGSFORMBINDING_H
#define SETTINGSFORMBINDING_H

/// @file settingsformbinding.h
/// @brief 入力欄と設定の宣言（Setting<T>）の対応の表（読み込み・保存・既定値に戻すを1か所で）

#include <QString>
#include <functional>
#include <vector>

#include "setting.h"

class QAbstractButton;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QSpinBox;

/**
 * @brief ダイアログの入力欄と設定の対応
 *
 * 入力欄ごとに add() で保存先を1度だけ書けば、load()・save()・reset() がすべての欄を同じ表で扱う。
 * 読み込み・保存・既定値に戻すで同じ項目を3回並べなくてよい。
 */
class SettingsFormBinding
{
public:
    void add(QSpinBox* box, const Setting<int>& setting);
    /// チェックボックス・ラジオボタン（checked）
    void add(QAbstractButton* button, const Setting<bool>& setting);
    /// チェックできるグループ（checked）
    void add(QGroupBox* group, const Setting<bool>& setting);
    /// コンボの選んだ位置
    void add(QComboBox* combo, const Setting<int>& setting);
    /// 文字の欄。defaultText があれば、未保存のときと既定値に戻すときにその値を使う
    void add(QLineEdit* edit, const Setting<QString>& setting, std::function<QString()> defaultText = {});

    /// 保存した値を入力欄に入れる
    void load() const;
    /// 入力欄の値を保存する
    void save() const;
    /// 入力欄を既定値に戻す（保存はしない）
    void reset() const;

private:
    struct Entry {
        std::function<void()> load;
        std::function<void()> save;
        std::function<void()> reset;
    };
    std::vector<Entry> m_entries;
};

#endif // SETTINGSFORMBINDING_H
