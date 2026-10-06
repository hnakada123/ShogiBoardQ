/// @file menuwindow.cpp
/// @brief メニューウィンドウクラスの実装

#include "menuwindow.h"
#include "buttonstyles.h"
#include "menubuttonwidget.h"
#include "appsettings.h"
#include "flowlayout.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <memory>
#include <utility>

MenuWindow::MenuWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setupUi();
    loadSettings();
}

MenuWindow::~MenuWindow()
{
    // 子 QAction の破棄通知が、破棄済みのメンバーへアクセスしないようにする。
    for (QAction* action : std::as_const(m_actionMap)) {
        if (action) disconnect(action, nullptr, this, nullptr);
    }
}

void MenuWindow::setupUi()
{
    setWindowTitle(tr("メニュー"));
    setAttribute(Qt::WA_DeleteOnClose, false);  // 閉じても破棄しない

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // タイトルバー風のヘッダー
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addStretch();

    // ボタンサイズ調整（縮小）
    m_buttonSizeDecreaseBtn = new QToolButton(this);
    m_buttonSizeDecreaseBtn->setText(QStringLiteral("\u25a1-"));  // □-
    m_buttonSizeDecreaseBtn->setToolTip(tr("Decrease button size"));
    m_buttonSizeDecreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    connect(m_buttonSizeDecreaseBtn, &QToolButton::clicked, this, &MenuWindow::onButtonSizeDecrease);
    headerLayout->addWidget(m_buttonSizeDecreaseBtn);

    // ボタンサイズ調整（拡大）
    m_buttonSizeIncreaseBtn = new QToolButton(this);
    m_buttonSizeIncreaseBtn->setText(QStringLiteral("\u25a1+"));  // □+
    m_buttonSizeIncreaseBtn->setToolTip(tr("Increase button size"));
    m_buttonSizeIncreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    connect(m_buttonSizeIncreaseBtn, &QToolButton::clicked, this, &MenuWindow::onButtonSizeIncrease);
    headerLayout->addWidget(m_buttonSizeIncreaseBtn);

    // セパレータ
    QFrame* separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    headerLayout->addWidget(separator1);

    // フォントサイズ調整（縮小）
    m_fontSizeDecreaseBtn = new QToolButton(this);
    m_fontSizeDecreaseBtn->setText(QStringLiteral("A-"));
    m_fontSizeDecreaseBtn->setToolTip(tr("Decrease font size"));
    m_fontSizeDecreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    connect(m_fontSizeDecreaseBtn, &QToolButton::clicked, this, &MenuWindow::onFontSizeDecrease);
    headerLayout->addWidget(m_fontSizeDecreaseBtn);

    // フォントサイズ調整（拡大）
    m_fontSizeIncreaseBtn = new QToolButton(this);
    m_fontSizeIncreaseBtn->setText(QStringLiteral("A+"));
    m_fontSizeIncreaseBtn->setToolTip(tr("Increase font size"));
    m_fontSizeIncreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    connect(m_fontSizeIncreaseBtn, &QToolButton::clicked, this, &MenuWindow::onFontSizeIncrease);
    headerLayout->addWidget(m_fontSizeIncreaseBtn);

    // セパレータ
    QFrame* separator2 = new QFrame(this);
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    headerLayout->addWidget(separator2);

    // お気に入り編集の用途を明示する
    m_customizeButton = new QToolButton(this);
    m_customizeButton->setText(tr("お気に入り編集"));
    m_customizeButton->setCheckable(true);
    m_customizeButton->setToolTip(tr("お気に入りの追加・削除・並べ替え"));
    m_customizeButton->setStyleSheet(ButtonStyles::menuMainButton());
    connect(m_customizeButton, &QToolButton::clicked, this, &MenuWindow::onCustomizeButtonClicked);
    headerLayout->addWidget(m_customizeButton);

    mainLayout->addLayout(headerLayout);

    m_customizeHint = new QLabel(tr("各項目の＋で追加、×で削除できます。お気に入りはドラッグで並べ替えできます。"), this);
    m_customizeHint->setWordWrap(true);
    m_customizeHint->setStyleSheet(QStringLiteral("color: #466783; padding: 4px;"));
    m_customizeHint->hide();
    mainLayout->addWidget(m_customizeHint);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("menuSearch"));
    m_searchEdit->setPlaceholderText(tr("このタブの項目を検索"));
    m_searchEdit->setAccessibleName(m_searchEdit->placeholderText());
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MenuWindow::updateFilter);
    mainLayout->addWidget(m_searchEdit);

    // タブウィジェット
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName(QStringLiteral("menuTabs"));
    m_customizeButton->setObjectName(QStringLiteral("menuCustomize"));
    m_buttonSizeDecreaseBtn->setObjectName(QStringLiteral("menuButtonSizeDecrease"));
    m_buttonSizeIncreaseBtn->setObjectName(QStringLiteral("menuButtonSizeIncrease"));
    m_fontSizeDecreaseBtn->setObjectName(QStringLiteral("menuFontSizeDecrease"));
    m_fontSizeIncreaseBtn->setObjectName(QStringLiteral("menuFontSizeIncrease"));
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MenuWindow::onTabChanged);
    mainLayout->addWidget(m_tabWidget);

    // ウィンドウサイズ設定
    resize(AppSettings::menuWindowSize());
}

