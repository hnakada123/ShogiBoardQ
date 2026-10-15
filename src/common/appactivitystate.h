#ifndef APPACTIVITYSTATE_H
#define APPACTIVITYSTATE_H
#include <QObject>
#include "playmode.h"

/// アプリの実行状態の正本。対局設定とは独立し、UIはこの状態から導出する。
class AppActivityState : public QObject
{
    Q_OBJECT
public:
    enum class Activity { Idle, DuringGame, DuringAnalysis, DuringCsaGame,
                          DuringTsumeSearch, DuringConsideration, DuringPositionEdit };
    Q_ENUM(Activity)
    Activity activity() const { return m_activity; }
    PlayMode playMode() const { return m_effectiveMode; }
    const PlayMode& playModeRef() const { return m_effectiveMode; }

    void setPlayMode(PlayMode mode)
    {
        Activity next = Activity::DuringGame;
        switch (mode) {
        case PlayMode::NotStarted:
        case PlayMode::PlayModeError: next = Activity::Idle; break;
        case PlayMode::AnalysisMode: next = Activity::DuringAnalysis; break;
        case PlayMode::ConsiderationMode: next = Activity::DuringConsideration; break;
        case PlayMode::TsumiSearchMode: next = Activity::DuringTsumeSearch; break;
        case PlayMode::CsaNetworkMode: next = Activity::DuringCsaGame; break;
        default: m_matchMode = mode; break;
        }
        if (m_activity == next && m_effectiveMode == mode) return;
        m_activity = next;
        m_effectiveMode = mode;
        emit changed();
    }
    void setActivity(Activity next)
    {
        if (m_activity == next) return;
        switch (next) {
        case Activity::DuringGame: setPlayMode(m_matchMode); return;
        case Activity::DuringAnalysis: setPlayMode(PlayMode::AnalysisMode); return;
        case Activity::DuringConsideration: setPlayMode(PlayMode::ConsiderationMode); return;
        case Activity::DuringTsumeSearch: setPlayMode(PlayMode::TsumiSearchMode); return;
        case Activity::DuringCsaGame: setPlayMode(PlayMode::CsaNetworkMode); return;
        case Activity::Idle:
        case Activity::DuringPositionEdit:
            m_activity = next;
            m_effectiveMode = PlayMode::NotStarted;
            emit changed();
            return;
        }
    }
signals:
    void changed();
private:
    Activity m_activity = Activity::Idle;
    PlayMode m_effectiveMode = PlayMode::NotStarted;
    PlayMode m_matchMode = PlayMode::HumanVsHuman;
};
#endif // APPACTIVITYSTATE_H
