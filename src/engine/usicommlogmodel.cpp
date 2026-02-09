/// @file usicommlogmodel.cpp
/// @brief USI通信ログ・エンジン情報表示モデルの実装

#include "usicommlogmodel.h"
#include "usi.h"

// ============================================================
// 初期化
// ============================================================

UsiCommLogModel::UsiCommLogModel(QObject* parent) : QObject(parent) {}

// ============================================================
// プロパティ getter
// ============================================================

QString UsiCommLogModel::engineName() const
{
    return m_engineName;
}

QString UsiCommLogModel::predictiveMove() const
{
    return m_predictiveMove;
}

QString UsiCommLogModel::searchedMove() const
{
    return m_searchedMove;
}

QString UsiCommLogModel::searchDepth() const
{
    return m_searchDepth;
}

QString UsiCommLogModel::nodeCount() const
{
    return m_nodeCount;
}

QString UsiCommLogModel::nodesPerSecond() const
{
    return m_nodesPerSecond;
}

QString UsiCommLogModel::hashUsage() const
{
    return m_hashUsage;
}

QString UsiCommLogModel::usiCommLog() const
{
    return m_usiCommLog;
}

// ============================================================
// プロパティ setter
// ============================================================

void UsiCommLogModel::setEngineName(const QString& engineName)
{
    qCDebug(lcEngine) << "setEngineName: old=" << m_engineName << "new=" << engineName;
    if (m_engineName != engineName)
    {
        m_engineName = engineName;
        emit engineNameChanged();
    }
}

void UsiCommLogModel::setPredictiveMove(const QString& predictiveMove)
{
    if (m_predictiveMove != predictiveMove)
    {
        m_predictiveMove = predictiveMove;
        emit predictiveMoveChanged();
    }
}

void UsiCommLogModel::setSearchedMove(const QString& searchedMove)
{
    if (m_searchedMove != searchedMove)
    {
        m_searchedMove = searchedMove;
        emit searchedMoveChanged();
    }
}

void UsiCommLogModel::setSearchDepth(const QString& searchDepth)
{
    if (m_searchDepth != searchDepth)
    {
        m_searchDepth = searchDepth;
        emit searchDepthChanged();
    }
}

void UsiCommLogModel::setNodeCount(const QString& nodeCount)
{
    if (m_nodeCount != nodeCount)
    {
        m_nodeCount = nodeCount;
        emit nodeCountChanged();
    }
}

void UsiCommLogModel::setNodesPerSecond(const QString& nodesPerSecond)
{
    if (m_nodesPerSecond != nodesPerSecond)
    {
        m_nodesPerSecond = nodesPerSecond;
        emit nodesPerSecondChanged();
    }
}

void UsiCommLogModel::setHashUsage(const QString& hashUsage)
{
    if (m_hashUsage != hashUsage)
    {
        m_hashUsage = hashUsage;
        emit hashUsageChanged();
    }
}

// ============================================================
// ログ追加・クリア
// ============================================================

void UsiCommLogModel::appendUsiCommLog(const QString& usiCommLog)
{
    m_usiCommLog = usiCommLog;
    emit usiCommLogChanged();
}

void UsiCommLogModel::clear()
{
    // 値が非空の場合のみ変更＆シグナル発火
    if (!m_engineName.isEmpty())       { m_engineName.clear();       emit engineNameChanged(); }
    if (!m_predictiveMove.isEmpty())   { m_predictiveMove.clear();   emit predictiveMoveChanged(); }
    if (!m_searchedMove.isEmpty())     { m_searchedMove.clear();     emit searchedMoveChanged(); }
    if (!m_searchDepth.isEmpty())      { m_searchDepth.clear();      emit searchDepthChanged(); }
    if (!m_nodeCount.isEmpty())        { m_nodeCount.clear();        emit nodeCountChanged(); }
    if (!m_nodesPerSecond.isEmpty())   { m_nodesPerSecond.clear();   emit nodesPerSecondChanged(); }
    if (!m_hashUsage.isEmpty())        { m_hashUsage.clear();        emit hashUsageChanged(); }
    if (!m_usiCommLog.isEmpty())       { m_usiCommLog.clear();       emit usiCommLogChanged(); }
}
