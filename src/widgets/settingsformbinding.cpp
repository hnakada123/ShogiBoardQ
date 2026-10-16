/// @file settingsformbinding.cpp
/// @brief 入力欄と設定の宣言の対応の表の実装

#include "settingsformbinding.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>

void SettingsFormBinding::add(QSpinBox* box, const Setting<int>& setting)
{
    m_entries.push_back({[box, &setting] { box->setValue(setting()); },
                         [box, &setting] { setting.set(box->value()); },
                         [box, &setting] { box->setValue(setting.defaultValue()); }});
}

void SettingsFormBinding::add(QAbstractButton* button, const Setting<bool>& setting)
{
    m_entries.push_back({[button, &setting] { button->setChecked(setting()); },
                         [button, &setting] { setting.set(button->isChecked()); },
                         [button, &setting] { button->setChecked(setting.defaultValue()); }});
}

void SettingsFormBinding::add(QGroupBox* group, const Setting<bool>& setting)
{
    m_entries.push_back({[group, &setting] { group->setChecked(setting()); },
                         [group, &setting] { setting.set(group->isChecked()); },
                         [group, &setting] { group->setChecked(setting.defaultValue()); }});
}

void SettingsFormBinding::add(QComboBox* combo, const Setting<int>& setting)
{
    m_entries.push_back({[combo, &setting] { combo->setCurrentIndex(setting()); },
                         [combo, &setting] { setting.set(combo->currentIndex()); },
                         [combo, &setting] { combo->setCurrentIndex(setting.defaultValue()); }});
}

void SettingsFormBinding::add(QLineEdit* edit, const Setting<QString>& setting, std::function<QString()> defaultText)
{
    const auto fallback = [defaultText, &setting] { return defaultText ? defaultText() : setting.defaultValue(); };
    m_entries.push_back({[edit, &setting, fallback] { edit->setText(setting.valueOr(fallback())); },
                         [edit, &setting] { setting.set(edit->text()); },
                         [edit, fallback] { edit->setText(fallback()); }});
}

void SettingsFormBinding::load() const
{
    for (const Entry& entry : m_entries) entry.load();
}

void SettingsFormBinding::save() const
{
    for (const Entry& entry : m_entries) entry.save();
}

void SettingsFormBinding::reset() const
{
    for (const Entry& entry : m_entries) entry.reset();
}
