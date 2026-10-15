#ifndef ENGINEVSENGINESTRATEGY_H
#define ENGINEVSENGINESTRATEGY_H

/// @file enginevsenginestrategy.h
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "gamemodestrategy.h"
#include "matchcoordinator.h"

#include <QObject>
#include <QStringList>
#include <QList>

#include "shogimove.h"

class Usi;

/**
 * @brief エンジン vs エンジンモードの対局ロジック
 *
 * MatchCoordinator から EvE 固有の処理（開始・初手・交互手番ループ）
 * を分離した Strategy 実装。QObject を継承して QTimer::singleShot の
 * スロット接続を可能にしている。
 */
class EngineVsEngineStrategy : public QObject, public GameModeStrategy {
    Q_OBJECT

public:
    EngineVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                           QObject* parent = nullptr);

    void start() override;
    void startInitialMoveIfNeeded() override { kickNextEvETurn(); }
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove) override;
    bool needsEngine() const override { return true; }

private slots:
    void kickNextEvETurn();
    void onEngineMoveReady(const UsiMove& move, const QString& position, const QString& ponder);

private:
    bool m_waitingForMove = false;
    bool m_waitingInitialization = false;

    MatchCoordinator::StrategyContext& m_ctx;

};

#endif // ENGINEVSENGINESTRATEGY_H
