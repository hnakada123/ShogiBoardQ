/// @file engineselector.cpp
/// @brief 登録エンジンを選ぶ部品の実装

#include "engineselector.h"
#include "plaintooltip.h"

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
    QList<EngineEntry> engines;
    const RegisteredEngines all = RegisteredEngines::load();
    for (const EngineEntry& engine : all.entries()) {
        if (!m_options.filter || m_options.filter(engine)) engines.append(engine);
    }
    m_engines = RegisteredEngines(engines);
}

void EngineSelector::restoreSaved()
{
    loadList();
    // 保存した名前を優先し、無ければ以前の版の番号・パス。内蔵の項目は空の名前で保存している
    const std::optional<QString> saved = EngineSelectionSettings::savedSelectionName(m_options.keys);
    const bool builtin = !m_options.builtinLabel.isEmpty() && saved.has_value() && saved->isEmpty();
    QString name;
    if (!builtin) {
        const int index = EngineSelectionSettings::restoreSelection(m_options.keys, m_engines);
        if (index >= 0) name = m_engines.entries().at(index).name;
    }
    populate(name, builtin);
}

void EngineSelector::reload()
{
    const std::optional<EngineEntry> current = selected();
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
        for (const EngineEntry& engine : m_engines.entries()) {
            m_combo->addItem(engine.name, engine.path);
            // エンジン名は登録したエンジンが名乗る名前なので、文字のまま出す
            m_combo->setItemData(m_combo->count() - 1, plainToolTip(engine.name), Qt::ToolTipRole);
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

std::optional<EngineEntry> EngineSelector::selected() const
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
        EngineSelectionSettings::saveSelection(m_options.keys, QString());
    } else if (const std::optional<EngineEntry> engine = selected()) {
        EngineSelectionSettings::saveSelection(m_options.keys, engine->name);
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
    const std::optional<EngineEntry> engine = selected();
    if (!engine) {
        QMessageBox::critical(m_combo->window(), tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }
    emit settingsRequested(*engine);
}

void EngineSelector::updateState()
{
    m_combo->setEnabled(m_enabled && m_combo->count() > 0);
    m_combo->setToolTip(plainToolTip(m_combo->currentText()));
    if (m_settingsButton) m_settingsButton->setEnabled(m_enabled && selected().has_value());
    if (m_hint) m_hint->setVisible(m_engines.isEmpty());
}
