#ifndef ENGINEVSENGINESTRATEGY_H
#define ENGINEVSENGINESTRATEGY_H

/// @file enginevsenginestrategy.h
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "gamemodestrategy.h"
#include "matchcoordinator.h"

#include <QObject>
#include <QStringList>
#include <QVector>

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
    EngineVsEngineStrategy(MatchCoordinator* coordinator,
                           const MatchCoordinator::StartOptions& opt,
                           QObject* parent = nullptr);

    void start() override;
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove) override;
    bool needsEngine() const override { return true; }

    /// EvE用SFEN履歴へのアクセサ（千日手チェック等で使用）
    QStringList* sfenRecordForEvE();
    QVector<ShogiMove>& gameMovesForEvE();
    int eveMoveIndex() const { return m_eveMoveIndex; }

private slots:
    void kickNextEvETurn();

private:
    void initPositionStringsForEvE(const QString& sfenStart);
    void startEvEFirstMoveByBlack();
    void startEvEFirstMoveByWhite();

    MatchCoordinator* m_coordinator;
    MatchCoordinator::StartOptions m_opt;

    /// EvE専用の棋譜コンテナ
    QStringList        m_eveSfenRecord;
    QVector<ShogiMove> m_eveGameMoves;
    int                m_eveMoveIndex = 0;
};

#endif // ENGINEVSENGINESTRATEGY_H
