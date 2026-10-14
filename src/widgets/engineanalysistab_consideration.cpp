/// @file engineanalysistab_consideration.cpp
/// @brief EngineAnalysisTab の ConsiderationTabManager 委譲メソッド

#include "engineanalysistab.h"
#include "considerationtabmanager.h"

#include <QTabWidget>
#include <QTableView>

// ===================== ConsiderationTabManager 初期化・委譲 =====================

void EngineAnalysisTab::ensureConsiderationTabManager()
{
    if (!m_considerationTabManager) {
        m_considerationTabManager = new ConsiderationTabManager(this);
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationMultiPVChanged,
                this, &EngineAnalysisTab::considerationMultiPVChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::stopConsiderationRequested,
                this, &EngineAnalysisTab::stopConsiderationRequested);
        connect(m_considerationTabManager, &ConsiderationTabManager::startConsiderationRequested,
                this, &EngineAnalysisTab::startConsiderationRequested);
        connect(m_considerationTabManager, &ConsiderationTabManager::engineSettingsRequested,
                this, &EngineAnalysisTab::engineSettingsRequested);
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationTimeSettingsChanged,
                this, &EngineAnalysisTab::considerationTimeSettingsChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::showArrowsChanged,
                this, &EngineAnalysisTab::showArrowsChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationEngineChanged,
                this, &EngineAnalysisTab::considerationEngineChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::pvRowClicked,
                this, &EngineAnalysisTab::pvRowClicked);
    }
}

ConsiderationTabManager* EngineAnalysisTab::considerationTabManager()
{
    ensureConsiderationTabManager();
    return m_considerationTabManager;
}

void EngineAnalysisTab::switchToConsiderationTab()
{
    if (m_tab && m_considerationTabIndex >= 0) {
        m_tab->setCurrentIndex(m_considerationTabIndex);
    }
}

void EngineAnalysisTab::switchToThinkingTab()
{
    if (m_tab) {
        m_tab->setCurrentIndex(0);
    }
}

void EngineAnalysisTab::setConsiderationStartEnabled(bool enabled)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setStartConsiderationEnabled(enabled);
}

