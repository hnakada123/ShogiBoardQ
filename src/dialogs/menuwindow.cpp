/// @file menuwindow.cpp
/// @brief メニューウィンドウクラスの実装

#include "menuwindow.h"
#include "buttonstyles.h"
#include "menubuttonwidget.h"
#include "appsettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QFrame>
#include <QDebug>
#include <memory>

MenuWindow::MenuWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setupUi();
    loadSettings();
}

void MenuWindow::setupUi()
{
    setWindowTitle(tr("メニュー"));
    setAttribute(Qt::WA_DeleteOnClose, false);  // 閉じても破棄しない

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

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

    // カスタマイズボタン（歯車アイコン）
    m_customizeButton = new QToolButton(this);
    m_customizeButton->setText(QStringLiteral("\u2699"));  // 歯車記号
    m_customizeButton->setCheckable(true);
    m_customizeButton->setToolTip(tr("Customize"));
    m_customizeButton->setStyleSheet(ButtonStyles::customizeSettings());
    connect(m_customizeButton, &QToolButton::clicked, this, &MenuWindow::onCustomizeButtonClicked);
    headerLayout->addWidget(m_customizeButton);

    mainLayout->addLayout(headerLayout);

    // タブウィジェット
    m_tabWidget = new QTabWidget(this);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MenuWindow::onTabChanged);
    mainLayout->addWidget(m_tabWidget);

    // ウィンドウサイズ設定
    resize(AppSettings::menuWindowSize());
}

void MenuWindow::setCategories(const QList<CategoryInfo>& categories)
{
    m_categories = categories;
    m_actionMap.clear();
    m_allButtons.clear();

    // タブをクリア
    while (m_tabWidget->count() > 0) {
        std::unique_ptr<QWidget> w(m_tabWidget->widget(0));
        m_tabWidget->removeTab(0);
    }

    // アクションマップを構築
    for (const auto& category : categories) {
        for (QAction* action : category.actions) {
            if (action && !action->objectName().isEmpty()) {
                m_actionMap[action->objectName()] = action;
            }
        }
    }

    // お気に入りタブを最初に追加
    m_favoritesScrollArea = new QScrollArea(this);
    m_favoritesScrollArea->setWidgetResizable(true);
    m_favoritesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_favoritesContainer = new QWidget();
    m_favoritesLayout = new QGridLayout(m_favoritesContainer);
    m_favoritesLayout->setContentsMargins(8, 8, 8, 8);
    m_favoritesLayout->setSpacing(8);
    m_favoritesLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_favoritesScrollArea->setWidget(m_favoritesContainer);
    m_tabWidget->addTab(m_favoritesScrollArea, QStringLiteral("\u2605 ") + tr("Favorites"));

    // カテゴリタブを追加
    for (const auto& category : categories) {
        QScrollArea* scrollArea = createCategoryTab(category);
        m_tabWidget->addTab(scrollArea, category.displayName);
    }

    // お気に入りタブを更新
    updateFavoritesTab();

    // 保存されたサイズ設定を適用
    updateAllButtonSizes();
}

void MenuWindow::setFavorites(const QStringList& favoriteActionNames)
{
    m_favoriteActionNames = favoriteActionNames;
    updateFavoritesTab();
    updateCustomizeModeForAllTabs();
}

QScrollArea* MenuWindow::createCategoryTab(const CategoryInfo& category)
{
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* container = new QWidget();
    QGridLayout* layout = new QGridLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    populateGrid(layout, category.actions, false);

    scrollArea->setWidget(container);
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
            std::unique_ptr<QWidget> widgetGuard(widget);
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

    populateGrid(m_favoritesLayout, favoriteActions, true);
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

void MenuWindow::populateGrid(QGridLayout* layout, const QList<QAction*>& actions, bool isFavoriteTab)
{
    int row = 0;
    int col = 0;

    for (QAction* action : actions) {
        if (!action || action->objectName().isEmpty()) continue;
        if (action->isSeparator()) continue;

        MenuButtonWidget* button = new MenuButtonWidget(action, layout->parentWidget());
        button->updateSizes(m_buttonSize, m_fontSize, m_iconSize);  // 保存されたサイズを適用
        bool isInFavorites = m_favoriteActionNames.contains(action->objectName());
        button->setCustomizeMode(m_customizeMode, isFavoriteTab, isInFavorites);

        connect(button, &MenuButtonWidget::actionTriggered, this, &MenuWindow::onActionTriggered);
        connect(button, &MenuButtonWidget::addToFavorites, this, &MenuWindow::onAddToFavorites);
        connect(button, &MenuButtonWidget::removeFromFavorites, this, &MenuWindow::onRemoveFromFavorites);
        connect(button, &MenuButtonWidget::dropReceived, this, &MenuWindow::onFavoriteReordered);

        layout->addWidget(button, row, col);
        m_allButtons.append(button);

        col++;
        if (col >= kColumnsPerRow) {
            col = 0;
            row++;
        }
    }

    // 残りのセルにストレッチを追加
    layout->setRowStretch(row + 1, 1);
    layout->setColumnStretch(kColumnsPerRow, 1);
}

QAction* MenuWindow::findActionByName(const QString& actionName) const
{
    return m_actionMap.value(actionName, nullptr);
}

void MenuWindow::onCustomizeButtonClicked()
{
    m_customizeMode = m_customizeButton->isChecked();
    updateCustomizeModeForAllTabs();
}

void MenuWindow::onActionTriggered(QAction* action)
{
    if (action) {
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
        Q_EMIT favoritesChanged(m_favoriteActionNames);
    }
}

void MenuWindow::onRemoveFromFavorites(const QString& actionName)
{
    if (m_favoriteActionNames.removeAll(actionName) > 0) {
        updateFavoritesTab();
        updateCustomizeModeForAllTabs();
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
        Q_EMIT favoritesChanged(m_favoriteActionNames);
    }
}

void MenuWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    // タブ変更時の処理（必要に応じて実装）
}

void MenuWindow::onButtonSizeIncrease()
{
    if (m_buttonSize < kMaxButtonSize) {
        m_buttonSize += 8;
        m_iconSize = m_buttonSize / 3;
        updateAllButtonSizes();
    }
}

void MenuWindow::onButtonSizeDecrease()
{
    if (m_buttonSize > kMinButtonSize) {
        m_buttonSize -= 8;
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
}

void MenuWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    event->accept();
}

void MenuWindow::loadSettings()
{
    m_favoriteActionNames = AppSettings::menuWindowFavorites();
    m_buttonSize = AppSettings::menuWindowButtonSize();
    m_fontSize = AppSettings::menuWindowFontSize();
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
