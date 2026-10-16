/// @file usicommlogmodel.cpp
/// @brief エンジン1台の情報欄と通信ログの行の実装

#include "usicommlogmodel.h"

UsiCommLogModel::UsiCommLogModel(QObject* parent)
    : QObject(parent)
{
}

void UsiCommLogModel::setEngineName(const QString& engineName)
{
    if (m_engineName == engineName) return;
    m_engineName = engineName;
    emit engineNameChanged();
}

void UsiCommLogModel::updateStatus(const EngineSearchStatus& status)
{
    EngineSearchStatus merged = m_status;
    merged.merge(status);
    setStatus(merged);
}

void UsiCommLogModel::setStatus(const EngineSearchStatus& status)
{
    if (m_status == status) return;
    m_status = status;
    emit statusChanged();
}

void UsiCommLogModel::setPredictedMove(const QString& move)
{
    if (m_predictedMove == move) return;
    m_predictedMove = move;
    emit statusChanged();
}

void UsiCommLogModel::appendLogLine(const QString& line)
{
    emit logLineAppended(line);
}

void UsiCommLogModel::clear()
{
    setEngineName(QString());
    if (m_status == EngineSearchStatus{} && m_predictedMove.isEmpty()) return;
    m_status = {};
    m_predictedMove.clear();
    emit statusChanged();
}