void MenuWindow::setCategories(const QList<CategoryInfo>& categories)
{
    const QSignalBlocker blocker(m_tabWidget);
    for (QAction* action : std::as_const(m_actionMap)) {
        if (!action) continue;
        disconnect(action, &QAction::changed, this, &MenuWindow::updateFilter);
        disconnect(action, &QObject::destroyed, this, &MenuWindow::updateFilter);
    }
    m_actionMap.clear();
    m_allButtons.clear();
    m_emptyLabels.clear();

    // タブをクリア
    while (m_tabWidget->count() > 0) {
        std::unique_ptr<QWidget> w(m_tabWidget->widget(0));
        m_tabWidget->removeTab(0);
    }

    // アクションマップを構築
    for (const auto& category : categories) {
        for (QAction* action : std::as_const(category.actions)) {
            if (action && !action->objectName().isEmpty()) {
                m_actionMap[action->objectName()] = action;
            }
        }
    }

    for (QAction* action : std::as_const(m_actionMap)) {
        connect(action, &QAction::changed, this, &MenuWindow::updateFilter);
        connect(action, &QObject::destroyed, this, &MenuWindow::updateFilter);
    }

    // お気に入りタブを最初に追加
    m_favoritesScrollArea = createCategoryTab({QString(), {}}, true);
    m_tabWidget->addTab(m_favoritesScrollArea, QStringLiteral("\u2605 ") + tr("お気に入り"));

    // カテゴリタブを追加
    for (const auto& category : categories) {
        QScrollArea* scrollArea = createCategoryTab(category);
        QString title = category.displayName;
        title.remove(QRegularExpression(QStringLiteral("\\s*\\([A-Za-z]\\)")));
        m_tabWidget->addTab(scrollArea, title);
        m_tabWidget->setTabToolTip(m_tabWidget->count() - 1, title);
    }

    // お気に入りタブを更新
    updateFavoritesTab();

    // 保存されたサイズ設定を適用
    updateAllButtonSizes();
    m_tabWidget->setCurrentIndex(qBound(0, m_savedTabIndex, m_tabWidget->count() - 1));
    updateFilter();
}

void MenuWindow::setFavorites(const QStringList& favoriteActionNames)
{
    m_favoriteActionNames = favoriteActionNames;
    updateFavoritesTab();
    updateCustomizeModeForAllTabs();
    saveSettings();
    Q_EMIT favoritesChanged(m_favoriteActionNames);
}

QScrollArea* MenuWindow::createCategoryTab(const CategoryInfo& category, bool isFavoriteTab)
{
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* page = new QWidget;
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    auto* emptyLabel = new QLabel(page);
    emptyLabel->setObjectName(QStringLiteral("menuEmptyMessage"));
    emptyLabel->setWordWrap(true);
    // スタイルシートを持つラベルは setContentsMargins が上書きされるため、余白も padding で指定する
    emptyLabel->setStyleSheet(QStringLiteral("QLabel { color: #596b7b; padding: 16px 12px; }"));
    pageLayout->addWidget(emptyLabel);

    auto* container = new QWidget(page);
    auto* layout = new FlowLayout(container, 8, 8, 8);
    pageLayout->addWidget(container);
    pageLayout->addStretch();
    m_emptyLabels.insert(container, emptyLabel);
    if (isFavoriteTab) {
        m_favoritesContainer = container;
        m_favoritesLayout = layout;
    }
    populateButtons(layout, category.actions, isFavoriteTab);
    scrollArea->setWidget(page);
    return scrollArea;
}

