#include "gamelayoutbuilder.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QWidget>
#include <QDebug>

GameLayoutBuilder::GameLayoutBuilder(const Deps& d, QObject* parent)
    : QObject(parent), m_d(d)
{}

QSplitter* GameLayoutBuilder::buildHorizontalSplit()
{
    if (!m_splitter) {
        m_splitter = new QSplitter(Qt::Horizontal);
        if (m_d.shogiView)           m_splitter->addWidget(m_d.shogiView);
        if (m_d.recordPaneOrWidget)  m_splitter->addWidget(m_d.recordPaneOrWidget);
    }
    return m_splitter;
}

void GameLayoutBuilder::attachToCentralLayout(QVBoxLayout* vbox)
{
    if (!vbox) return;
    // 既存中身クリアは MainWindow 側の責務のままでOK（clearLayout等）
    if (m_splitter) vbox->addWidget(m_splitter);
    if (m_d.analysisTabWidget) vbox->addWidget(m_d.analysisTabWidget);
}
