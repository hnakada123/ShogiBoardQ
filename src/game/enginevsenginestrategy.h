#ifndef ENGINEVSENGINESTRATEGY_H
#define ENGINEVSENGINESTRATEGY_H

/// @file enginevsenginestrategy.h
/// @brief エンジン vs エンジンモードの Strategy

#include <QObject>
#include <QString>

#include "gamemodestrategy.h"
#include "usimatchhandler.h"
#include "enginelifecyclemanager.h"

/**
 * @brief エンジン同士の対局。先手は E1、後手は E2 の枠のエンジンが指す
 */
class EngineVsEngineStrategy : public QObject, public GameModeStrategy {
    Q_OBJECT

public:
    struct Engines {
        QString blackPath;
        QString blackName;
        QString whitePath;
        QString whiteName;
    };

    EngineVsEngineStrategy(MatchStrategyHost& host, Engines engines, QObject* parent = nullptr);

    void start() override;
    void startInitialMoveIfNeeded() override { kickNextTurn(); }
    void onHumanMove(const RecordEntry& /*played*/) override {}

private slots:
    void kickNextTurn();
    void onEngineMoveReady(const UsiMatchMove& move);

private:
    MatchStrategyHost& m_host;
    Engines m_engines;
    EngineLease m_black;   ///< 枠 E1 の先手のエンジン
    EngineLease m_white;   ///< 枠 E2 の後手のエンジン
    bool m_waitingForMove = false;
};

#endif // ENGINEVSENGINESTRATEGY_H