void MenuWindow::updateFavoritesTab()
{
    if (!m_favoritesLayout) return;

    // 既存のウィジェットを削除
    while (auto* rawItem = m_favoritesLayout->takeAt(0)) {
        std::unique_ptr<QLayoutItem> layoutItem(rawItem);
        if (auto* widget = layoutItem->widget()) {
            auto* btn = qobject_cast<MenuButtonWidget*>(widget);
            if (btn) {
                m_allButtons.removeAll(btn);
            }
            // クリック・ドロップのシグナル送信元はイベント処理が終わるまで生存させる。
            widget->hide();
            widget->deleteLater();
        }
    }

    // お気に入りアクションを追加
    QList<QAction*> favoriteActions;
    for (const QString& actionName : std::as_const(m_favoriteActionNames)) {
        QAction* action = findActionByName(actionName);
        if (action) {
            favoriteActions.append(action);
        }
    }

    populateButtons(m_favoritesLayout, favoriteActions, true);
    updateFilter();
}

void MenuWindow::updateCustomizeModeForAllTabs()
{
    for (MenuButtonWidget* btn : std::as_const(m_allButtons)) {
        if (!btn) continue;

        // このボタンがお気に入りタブに属しているかどうかを判定
        bool isFavoriteTab = false;
        QWidget* parent = btn->parentWidget();
        while (parent) {
            if (parent == m_favoritesContainer) {
                isFavoriteTab = true;
                break;
            }
            parent = parent->parentWidget();
        }

        bool isInFavorites = m_favoriteActionNames.contains(btn->actionName());
        btn->setCustomizeMode(m_customizeMode, isFavoriteTab, isInFavorites);
    }
}

void MenuWindow::populateButtons(FlowLayout* layout, const QList<QAction*>& actions, bool isFavoriteTab)
{
    for (QAction* action : actions) {
        if (!action || action->objectName().isEmpty() || action->isSeparator()) continue;

        auto* button = new MenuButtonWidget(action, layout->parentWidget());
        button->updateSizes(m_buttonSize, m_fontSize, m_iconSize);
        button->setCustomizeMode(m_customizeMode, isFavoriteTab, m_favoriteActionNames.contains(action->objectName()));
        connect(button, &MenuButtonWidget::actionTriggered, this, &MenuWindow::onActionTriggered);
        connect(button, &MenuButtonWidget::addToFavorites, this, &MenuWindow::onAddToFavorites);
        connect(button, &MenuButtonWidget::removeFromFavorites, this, &MenuWindow::onRemoveFromFavorites);
        connect(button, &MenuButtonWidget::dropReceived, this, &MenuWindow::onFavoriteReordered);
        layout->addWidget(button);
        m_allButtons.append(button);
    }
    equalizeButtonHeights();
}

void MenuWindow::equalizeButtonHeights()
{
    // タブ（ボタンを並べる領域）ごとに、最も行数の多いボタンの高さへ揃える
    QHash<QWidget*, int> heights;
    for (MenuButtonWidget* button : std::as_const(m_allButtons)) {
        if (!button) continue;
        int& height = heights[button->parentWidget()];
        height = qMax(height, button->naturalHeight());
    }
    for (MenuButtonWidget* button : std::as_const(m_allButtons)) {
        if (button) button->setRowHeight(heights.value(button->parentWidget()));
    }
}

void MenuWindow::updateFilter()
{
    const QString query = m_searchEdit->text().trimmed();
    QMap<QWidget*, int> visibleCounts;
    for (MenuButtonWidget* button : std::as_const(m_allButtons)) {
        const QAction* action = button->action();
        const bool visible = action && action->isVisible()
            && (query.isEmpty() || action->text().contains(query, Qt::CaseInsensitive)
                || action->toolTip().contains(query, Qt::CaseInsensitive));
        button->setVisible(visible);
        if (visible) ++visibleCounts[button->parentWidget()];
    }
    for (auto it = m_emptyLabels.cbegin(); it != m_emptyLabels.cend(); ++it) {
        it.value()->setText(it.key() == m_favoritesContainer && query.isEmpty()
            ? tr("お気に入りはまだありません。「お気に入り編集」を押して、各タブの項目を追加してください。")
            : tr("該当する項目がありません。"));
        it.value()->setVisible(visibleCounts.value(it.key()) == 0);
        it.key()->layout()->invalidate();
    }
}

QAction* MenuWindow::findActionByName(const QString& actionName) const
{
    return m_actionMap.value(actionName, nullptr);
}

void MenuWindow::onCustomizeButtonClicked()
{
    m_customizeMode = m_customizeButton->isChecked();
    m_customizeButton->setText(m_customizeMode ? tr("編集完了") : tr("お気に入り編集"));
    m_customizeHint->setVisible(m_customizeMode);
    updateCustomizeModeForAllTabs();
}

