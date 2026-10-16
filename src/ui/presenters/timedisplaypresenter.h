#ifndef TIMEDISPLAYPRESENTER_H
#define TIMEDISPLAYPRESENTER_H

/// @file timedisplaypresenter.h
/// @brief 時刻表示プレゼンタクラスの定義

#include <QObject>
#include <QString>

#include "shogitypes.h"

class ShogiView;
class ShogiClock;

class TimeDisplayPresenter : public QObject
{
    Q_OBJECT
public:
    TimeDisplayPresenter(ShogiView* view, const ShogiClock* clock, QObject* parent = nullptr);

    /// 最後に受け取った残り時間と手番で表示を描き直す（盤を反転したときなど）。まだ受け取っていなければ何もしない
    void refresh();

public slots:
    // MatchCoordinator::timeUpdated から接続
    void onMatchTimeUpdated(qint64 blackMs, qint64 whiteMs, Turn sideToMove, qint64 urgencyMs);

private:
    void updateUrgencyStyles(Turn sideToMove);

    static inline QString fmt_hhmmss(qint64 ms);

private:
    ShogiView*  m_view  = nullptr;
    const ShogiClock* m_clock = nullptr;
    qint64      m_lastBlackMs = 0;
    qint64      m_lastWhiteMs = 0;
    Turn        m_lastSideToMove = Turn::Black;
    bool        m_hasLastValues = false;    ///< onMatchTimeUpdated を一度でも受け取ったか
};

#endif // TIMEDISPLAYPRESENTER_H
