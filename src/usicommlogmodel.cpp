#include "usicommlogmodel.h"

// エンジン名、予想手、探索手、深さ、ノード数、局面探索数、ハッシュ使用率の更新に関するクラス
// コンストラクタ
UsiCommLogModel::UsiCommLogModel(QObject* parent) : QObject(parent) {}

// 将棋エンジン名を取得する。
QString UsiCommLogModel::engineName() const
{
    return m_engineName;
}

// 予想手を取得する。
QString UsiCommLogModel::predictiveMove() const
{
    return m_predictiveMove;
}

// 探索手を取得する。
QString UsiCommLogModel::searchedMove() const
{
    return m_searchedMove;
}

// 深さを取得する。
QString UsiCommLogModel::searchDepth() const
{
    return m_searchDepth;
}

// ノード数を取得する。
QString UsiCommLogModel::nodeCount() const
{
    return m_nodeCount;
}

// 探索局面数を取得する。
QString UsiCommLogModel::nodesPerSecond() const
{
    return m_nodesPerSecond;
}

// ハッシュ使用率を取得する。
QString UsiCommLogModel::hashUsage() const
{
    return m_hashUsage;
}

// 将棋GUIと将棋エンジン間のUSIプロトコル通信コマンド行を取得する。
QString UsiCommLogModel::usiCommLog() const
{
    return m_usiCommLog;
}

// 将棋エンジン名をセットし、GUIの表示を更新する。
void UsiCommLogModel::setEngineName(const QString& engineName)
{
    if (m_engineName != engineName)
    {
        m_engineName = engineName;
        emit engineNameChanged();
    }
}

// 予想手をセットし、GUIの表示を更新する。
void UsiCommLogModel::setPredictiveMove(const QString& predictiveMove)
{
    if (m_predictiveMove != predictiveMove)
    {
        m_predictiveMove = predictiveMove;
        emit predictiveMoveChanged();
    }
}

// 探索手をセットし、GUIの表示を更新する。
void UsiCommLogModel::setSearchedMove(const QString& searchedMove)
{
    if (m_searchedMove != searchedMove)
    {
        m_searchedMove = searchedMove;
        emit searchedMoveChanged();
    }
}

// 深さをセットし、GUIの表示を更新する。
void UsiCommLogModel::setSearchDepth(const QString& searchDepth)
{
    if (m_searchDepth != searchDepth)
    {
        m_searchDepth = searchDepth;
        emit searchDepthChanged();
    }
}

// ノード数をセットし、GUIの表示を更新する。
void UsiCommLogModel::setNodeCount(const QString& nodeCount)
{
    if (m_nodeCount != nodeCount)
    {
        m_nodeCount = nodeCount;
        emit nodeCountChanged();
    }
}

// 探索局面数をセットし、GUIの表示を更新する。
void UsiCommLogModel::setNodesPerSecond(const QString& nodesPerSecond)
{
    if (m_nodesPerSecond != nodesPerSecond)
    {
        m_nodesPerSecond = nodesPerSecond;
        emit nodesPerSecondChanged();
    }
}

// ハッシュ使用率をセットし、GUIの表示を更新する。
void UsiCommLogModel::setHashUsage(const QString& hashUsage)
{
    if (m_hashUsage != hashUsage)
    {
        m_hashUsage = hashUsage;
        emit hashUsageChanged();
    }
}

// 将棋GUIと将棋エンジン間のUSIプロトコル通信コマンド行をセットし、GUIに追記する。
void UsiCommLogModel::appendUsiCommLog(const QString& usiCommLog)
{
    m_usiCommLog = usiCommLog;
    emit usiCommLogChanged();
}
