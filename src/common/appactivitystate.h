#ifndef APPACTIVITYSTATE_H
#define APPACTIVITYSTATE_H

/// @file appactivitystate.h
/// @brief アプリがいま何をしているか（待機・対局・解析・検討・詰み探索・通信対局・局面編集）

#include <QObject>

/// アプリの動作状態の正本。メニューやボタンの有効・無効（UiStatePolicyManager）はこの状態から決める。
/// 対局者の組（人間・エンジン）は対局の司令塔（MatchCoordinator::players）が持つ。
class AppActivityState : public QObject
{
    Q_OBJECT
public:
    enum class Activity { Idle, DuringGame, DuringAnalysis, DuringCsaGame,
                          DuringTsumeSearch, DuringConsideration, DuringPositionEdit };
    Q_ENUM(Activity)

    Activity activity() const { return m_activity; }
    bool is(Activity activity) const { return m_activity == activity; }

    void setActivity(Activity next)
    {
        if (m_activity == next) return;
        m_activity = next;
        emit changed();
    }

signals:
    void changed();

private:
    Activity m_activity = Activity::Idle;
};

#endif // APPACTIVITYSTATE_H
