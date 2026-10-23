/// @file engineselector.cpp
/// @brief 登録エンジンを選ぶ部品の実装

#include "engineselector.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>

EngineSelector::EngineSelector(const Widgets& widgets, const Options& options, QObject* parent)
    : QObject(parent)
    , m_combo(widgets.combo)
    , m_settingsButton(widgets.settingsButton)
    , m_hint(widgets.hint)
    , m_options(options)
{
    Q_ASSERT(m_combo);
    restoreSaved();
    connect(m_combo, &QComboBox::currentIndexChanged, this, &EngineSelector::onCurrentIndexChanged);
    if (m_settingsButton)
        connect(m_settingsButton, &QAbstractButton::clicked, this, &EngineSelector::onSettingsClicked);
}

void EngineSelector::loadList()
{
    QList<Engine> engines;
    const RegisteredEngines all = RegisteredEngines::load();
    for (const Engine& engine : all.entries()) {
        if (!m_options.filter || m_options.filter(engine)) engines.append(engine);
    }
    m_engines = RegisteredEngines(engines);
}

void EngineSelector::restoreSaved()
{
    loadList();
    // 保存した名前を優先し、無ければ以前の版の番号・パス。内蔵の項目は空の名前で保存している
    const std::optional<QString> saved = EngineListSettings::savedSelectionName(m_options.keys);
    const bool builtin = !m_options.builtinLabel.isEmpty() && saved.has_value() && saved->isEmpty();
    QString name;
    if (!builtin) {
        const int index = EngineListSettings::restoreSelection(m_options.keys, m_engines);
        if (index >= 0) name = m_engines.entries().at(index).name;
    }
    populate(name, builtin);
}

void EngineSelector::reload()
{
    const std::optional<Engine> current = selected();
    const bool builtin = isBuiltinSelected();
    loadList();
    populate(current ? current->name : QString(), builtin);
    emit selectionChanged();
}

void EngineSelector::populate(const QString& selectName, bool builtin)
{
    {
        const QSignalBlocker blocker(m_combo);
        m_combo->clear();
        if (!m_options.builtinLabel.isEmpty()) m_combo->addItem(m_options.builtinLabel);
        for (const Engine& engine : m_engines.entries()) {
            m_combo->addItem(engine.name, engine.path);
            m_combo->setItemData(m_combo->count() - 1, engine.name, Qt::ToolTipRole);
        }
        const int offset = m_options.builtinLabel.isEmpty() ? 0 : 1;
        const int index = m_engines.indexOfName(selectName);
        // 選んでいなければ先頭の登録エンジン（無ければ内蔵の項目）
        if (builtin) m_combo->setCurrentIndex(0);
        else if (index >= 0) m_combo->setCurrentIndex(index + offset);
        else m_combo->setCurrentIndex(m_engines.isEmpty() ? (offset ? 0 : -1) : offset);
    }
    updateState();
}

int EngineSelector::engineIndexAt(int comboIndex) const
{
    const int index = comboIndex - (m_options.builtinLabel.isEmpty() ? 0 : 1);
    return index >= 0 && index < m_engines.entries().size() ? index : -1;
}

std::optional<EngineSelector::Engine> EngineSelector::selected() const
{
    const int index = engineIndexAt(m_combo->currentIndex());
    if (index < 0) return std::nullopt;
    return m_engines.entries().at(index);
}

bool EngineSelector::isBuiltinSelected() const
{
    return !m_options.builtinLabel.isEmpty() && m_combo->currentIndex() == 0;
}

void EngineSelector::select(const QString& name)
{
    const int index = m_engines.indexOfName(name);
    if (index >= 0) m_combo->setCurrentIndex(index + (m_options.builtinLabel.isEmpty() ? 0 : 1));
}

void EngineSelector::selectBuiltin()
{
    if (!m_options.builtinLabel.isEmpty()) m_combo->setCurrentIndex(0);
}

void EngineSelector::saveSelection() const
{
    if (!m_options.keys.name) return;
    if (isBuiltinSelected()) {
        EngineListSettings::saveSelection(m_options.keys, QString());
    } else if (const std::optional<Engine> engine = selected()) {
        EngineListSettings::saveSelection(m_options.keys, engine->name);
    }
}

void EngineSelector::setEnabled(bool enabled)
{
    m_enabled = enabled;
    updateState();
}

void EngineSelector::onCurrentIndexChanged()
{
    updateState();
    emit selectionChanged();
}

void EngineSelector::onSettingsClicked()
{
    const std::optional<Engine> engine = selected();
    if (!engine) {
        QMessageBox::critical(m_combo->window(), tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }
    emit settingsRequested(*engine);
}

void EngineSelector::updateState()
{
    m_combo->setEnabled(m_enabled && m_combo->count() > 0);
    m_combo->setToolTip(m_combo->currentText());
    if (m_settingsButton) m_settingsButton->setEnabled(m_enabled && selected().has_value());
    if (m_hint) m_hint->setVisible(m_engines.isEmpty());
}
