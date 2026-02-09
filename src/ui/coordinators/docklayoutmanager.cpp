/// @file docklayoutmanager.cpp
/// @brief ドックレイアウト管理クラスの実装

#include "docklayoutmanager.h"
#include "settingsservice.h"

#include <QMainWindow>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QAction>

#include "loggingcategory.h"

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

    // すべてのドックをフローティング解除して表示
    if (menuDock) {
        menuDock->setFloating(false);
        menuDock->setVisible(true);
    }
    if (josekiDock) {
        josekiDock->setFloating(false);
        josekiDock->setVisible(false);  // デフォルトは非表示
    }
    if (recordDock) {
        recordDock->setFloating(false);
        recordDock->setVisible(true);
    }

    // 解析ドック群をリセット
    QList<QDockWidget*> analysisDocks = {
        gameInfoDock, thinkingDock, considerationDock, usiLogDock,
        csaLogDock, commentDock, branchTreeDock
    };
    for (QDockWidget* d : std::as_const(analysisDocks)) {
        if (d) {
            d->setFloating(false);
            d->setVisible(true);
        }
    }
    if (evalChartDock) {
        evalChartDock->setFloating(false);
        evalChartDock->setVisible(true);
    }

    // まず全てのドックをいったん削除
    if (menuDock) m_mainWindow->removeDockWidget(menuDock);
    if (josekiDock) m_mainWindow->removeDockWidget(josekiDock);
    if (recordDock) m_mainWindow->removeDockWidget(recordDock);
    for (QDockWidget* d : std::as_const(analysisDocks)) {
        if (d) m_mainWindow->removeDockWidget(d);
    }
    if (evalChartDock) m_mainWindow->removeDockWidget(evalChartDock);

    // 上段左: メニューウィンドウ
    if (menuDock) {
        m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, menuDock);
        menuDock->setVisible(true);
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

    // 下段: 解析ドック群（タブ化して配置）
    if (gameInfoDock) {
        m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, gameInfoDock);
        gameInfoDock->setVisible(true);
    }
    // 残りのドックをタブ化
    if (thinkingDock) {
        if (gameInfoDock) {
            m_mainWindow->tabifyDockWidget(gameInfoDock, thinkingDock);
        } else {
            m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, thinkingDock);
        }
        thinkingDock->setVisible(true);
    }
    if (considerationDock && thinkingDock) {
        m_mainWindow->tabifyDockWidget(thinkingDock, considerationDock);
        considerationDock->setVisible(true);
    }
    if (usiLogDock && considerationDock) {
        m_mainWindow->tabifyDockWidget(considerationDock, usiLogDock);
        usiLogDock->setVisible(true);
    }
    if (csaLogDock && usiLogDock) {
        m_mainWindow->tabifyDockWidget(usiLogDock, csaLogDock);
        csaLogDock->setVisible(true);
    }
    if (commentDock && csaLogDock) {
        m_mainWindow->tabifyDockWidget(csaLogDock, commentDock);
        commentDock->setVisible(true);
    }
    if (branchTreeDock && commentDock) {
        m_mainWindow->tabifyDockWidget(commentDock, branchTreeDock);
        branchTreeDock->setVisible(true);
    }

    // 対局情報ドックをアクティブに
    if (gameInfoDock) {
        gameInfoDock->raise();
    } else if (thinkingDock) {
        thinkingDock->raise();
    }

    // 下段右: 評価値グラフ（解析の右に分割配置）
    QDockWidget* leftmostDock = gameInfoDock ? gameInfoDock : thinkingDock;
    if (evalChartDock && leftmostDock) {
        m_mainWindow->splitDockWidget(leftmostDock, evalChartDock, Qt::Horizontal);
        evalChartDock->setVisible(true);
    }

    // ドックのサイズを調整（おおよその比率）
    m_mainWindow->resizeDocks({menuDock, recordDock}, {250, 350}, Qt::Horizontal);
    if (thinkingDock && evalChartDock) {
        m_mainWindow->resizeDocks({thinkingDock, evalChartDock}, {500, 400}, Qt::Horizontal);
    }

    emit layoutChanged();
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

    emit layoutChanged();
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

    emit layoutChanged();
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
