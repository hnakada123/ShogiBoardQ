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

EngineInfoWidget* EngineAnalysisTab::considerationInfo() const
{
    if (!m_considerationTabManager) return nullptr;
    return m_considerationTabManager->considerationInfo();
}

QTableView* EngineAnalysisTab::considerationView() const
{
    if (!m_considerationTabManager) return nullptr;
    return m_considerationTabManager->considerationView();
}

void EngineAnalysisTab::setConsiderationThinkingModel(ShogiEngineThinkingModel* m)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationThinkingModel(m);
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

int EngineAnalysisTab::considerationMultiPV() const
{
    if (!m_considerationTabManager) return 1;
    return m_considerationTabManager->considerationMultiPV();
}

void EngineAnalysisTab::setConsiderationMultiPV(int value)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationMultiPV(value);
}

void EngineAnalysisTab::setConsiderationTimeLimit(bool unlimited, int byoyomiSecVal)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationTimeLimit(unlimited, byoyomiSecVal);
}

void EngineAnalysisTab::setConsiderationEngineName(const QString& name)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationEngineName(name);
}

void EngineAnalysisTab::startElapsedTimer()
{
    ensureConsiderationTabManager();
    m_considerationTabManager->startElapsedTimer();
}

void EngineAnalysisTab::stopElapsedTimer()
{
    if (m_considerationTabManager) {
        m_considerationTabManager->stopElapsedTimer();
    }
}

void EngineAnalysisTab::resetElapsedTimer()
{
    if (m_considerationTabManager) {
        m_considerationTabManager->resetElapsedTimer();
    }
}

void EngineAnalysisTab::setConsiderationRunning(bool running)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationRunning(running);
}

void EngineAnalysisTab::loadEngineList()
{
    ensureConsiderationTabManager();
    m_considerationTabManager->loadEngineList();
}

void EngineAnalysisTab::loadConsiderationTabSettings()
{
    ensureConsiderationTabManager();
    m_considerationTabManager->loadConsiderationTabSettings();
}

void EngineAnalysisTab::saveConsiderationTabSettings()
{
    if (m_considerationTabManager) {
        m_considerationTabManager->saveConsiderationTabSettings();
    }
}

int EngineAnalysisTab::selectedEngineIndex() const
{
    if (!m_considerationTabManager) return 0;
    return m_considerationTabManager->selectedEngineIndex();
}

QString EngineAnalysisTab::selectedEngineName() const
{
    if (!m_considerationTabManager) return QString();
    return m_considerationTabManager->selectedEngineName();
}

bool EngineAnalysisTab::isUnlimitedTime() const
{
    if (!m_considerationTabManager) return true;
    return m_considerationTabManager->isUnlimitedTime();
}

int EngineAnalysisTab::byoyomiSec() const
{
    if (!m_considerationTabManager) return 20;
    return m_considerationTabManager->byoyomiSec();
}

bool EngineAnalysisTab::isShowArrowsChecked() const
{
    if (!m_considerationTabManager) return true;
    return m_considerationTabManager->isShowArrowsChecked();
}
