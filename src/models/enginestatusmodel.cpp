/// @file enginestatusmodel.cpp
/// @brief エンジン1台の情報欄と通信ログの行の実装

#include "enginestatusmodel.h"

EngineStatusModel::EngineStatusModel(QObject* parent)
    : QObject(parent)
{
}

void EngineStatusModel::setEngineName(const QString& engineName)
{
    if (m_engineName == engineName) return;
    m_engineName = engineName;
    emit engineNameChanged();
}

void EngineStatusModel::updateStatus(const EngineSearchStatus& status)
{
    EngineSearchStatus merged = m_status;
    merged.merge(status);
    setStatus(merged);
}

void EngineStatusModel::setStatus(const EngineSearchStatus& status)
{
    if (m_status == status) return;
    m_status = status;
    emit statusChanged();
}

void EngineStatusModel::setPredictedMove(const QString& move)
{
    if (m_predictedMove == move) return;
    m_predictedMove = move;
    emit statusChanged();
}

void EngineStatusModel::appendLogLine(const QString& line)
{
    emit logLineAppended(line);
}

void EngineStatusModel::clear()
{
    setEngineName(QString());
    clearStatus();
}

void EngineStatusModel::clearStatus()
{
    if (m_status == EngineSearchStatus{} && m_predictedMove.isEmpty()) return;
    m_status = {};
    m_predictedMove.clear();
    emit statusChanged();
}
