/// @file docklayoutmanager.cpp
/// @brief ドックレイアウト管理クラスの実装

#include "docklayoutmanager.h"
#include "settingsservice.h"

#include <QMainWindow>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QAction>

#include "logcategories.h"

DockLayoutManager::DockLayoutManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_docks(static_cast<int>(DockType::Count), nullptr)
{
}

void DockLayoutManager::registerDock(DockType type, QDockWidget* dock)
{
    const int index = static_cast<int>(type);
    if (index >= 0 && index < m_docks.size()) {
        m_docks[index] = dock;
    }
}

void DockLayoutManager::setSavedLayoutsMenu(QMenu* menu)
{
    m_savedLayoutsMenu = menu;
}

QDockWidget* DockLayoutManager::dock(DockType type) const
{
    const int index = static_cast<int>(type);
    if (index >= 0 && index < m_docks.size()) {
        return m_docks[index];
    }
    return nullptr;
}

void DockLayoutManager::showAllDocks()
{
    for (QDockWidget* d : std::as_const(m_docks)) {
        if (d) {
            d->setVisible(true);
        }
    }
}

void DockLayoutManager::resetToDefault()
{
    if (!m_mainWindow) return;

    auto* menuDock = dock(DockType::Menu);
    auto* josekiDock = dock(DockType::Joseki);
    auto* recordDock = dock(DockType::Record);
    auto* gameInfoDock = dock(DockType::GameInfo);
    auto* thinkingDock = dock(DockType::Thinking);
    auto* considerationDock = dock(DockType::Consideration);
    auto* usiLogDock = dock(DockType::UsiLog);
    auto* csaLogDock = dock(DockType::CsaLog);
    auto* commentDock = dock(DockType::Comment);
    auto* branchTreeDock = dock(DockType::BranchTree);
    auto* evalChartDock = dock(DockType::EvalChart);

    // すべてのドックをフローティング解除
    QList<QDockWidget*> allDocks = {
        menuDock, josekiDock, recordDock, evalChartDock,
        gameInfoDock, usiLogDock, csaLogDock, commentDock,
        branchTreeDock, considerationDock, thinkingDock
    };
    for (QDockWidget* d : std::as_const(allDocks)) {
        if (d) {
            d->setFloating(false);
        }
    }

    // まず全てのドックをいったん削除
    for (QDockWidget* d : std::as_const(allDocks)) {
        if (d) m_mainWindow->removeDockWidget(d);
    }

    // メニューウィンドウ（デフォルトは非表示）
    if (menuDock) {
        m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, menuDock);
        menuDock->setVisible(false);
    }

    // 定跡ドック（デフォルトは非表示）
    if (josekiDock) {
        m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, josekiDock);
        josekiDock->setVisible(false);
    }

    // 上段右: 棋譜
    if (recordDock) {
        m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, recordDock);
        recordDock->setVisible(true);
    }

    // 下段: 全ドックをタブ化して配置
    // タブ順: 評価値グラフ, 対局情報, USI通信ログ, CSA通信ログ, 棋譜コメント, 分岐ツリー, 検討, 思考
    QList<QDockWidget*> bottomDocks = {
        evalChartDock, gameInfoDock, usiLogDock, csaLogDock,
        commentDock, branchTreeDock, considerationDock, thinkingDock
    };

    QDockWidget* prevDock = nullptr;
    for (QDockWidget* d : std::as_const(bottomDocks)) {
        if (!d) continue;
        if (!prevDock) {
            m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, d);
        } else {
            m_mainWindow->tabifyDockWidget(prevDock, d);
        }
        d->setVisible(true);
        prevDock = d;
    }

    // 思考タブをアクティブに
    if (thinkingDock) {
        thinkingDock->raise();
    }

    // ドックのサイズを調整
    if (recordDock) {
        m_mainWindow->resizeDocks({recordDock}, {350}, Qt::Horizontal);
    }
}

