#ifndef MATCHTIMEKEEPER_H
#define MATCHTIMEKEEPER_H

/// @file matchtimekeeper.h
/// @brief 時計管理・USI残時間計算・ターンエポック管理クラスの定義

#include <QObject>
#include <QString>
#include <QMetaObject>
#include <functional>

#include "matchcoordinator.h"

class ShogiClock;
class ShogiGameController;

/**
 * @brief 時計管理・USI残時間計算・ターンエポック管理を担当するマネージャ
 *
 * MatchCoordinator から時間管理関連のロジックを分離する。
 * 時計との接続/切断、timeUpdated シグナルの発行、GoTimes 計算を行う。
 *
 * MatchCoordinator 内部状態への参照は Refs struct で受け取り、
 * 時計読み出しのコールバックは Hooks struct で実行する。
 */
class MatchTimekeeper : public QObject
{
    Q_OBJECT

public:
    using Player = MatchCoordinator::Player;
    using GoTimes = MatchCoordinator::GoTimes;
    using TimeControl = MatchCoordinator::TimeControl;

    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;
    };

    /**
     * @brief 時計読み出しコールバック群
     *
     * ShogiClock の API 差異を吸収し、GoTimes 計算に必要な時間情報を取得する。
     *
     * @note MC::ensureTimekeeper() で lambda 経由で設定される。
     *       MC::Hooks の remainingMsFor/incrementMsFor/byoyomiMs を int 引数版に変換。
     * @see MatchCoordinator::ensureTimekeeper
     */
    struct Hooks {
        /// @brief 指定プレイヤー (0=P1, 1=P2) の残り時間（ms）を返す
        /// @note 配線元: MC lambda → MC::Hooks::remainingMsFor (Player→int 変換)
        std::function<qint64(int)> remainingMsFor;

        /// @brief 指定プレイヤー (0=P1, 1=P2) のフィッシャー加算時間（ms）を返す
        /// @note 配線元: MC lambda → MC::Hooks::incrementMsFor (Player→int 変換)
        std::function<qint64(int)> incrementMsFor;

        /// @brief 秒読み時間（ms、共通）を返す
        /// @note 配線元: MC lambda → MC::Hooks::byoyomiMs (パススルー)
        std::function<qint64()> byoyomiMs;
    };

    explicit MatchTimekeeper(QObject* parent = nullptr);

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- 時計管理 ---

    void setClock(ShogiClock* clock);
    ShogiClock* clock() const;
    const ShogiClock* clockConst() const;

    // --- 時間制御設定 ---

    void setTimeControlConfig(bool useByoyomi,
                              int byoyomiMs1, int byoyomiMs2,
                              int incMs1,     int incMs2,
                              bool loseOnTimeout);
    const TimeControl& timeControl() const;

    // --- GoTimes 計算 ---

    GoTimes computeGoTimes() const;
    void computeGoTimesForUSI(qint64& outB, qint64& outW) const;
    void refreshGoTimes();

    // --- ターンエポック ---

    void markTurnEpochNowFor(Player side, qint64 nowMs = -1);
    qint64 turnEpochFor(Player side) const;

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
    Hooks m_hooks;

    ShogiClock* m_clock = nullptr;
    QMetaObject::Connection m_clockConn;
    TimeControl m_tc;
    qint64 m_turnEpochP1Ms = -1;
    qint64 m_turnEpochP2Ms = -1;
};

#endif // MATCHTIMEKEEPER_H
