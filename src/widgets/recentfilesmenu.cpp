/// @file recentfilesmenu.cpp
/// @brief 「履歴」ボタンのメニューの実装

#include "recentfilesmenu.h"

#include <QAction>
#include <QFileInfo>
#include <QMenu>

RecentFilesMenu::RecentFilesMenu(QMenu* menu)
    : QObject(menu)
    , m_menu(menu)
{
    setFiles({});
}

void RecentFilesMenu::setFiles(const QStringList& files)
{
    m_menu->clear();
    if (files.isEmpty()) {
        m_menu->addAction(tr("（履歴なし）"))->setEnabled(false);
        return;
    }
    for (const QString& filePath : files) {
        QAction* action = m_menu->addAction(QFileInfo(filePath).fileName());
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &RecentFilesMenu::onFileActionTriggered);
    }
    m_menu->addSeparator();
    connect(m_menu->addAction(tr("履歴をクリア")), &QAction::triggered, this, &RecentFilesMenu::clearTriggered);
}

void RecentFilesMenu::onFileActionTriggered()
{
    const auto* action = qobject_cast<QAction*>(sender());
    if (action) emit fileTriggered(action->data().toString());
}
