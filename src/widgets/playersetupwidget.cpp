/// @file playersetupwidget.cpp
/// @brief 対局開始ダイアログの対局者1人の欄の実装

#include "playersetupwidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>

#include "buttonstyles.h"
#include "engineselector.h"

namespace {
/// ページ（名前の欄・エンジン設定ボタン）の番号
constexpr int kHumanPage = 0;
constexpr int kEnginePage = 1;

/// 余白のない1行のページ
QWidget* makePage(QWidget* parent, const QString& name, QWidget* content)
{
    auto* page = new QWidget(parent);
    page->setObjectName(name);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    if (content) {
        layout->addWidget(content);
        if (!qobject_cast<QLineEdit*>(content)) layout->addStretch();
    }
    return page;
}
} // namespace

PlayerSetupWidget::PlayerSetupWidget(int side, const GameSettings::StartGame::Player& saved, QWidget* parent)
    : QWidget(parent)
    , m_saved(saved)
{
    const QString n = QString::number(side);
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);

    auto* typeLabel = new QLabel(tr("対局者"), this);
    typeLabel->setObjectName(QStringLiteral("labelPlayerType") + n);
    typeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto* combo = new QComboBox(this);
    combo->setObjectName(QStringLiteral("comboBoxPlayer") + n);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setMinimumWidth(200);
    typeLabel->setBuddy(combo);

    auto* nameLabel = new QLabel(tr("名前"), this);
    nameLabel->setObjectName(QStringLiteral("labelPlayer") + n);
    m_humanName = new QLineEdit(this);
    m_humanName->setObjectName(QStringLiteral("lineEditHumanName") + n);
    nameLabel->setBuddy(m_humanName);
    m_engineSettings = new QPushButton(tr("エンジン設定"), this);
    m_engineSettings->setObjectName(QStringLiteral("pushButtonEngineSettings") + n);
    m_engineSettings->setStyleSheet(ButtonStyles::secondaryNeutral());
    m_engineSettings->setAutoDefault(false);

    // 人間なら「名前」と入力欄、エンジンなら「エンジン設定」ボタンを出す（見出しの列は空ける）
    m_labelPages = new QStackedWidget(this);
    m_labelPages->setObjectName(QStringLiteral("stackedWidgetLabel") + n);
    m_labelPages->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_labelPages->addWidget(makePage(m_labelPages, QStringLiteral("pageLabelHuman") + n, nameLabel));
    m_labelPages->addWidget(makePage(m_labelPages, QStringLiteral("pageLabelEngine") + n, nullptr));
    m_inputPages = new QStackedWidget(this);
    m_inputPages->setObjectName(QStringLiteral("stackedWidgetPlayer") + n);
    m_inputPages->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_inputPages->addWidget(makePage(m_inputPages, QStringLiteral("pageHuman") + n, m_humanName));
    m_inputPages->addWidget(makePage(m_inputPages, QStringLiteral("pageEngine") + n, m_engineSettings));

    grid->addWidget(typeLabel, 0, 0);
    grid->addWidget(combo, 0, 1);
    grid->addWidget(m_labelPages, 1, 0);
    grid->addWidget(m_inputPages, 1, 1);

    // コンボの先頭（内蔵の項目）を「人間」にする。エンジンは名前で保存し、以前の版の番号・パスも読む
    EngineSelector::Options options;
    options.keys = {saved.engineName.key(), saved.engineNumber.key(), saved.enginePath.key()};
    options.builtinLabel = tr("人間");
    m_selector = new EngineSelector({combo, m_engineSettings, nullptr}, options, this);
    connect(m_selector, &EngineSelector::selectionChanged, this, &PlayerSetupWidget::updatePages);
    connect(m_selector, &EngineSelector::selectionChanged, this, &PlayerSetupWidget::kindChanged);
    connect(m_selector, &EngineSelector::settingsRequested, this, &PlayerSetupWidget::engineSettingsRequested);
    load();
}

bool PlayerSetupWidget::isEngine() const
{
    return !m_selector->isBuiltinSelected() && m_selector->selected().has_value();
}

PlayerChoice PlayerSetupWidget::choice() const
{
    PlayerChoice choice;
    choice.humanName = m_humanName->text();
    if (const std::optional<EngineListSettings::EngineEntry> engine = m_selector->selected()) {
        choice.kind = PlayerChoice::Kind::Engine;
        choice.engine = *engine;
    }
    return choice;
}

void PlayerSetupWidget::setChoice(const PlayerChoice& choice)
{
    m_humanName->setText(choice.humanName);
    if (choice.isEngine()) m_selector->select(choice.engine.name);
    else m_selector->selectBuiltin();
}

void PlayerSetupWidget::load()
{
    m_humanName->setText(m_saved.humanName.valueOr(tr("You")));
    const QList<EngineListSettings::EngineEntry>& engines = m_selector->engines();
    int index = -1;
    if (!m_saved.isHuman()) {
        const QString name = m_saved.engineName();
        if (!name.isEmpty()) {
            // 保存したエンジンが削除されていれば人間に戻す（別のエンジンに替えない）
            index = EngineListSettings::indexOfName(engines, name);
        } else {
            // 名前の無い以前の版は番号・パスで探す。何も保存していなければ先頭の登録エンジン
            index = EngineListSettings::restoreSelection({nullptr, m_saved.engineNumber.key(),
                                                          m_saved.enginePath.key()},
                                                         engines);
            if (index < 0 && !engines.isEmpty()) index = 0;
        }
    }
    if (index >= 0) m_selector->select(engines.at(index).name);
    else m_selector->selectBuiltin();
    updatePages();
}

void PlayerSetupWidget::save() const
{
    const bool human = !isEngine();
    m_saved.isHuman.set(human);
    m_saved.isEngine.set(!human);
    m_saved.humanName.set(m_humanName->text());
    if (!human) m_selector->saveSelection();
}

void PlayerSetupWidget::reset(bool engine)
{
    m_humanName->setText(tr("You"));
    if (engine && m_selector->hasEngines()) m_selector->select(m_selector->engines().first().name);
    else m_selector->selectBuiltin();
}

void PlayerSetupWidget::updatePages()
{
    const int page = isEngine() ? kEnginePage : kHumanPage;
    m_labelPages->setCurrentIndex(page);
    m_inputPages->setCurrentIndex(page);
}