void DockLayoutManager::saveLayoutAs()
{
    if (!m_mainWindow) return;

    bool ok;
    QString name = QInputDialog::getText(m_mainWindow,
        tr("ドックレイアウトを保存"),
        tr("レイアウト名:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || name.trimmed().isEmpty()) {
        return;  // キャンセルまたは空の名前
    }

    name = name.trimmed();

    // 既存のレイアウトがあれば上書き確認
    QStringList existingNames = SettingsService::savedDockLayoutNames();
    if (existingNames.contains(name)) {
        QMessageBox::StandardButton reply = QMessageBox::question(m_mainWindow,
            tr("確認"),
            tr("「%1」は既に存在します。上書きしますか？").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    // 現在のドック状態を保存
    QByteArray state = m_mainWindow->saveState();
    SettingsService::saveDockLayout(name, state);

    // メニューを更新
    updateSavedLayoutsMenu();

    QMessageBox::information(m_mainWindow, tr("保存完了"),
        tr("レイアウト「%1」を保存しました。").arg(name));
}

void DockLayoutManager::restoreLayout(const QString& name)
{
    if (!m_mainWindow) return;

    QByteArray state = SettingsService::loadDockLayout(name);
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

    QMessageBox::StandardButton reply = QMessageBox::question(m_mainWindow,
        tr("確認"),
        tr("レイアウト「%1」を削除しますか？").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        SettingsService::deleteDockLayout(name);
        updateSavedLayoutsMenu();
    }
}

void DockLayoutManager::setAsStartupLayout(const QString& name)
{
    if (!m_mainWindow) return;

    SettingsService::setStartupDockLayoutName(name);
    updateSavedLayoutsMenu();
    QMessageBox::information(m_mainWindow, tr("設定完了"),
        tr("レイアウト「%1」を起動時のレイアウトに設定しました。").arg(name));
}

void DockLayoutManager::clearStartupLayout()
{
    if (!m_mainWindow) return;

    SettingsService::setStartupDockLayoutName(QString());
    updateSavedLayoutsMenu();
    QMessageBox::information(m_mainWindow, tr("設定完了"),
        tr("起動時のレイアウト設定をクリアしました。\n次回起動時はデフォルトレイアウトが使用されます。"));
}

void DockLayoutManager::restoreStartupLayoutIfSet()
{
    if (!m_mainWindow) return;

    QString startupLayoutName = SettingsService::startupDockLayoutName();
    if (!startupLayoutName.isEmpty()) {
        QByteArray state = SettingsService::loadDockLayout(startupLayoutName);
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

    QStringList names = SettingsService::savedDockLayoutNames();
    QString startupLayoutName = SettingsService::startupDockLayoutName();

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
            connect(restoreAction, &QAction::triggered, this, [this, name]() {
                restoreLayout(name);
            });

            // 起動時のレイアウトに設定
            QAction* setStartupAction = layoutMenu->addAction(tr("起動時のレイアウトに設定"));
            connect(setStartupAction, &QAction::triggered, this, [this, name]() {
                setAsStartupLayout(name);
            });

            layoutMenu->addSeparator();

            // 削除アクション
            QAction* deleteAction = layoutMenu->addAction(tr("削除"));
            connect(deleteAction, &QAction::triggered, this, [this, name]() {
                deleteLayout(name);
            });
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
                                        QAction* clearStartupLayout,
                                        QAction* lockDocks,
                                        QMenu* savedLayoutsMenu)
{
    if (resetLayout) {
        connect(resetLayout, &QAction::triggered, this, &DockLayoutManager::resetToDefault);
    }
    if (saveLayoutAs) {
        connect(saveLayoutAs, &QAction::triggered, this, &DockLayoutManager::saveLayoutAs);
    }
    if (clearStartupLayout) {
        connect(clearStartupLayout, &QAction::triggered, this, &DockLayoutManager::clearStartupLayout);
    }
    if (lockDocks) {
        lockDocks->setChecked(SettingsService::docksLocked());
        connect(lockDocks, &QAction::toggled, this, &DockLayoutManager::setDocksLocked);
        // 起動時適用
        setDocksLocked(lockDocks->isChecked());
    }
    if (savedLayoutsMenu) {
        setSavedLayoutsMenu(savedLayoutsMenu);
        updateSavedLayoutsMenu();
    }
}

void DockLayoutManager::setDocksLocked(bool locked)
{
    // 固定時: ドッキング禁止、フローティング禁止、移動禁止
    // 非固定時: 全て許可
    const Qt::DockWidgetAreas areas = locked ? Qt::NoDockWidgetArea : Qt::AllDockWidgetAreas;
    const QDockWidget::DockWidgetFeatures features = locked
        ? QDockWidget::DockWidgetClosable  // 固定時は閉じるのみ許可
        : (QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    for (QDockWidget* d : std::as_const(m_docks)) {
        if (d) {
            d->setAllowedAreas(areas);
            d->setFeatures(features);
        }
    }
}

void DockLayoutManager::saveDockStates()
{
    // 必要なドックのみ保存（従来MainWindowが行っていたもの）
    auto saveIf = [](QDockWidget* dock,
                     auto setFloating,
                     auto setVisible,
                     auto setGeometry) {
        if (!dock) return;
        setFloating(dock->isFloating());
        setVisible(dock->isVisible());
        setGeometry(dock->saveGeometry());
    };

    saveIf(dock(DockType::EvalChart),
           SettingsService::setEvalChartDockFloating,
           SettingsService::setEvalChartDockVisible,
           SettingsService::setEvalChartDockGeometry);

    saveIf(dock(DockType::Record),
           SettingsService::setRecordPaneDockFloating,
           SettingsService::setRecordPaneDockVisible,
           SettingsService::setRecordPaneDockGeometry);

    saveIf(dock(DockType::Menu),
           SettingsService::setMenuWindowDockFloating,
           SettingsService::setMenuWindowDockVisible,
           SettingsService::setMenuWindowDockGeometry);

    saveIf(dock(DockType::Joseki),
           SettingsService::setJosekiWindowDockFloating,
           SettingsService::setJosekiWindowDockVisible,
           SettingsService::setJosekiWindowDockGeometry);

    saveIf(dock(DockType::AnalysisResults),
           SettingsService::setKifuAnalysisResultsDockFloating,
           SettingsService::setKifuAnalysisResultsDockVisible,
           SettingsService::setKifuAnalysisResultsDockGeometry);
}
