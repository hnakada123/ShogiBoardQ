/// @file docklayoutmanager.cpp
/// @brief ドックレイアウト管理クラスの実装

#include "appsettings.h"
#include "dialogfontscale.h"
#include "dialogutils.h"
#include "docklayoutmanager.h"
#include "docksettings.h"
#include "dockregistry.h"

#include <QMainWindow>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QAction>

#include "logcategories.h"

DockLayoutManager::DockLayoutManager(QMainWindow* mainWindow, DockRegistry* docks, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_docks(docks)
{
    Q_ASSERT(m_mainWindow && m_docks);
}

void DockLayoutManager::setSavedLayoutsMenu(QMenu* menu)
{
    m_savedLayoutsMenu = menu;
}

void DockLayoutManager::showAllDocks()
{
    const QList<QDockWidget*> docks = m_docks->docks();
    for (QDockWidget* d : docks) d->setVisible(true);
}

void DockLayoutManager::resetToDefault()
{
    m_docks->resetToDefault();
}

void DockLayoutManager::saveLayoutAs()
{
    if (!m_mainWindow) return;

    QInputDialog dialog(m_mainWindow);
    dialog.setWindowTitle(tr("ドックレイアウトを保存"));
    dialog.setLabelText(tr("レイアウト名:"));
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.ensurePolished();
    DialogFontScale::install(&dialog, AppSettings::dockLayoutNameDialogFontSize, {&AppSettings::dockLayoutNameDialogSize});
    const int result = dialog.exec();
    QString name = dialog.textValue();
    if (result != QDialog::Accepted || name.trimmed().isEmpty()) {
        return;  // キャンセルまたは空の名前
    }

    name = name.trimmed();

    // 既存のレイアウトがあれば上書き確認
    QStringList existingNames = DockSettings::savedDockLayoutNames();
    if (existingNames.contains(name)) {
        if (!DialogUtils::confirmAction(m_mainWindow, tr("確認"),
                                        tr("「%1」は既に存在します。上書きしますか？").arg(name),
                                        tr("上書きする"))) {
            return;
        }
    }

    // 現在のドック状態を保存
    QByteArray state = m_mainWindow->saveState();
    DockSettings::saveDockLayout(name, state);

    // メニューを更新
    updateSavedLayoutsMenu();

    QMessageBox::information(m_mainWindow, tr("保存完了"),
        tr("レイアウト「%1」を保存しました。").arg(name));
}

void DockLayoutManager::restoreLayout(const QString& name)
{
    if (!m_mainWindow) return;

    QByteArray state = DockSettings::loadDockLayout(name);
    if (state.isEmpty()) {
        QMessageBox::warning(m_mainWindow, tr("エラー"),
            tr("レイアウト「%1」が見つかりません。").arg(name));
        return;
    }

    // すべてのドックを表示状態にしてから復元
    showAllDocks();

    // 状態を復元
    m_mainWindow->restoreState(state);
}

void DockLayoutManager::deleteLayout(const QString& name)
{
    if (!m_mainWindow) return;

    if (DialogUtils::confirmAction(m_mainWindow, tr("確認"),
                                   tr("レイアウト「%1」を削除しますか？").arg(name), tr("削除する"))) {
        DockSettings::deleteDockLayout(name);
        updateSavedLayoutsMenu();
    }
}

void DockLayoutManager::setAsStartupLayout(const QString& name)
{
    if (!m_mainWindow) return;

    DockSettings::startupDockLayoutName.set(name);
    updateSavedLayoutsMenu();
    QMessageBox::information(m_mainWindow, tr("設定完了"),
        tr("レイアウト「%1」を起動時のレイアウトに設定しました。").arg(name));
}

void DockLayoutManager::clearStartupLayout()
{
    if (!m_mainWindow) return;

    DockSettings::startupDockLayoutName.set(QString());
    updateSavedLayoutsMenu();
    QMessageBox::information(m_mainWindow, tr("設定完了"),
        tr("起動時のレイアウト設定をクリアしました。\n次回起動時はデフォルトレイアウトが使用されます。"));
}

void DockLayoutManager::restoreStartupLayoutIfSet()
{
    if (!m_mainWindow) return;

    QString startupLayoutName = DockSettings::startupDockLayoutName();
    if (!startupLayoutName.isEmpty()) {
        QByteArray state = DockSettings::loadDockLayout(startupLayoutName);
        if (!state.isEmpty()) {
            // すべてのドックを表示状態にしてから復元
            showAllDocks();

            m_mainWindow->restoreState(state);
            qCInfo(lcUi) << "Restored startup layout:" << startupLayoutName;
        }
    }
}

void DockLayoutManager::updateSavedLayoutsMenu()
{
    if (!m_savedLayoutsMenu) return;

    m_savedLayoutsMenu->clear();

    QStringList names = DockSettings::savedDockLayoutNames();
    QString startupLayoutName = DockSettings::startupDockLayoutName();

    if (names.isEmpty()) {
        QAction* emptyAction = m_savedLayoutsMenu->addAction(tr("（保存済みレイアウトなし）"));
        emptyAction->setEnabled(false);
    } else {
        for (const QString& name : std::as_const(names)) {
            // レイアウト名（起動時レイアウトには★マーク）
            QString displayName = name;
            if (name == startupLayoutName) {
                displayName = QStringLiteral("★ ") + name;
            }

            // 復元用サブメニュー
            QMenu* layoutMenu = m_savedLayoutsMenu->addMenu(displayName);

            // 復元アクション
            QAction* restoreAction = layoutMenu->addAction(tr("復元"));
            restoreAction->setData(name);
            connect(restoreAction, &QAction::triggered, this, &DockLayoutManager::onRestoreLayoutTriggered);

            // 起動時のレイアウトに設定
            QAction* setStartupAction = layoutMenu->addAction(tr("起動時のレイアウトに設定"));
            setStartupAction->setData(name);
            connect(setStartupAction, &QAction::triggered, this, &DockLayoutManager::onSetStartupLayoutTriggered);

            layoutMenu->addSeparator();

            // 削除アクション
            QAction* deleteAction = layoutMenu->addAction(tr("削除"));
            deleteAction->setData(name);
            connect(deleteAction, &QAction::triggered, this, &DockLayoutManager::onDeleteLayoutTriggered);
        }
    }

    // 起動時レイアウト設定のクリア
    m_savedLayoutsMenu->addSeparator();
    QAction* clearStartupAction = m_savedLayoutsMenu->addAction(tr("起動時のレイアウトをクリア"));
    clearStartupAction->setEnabled(!startupLayoutName.isEmpty());
    connect(clearStartupAction, &QAction::triggered, this, &DockLayoutManager::clearStartupLayout);
}

void DockLayoutManager::wireMenuActions(QAction* resetLayout,
                                        QAction* saveLayoutAs,
                                        QAction* lockDocks,
                                        QMenu* savedLayoutsMenu)
{
    if (resetLayout) {
        connect(resetLayout, &QAction::triggered, this, &DockLayoutManager::resetToDefault);
    }
    if (saveLayoutAs) {
        connect(saveLayoutAs, &QAction::triggered, this, &DockLayoutManager::saveLayoutAs);
    }
    if (lockDocks) {
        lockDocks->setChecked(DockSettings::docksLocked());
        connect(lockDocks, &QAction::toggled, this, &DockLayoutManager::setDocksLocked);
        // 起動時適用
        setDocksLocked(lockDocks->isChecked());
    }
    if (savedLayoutsMenu) {
        savedLayoutsMenu->setProperty("automationMenu", true);
        setSavedLayoutsMenu(savedLayoutsMenu);
        updateSavedLayoutsMenu();
    }
}

void DockLayoutManager::setDocksLocked(bool locked)
{
    DockSettings::docksLocked.set(locked);

    // 固定時: ドッキング禁止、フローティング禁止、移動禁止
    // 非固定時: 全て許可
    const Qt::DockWidgetAreas areas = locked ? Qt::NoDockWidgetArea : Qt::AllDockWidgetAreas;
    const QDockWidget::DockWidgetFeatures features = locked
        ? QDockWidget::DockWidgetClosable  // 固定時は閉じるのみ許可
        : (QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    const QList<QDockWidget*> docks = m_docks->docks();
    for (QDockWidget* d : docks) {
        d->setAllowedAreas(areas);
        d->setFeatures(features);
    }
}

void DockLayoutManager::onRestoreLayoutTriggered()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action) {
        restoreLayout(action->data().toString());
    }
}

void DockLayoutManager::onSetStartupLayoutTriggered()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action) {
        setAsStartupLayout(action->data().toString());
    }
}

void DockLayoutManager::onDeleteLayoutTriggered()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action) {
        deleteLayout(action->data().toString());
    }
}

void DockLayoutManager::saveDockStates()
{
    m_docks->saveStates();
}
