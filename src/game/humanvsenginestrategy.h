#ifndef HUMANVSENGINESTRATEGY_H
#define HUMANVSENGINESTRATEGY_H

/// @file humanvsenginestrategy.h
/// @brief 人間 vs エンジンモードの Strategy

#include <QObject>
#include <QString>

#include "gamemodestrategy.h"
#include "usimatchhandler.h"
#include "enginelifecyclemanager.h"

/**
 * @brief 人間とエンジンの対局。エンジンは E1 の枠に置き、engineSide の手番を指す
 */
class HumanVsEngineStrategy : public QObject, public GameModeStrategy {
    Q_OBJECT

public:
    HumanVsEngineStrategy(MatchStrategyHost& host, Turn engineSide, QString enginePath, QString engineName,
                          QObject* parent = nullptr);

    void start() override;
    void onHumanMove(const RecordEntry& played) override;
    void startInitialMoveIfNeeded() override;

private slots:
    void onEngineInitialized();
    void onEngineMoveReady(const UsiMatchMove& move);

private:
    void requestEngineMove();

    MatchStrategyHost& m_host;
    Turn m_engineSide;             ///< エンジンの手番（人間はその相手）
    QString m_enginePath;
    QString m_engineName;
    EngineLease m_engine;          ///< 枠 E1 のエンジン（次の対局の司令塔が戦略を作り直すときに手放す）
    bool m_waitingForMove = false;
    bool m_initialMoveRequested = false;
};

#endif // HUMANVSENGINESTRATEGY_H