void MenuWindow::onActionTriggered(QAction* action)
{
    if (action && action->isEnabled() && action->isVisible()) {
        action->trigger();
        Q_EMIT actionTriggered(action);
    }
}

void MenuWindow::onAddToFavorites(const QString& actionName)
{
    if (!m_favoriteActionNames.contains(actionName)) {
        m_favoriteActionNames.append(actionName);
        updateFavoritesTab();
        updateCustomizeModeForAllTabs();
        AppSettings::setMenuWindowFavorites(m_favoriteActionNames);
        Q_EMIT favoritesChanged(m_favoriteActionNames);
    }
}

void MenuWindow::onRemoveFromFavorites(const QString& actionName)
{
    if (m_favoriteActionNames.removeAll(actionName) > 0) {
        updateFavoritesTab();
        updateCustomizeModeForAllTabs();
        AppSettings::setMenuWindowFavorites(m_favoriteActionNames);
        Q_EMIT favoritesChanged(m_favoriteActionNames);
    }
}

void MenuWindow::onFavoriteReordered(const QString& sourceActionName, const QString& targetActionName)
{
    qsizetype sourceIndex = m_favoriteActionNames.indexOf(sourceActionName);
    qsizetype targetIndex = m_favoriteActionNames.indexOf(targetActionName);

    if (sourceIndex >= 0 && targetIndex >= 0 && sourceIndex != targetIndex) {
        m_favoriteActionNames.move(sourceIndex, targetIndex);
        updateFavoritesTab();
        AppSettings::setMenuWindowFavorites(m_favoriteActionNames);
        Q_EMIT favoritesChanged(m_favoriteActionNames);
    }
}

void MenuWindow::onTabChanged(int index)
{
    m_savedTabIndex = index;
    AppSettings::setMenuWindowCurrentTab(index);
}

void MenuWindow::onButtonSizeIncrease()
{
    if (m_buttonSize < kMaxButtonSize) {
        m_buttonSize = qMin(kMaxButtonSize, m_buttonSize + 8);
        m_iconSize = m_buttonSize / 3;
        updateAllButtonSizes();
    }
}

void MenuWindow::onButtonSizeDecrease()
{
    if (m_buttonSize > kMinButtonSize) {
        m_buttonSize = qMax(kMinButtonSize, m_buttonSize - 8);
        m_iconSize = m_buttonSize / 3;
        updateAllButtonSizes();
    }
}

void MenuWindow::onFontSizeIncrease()
{
    if (m_fontSize < kMaxFontSize) {
        m_fontSize += 1;
        updateAllButtonSizes();
    }
}

void MenuWindow::onFontSizeDecrease()
{
    if (m_fontSize > kMinFontSize) {
        m_fontSize -= 1;
        updateAllButtonSizes();
    }
}

void MenuWindow::updateAllButtonSizes()
{
    for (MenuButtonWidget* btn : std::as_const(m_allButtons)) {
        if (btn) {
            btn->updateSizes(m_buttonSize, m_fontSize, m_iconSize);
        }
    }
    equalizeButtonHeights();
    m_buttonSizeDecreaseBtn->setEnabled(m_buttonSize > kMinButtonSize);
    m_buttonSizeIncreaseBtn->setEnabled(m_buttonSize < kMaxButtonSize);
    m_fontSizeDecreaseBtn->setEnabled(m_fontSize > kMinFontSize);
    m_fontSizeIncreaseBtn->setEnabled(m_fontSize < kMaxFontSize);
    AppSettings::setMenuWindowButtonSize(m_buttonSize);
    AppSettings::setMenuWindowFontSize(m_fontSize);
}

void MenuWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    event->accept();
}

void MenuWindow::loadSettings()
{
    m_favoriteActionNames = AppSettings::menuWindowFavorites();
    m_buttonSize = qBound(kMinButtonSize, AppSettings::menuWindowButtonSize(), kMaxButtonSize);
    m_fontSize = qBound(kMinFontSize, AppSettings::menuWindowFontSize(), kMaxFontSize);
    m_savedTabIndex = AppSettings::menuWindowCurrentTab();
    m_iconSize = m_buttonSize / 3;
    resize(AppSettings::menuWindowSize());
}

void MenuWindow::saveSettings()
{
    AppSettings::setMenuWindowFavorites(m_favoriteActionNames);
    AppSettings::setMenuWindowButtonSize(m_buttonSize);
    AppSettings::setMenuWindowFontSize(m_fontSize);
    AppSettings::setMenuWindowSize(size());
}
