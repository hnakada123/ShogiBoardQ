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
        
        // 子ウィジェットが折りたたまれないようにする
        m_splitter->setChildrenCollapsible(false);
    }
    return m_splitter;
}
