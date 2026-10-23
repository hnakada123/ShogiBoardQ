#ifndef HUMANVSENGINESTRATEGY_H
#define HUMANVSENGINESTRATEGY_H

/// @file humanvsenginestrategy.h
/// @brief 人間 vs エンジンモードの Strategy

#include <QObject>
#include <QString>

#include "gamemodestrategy.h"
#include "usimatchhandler.h"

class MatchEngineSeat;

/**
 * @brief 人間とエンジンの対局。エンジンは E1 の枠に置き、engineSide の手番を指す
 *
 * エンジンの初期化を待つ間に手番が来たら（初手・人間が先に指した後）、時計を止めて初期化の完了を待つ。
 */
class HumanVsEngineStrategy : public QObject, public GameModeStrategy {
    Q_OBJECT

public:
    HumanVsEngineStrategy(MatchStrategyHost& host, Turn engineSide, QString enginePath, QString engineName,
                          QObject* parent = nullptr);

    void start() override;
    void onHumanMove(const RecordEntry& played) override;
    void startInitialMoveIfNeeded() override;
    MatchEngineSeat* engineSeat(Turn side) const override { return side == m_engineSide ? m_seat : nullptr; }

private slots:
    void onEngineInitialized();
    void onEngineMoveReady(const UsiMatchMove& move);

private:
    /// エンジンに思考を頼む。初期化中なら時計を止めて、初期化の後で頼む
    void requestEngineMoveWhenReady();
    void requestEngineMove();

    MatchStrategyHost& m_host;
    Turn m_engineSide;             ///< エンジンの手番（人間はその相手）
    QString m_enginePath;
    QString m_engineName;
    MatchEngineSeat* m_seat = nullptr;  ///< 枠 E1 のエンジン（次の対局の司令塔が戦略を作り直すときに手放す）
    bool m_waitingForMove = false;
    bool m_moveWanted = false;     ///< 初期化を待って思考を頼む
};

#endif // HUMANVSENGINESTRATEGY_H
