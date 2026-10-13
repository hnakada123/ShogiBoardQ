#ifndef MATCHTIMEKEEPER_H
#define MATCHTIMEKEEPER_H

/// @file matchtimekeeper.h
/// @brief 時計の配線・残り時間の通知・ターンエポック管理クラスの定義

#include <QObject>
#include <QString>
#include <QMetaObject>
#include <functional>

#include "matchcoordinator.h"

class ShogiClock;
class ShogiGameController;

/**
 * @brief 時計の配線・残り時間の通知・ターンエポック管理を担当するマネージャ
 *
 * 時計との接続/切断と timeUpdated シグナルの発行を行う。持ち時間の決まりは時計（ShogiClock）が持ち、
 * エンジンの思考時間はエンジンにつないだ時計から UsiGoTimeCalculator が求める。
 */
class MatchTimekeeper : public QObject
{
    Q_OBJECT

public:

    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;
    };

    explicit MatchTimekeeper(QObject* parent = nullptr);

    void setRefs(const Refs& refs);

    // --- 時計管理 ---

    void setClock(ShogiClock* clock);
    ShogiClock* clock() const;
    const ShogiClock* clockConst() const;

    // --- ターンエポック ---

    void markTurnEpochNowFor(Turn side, qint64 nowMs = -1);
    qint64 turnEpochFor(Turn side) const;

    // --- 時計スナップショット ---

    void recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const;

public slots:
    void pokeTimeUpdateNow();

signals:
    void timeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);

private slots:
    void onClockTick();

private:
    void wireClock();
    void unwireClock();
    void emitTimeUpdateFromClock();

    Refs m_refs;

    ShogiClock* m_clock = nullptr;
    QMetaObject::Connection m_clockConn;
    qint64 m_turnEpochP1Ms = -1;
    qint64 m_turnEpochP2Ms = -1;
};

#endif // MATCHTIMEKEEPER_H
