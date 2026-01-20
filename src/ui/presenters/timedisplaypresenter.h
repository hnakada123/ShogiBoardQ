#ifndef TIMEDISPLAYPRESENTER_H
#define TIMEDISPLAYPRESENTER_H

#include <QObject>
#include <QString>

class ShogiView;
class ShogiClock;

class TimeDisplayPresenter : public QObject
{
    Q_OBJECT
public:
    explicit TimeDisplayPresenter(ShogiView* view, QObject* parent = nullptr);

    // ShogiClock を設定（秒読み状態の判定に使用）
    void setClock(ShogiClock* clock);

public slots:
    // MatchCoordinator::timeUpdated から接続
    void onMatchTimeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);

private:
    void applyTurnHighlights(bool p1turn);
    void updateUrgencyStyles(bool p1turn);

    // 秒読みに入っているかどうかを判定
    bool isInByoyomi(bool p1turn) const;

    static inline QString fmt_hhmmss(qint64 ms);

private:
    ShogiView*  m_view  = nullptr;
    ShogiClock* m_clock = nullptr;
    qint64      m_lastP1Ms = 0;
    qint64      m_lastP2Ms = 0;
};

#endif // TIMEDISPLAYPRESENTER_H
