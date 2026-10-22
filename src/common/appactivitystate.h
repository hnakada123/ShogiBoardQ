#ifndef APPACTIVITYSTATE_H
#define APPACTIVITYSTATE_H

/// @file appactivitystate.h
/// @brief アプリがいま何をしているか（待機・対局・解析・検討・詰み探索・通信対局・局面編集）と、その持ち主

#include <QList>
#include <QObject>
#include <QPointer>

class AppActivityState;

/**
 * @brief 動作状態を持つ権利（エンジンの枠の EngineLease と同じ形。move-only）
 *
 * AppActivityState::begin が返す。持っている間だけ、その動作状態が続く。手放す（reset・破棄）と
 * 待機中に戻す。ほかの部品が別の動作を始めた後（取り上げられた後）は、手放しても何もしない。
 */
class ActivityLease
{
public:
    ActivityLease() = default;
    ~ActivityLease() { reset(); }
    ActivityLease(ActivityLease&& other) noexcept;
    ActivityLease& operator=(ActivityLease&& other) noexcept;
    ActivityLease(const ActivityLease&) = delete;
    ActivityLease& operator=(const ActivityLease&) = delete;

    /// 今もこのリースの動作状態か（取り上げられていない・手放していない）
    bool isCurrent() const;
    /// 手放す（今もこのリースの動作状態なら待機中に戻す）
    void reset();

private:
    friend class AppActivityState;
    ActivityLease(AppActivityState* state, quint64 token) : m_state(state), m_token(token) {}

    QPointer<AppActivityState> m_state;
    quint64 m_token = 0;
};

/**
 * @brief アプリの動作状態の正本
 *
 * メニューやボタンの有効・無効（UiStatePolicyManager）はこの状態から決める。状態を変えられるのは
 * begin で受け取ったリースの持ち主だけで、終えられるのも持ち主だけ。別の部品が別の動作を始めると、
 * 前の持ち主に revoked で知らせる（受けた部品は自分の処理を終える）。
 * 対局者の組（人間・エンジン）は対局の司令塔（MatchCoordinator::players）が持つ。
 */
class AppActivityState : public QObject
{
    Q_OBJECT
public:
    enum class Activity { Idle, DuringGame, DuringAnalysis, DuringCsaGame,
                          DuringTsumeSearch, DuringConsideration, DuringPositionEdit };
    Q_ENUM(Activity)

    Activity activity() const { return m_activity; }
    bool is(Activity activity) const { return m_activity == activity; }

    /// owner が activity を始める。前の持ち主（別の部品）には revoked で知らせる
    [[nodiscard]] ActivityLease begin(Activity activity, QObject* owner);

signals:
    void changed();
    /// owner が持っていた動作状態を、別の部品が始めた動作が取り上げた。始めた部品の準備が
    /// 済んでから（次のイベントループで）届くので、受け手は自分の処理を終えるだけでよい
    void revoked(QObject* owner);

private slots:
    void notifyRevoked();

private:
    friend class ActivityLease;
    /// token のリースが今の持ち主なら待機中に戻す
    void release(quint64 token);
    void setActivity(Activity next);

    Activity m_activity = Activity::Idle;
    quint64 m_token = 0;        ///< 今の持ち主のリース（待機中は 0）
    quint64 m_nextToken = 1;
    QPointer<QObject> m_owner;  ///< 今の持ち主
    QList<QPointer<QObject>> m_revokedOwners;  ///< まだ知らせていない、取り上げられた持ち主
};

#endif // APPACTIVITYSTATE_H
