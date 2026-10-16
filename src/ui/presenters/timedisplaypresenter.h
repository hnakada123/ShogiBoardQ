#ifndef TIMEDISPLAYPRESENTER_H
#define TIMEDISPLAYPRESENTER_H

/// @file timedisplaypresenter.h
/// @brief 時刻表示プレゼンタクラスの定義

#include <QObject>
#include <QString>

#include "shogitypes.h"

class ShogiView;
class ShogiClock;

/**
 * @brief 対局時計の残り時間を盤の時計欄に出す
 *
 * 時計（ShogiClock）の timeUpdated を直接受ける。ローカル対局は対局の時計、通信対局はサーバーの残り時間を
 * 写した表示用の時計で、どちらも同じ時計を見る（対局の司令塔は中継しない）。
 */
class TimeDisplayPresenter : public QObject
{
    Q_OBJECT
public:
    TimeDisplayPresenter(ShogiView* view, const ShogiClock* clock, QObject* parent = nullptr);

    /// 最後に受け取った残り時間と手番で表示を描き直す（盤を反転したときなど）。まだ受け取っていなければ何もしない
    void refresh();

    /// 残り時間と手番を出す（時計の値によらない表示のテスト用にも使う）
    void showTimes(qint64 blackMs, qint64 whiteMs, Turn sideToMove);

public slots:
    /// 時計の今の値を出す（ShogiClock::timeUpdated から）
    void showClock();

private:
    void updateUrgencyStyles(Turn sideToMove);

    static inline QString fmt_hhmmss(qint64 ms);

private:
    ShogiView*  m_view  = nullptr;
    const ShogiClock* m_clock = nullptr;
    qint64      m_lastBlackMs = 0;
    qint64      m_lastWhiteMs = 0;
    Turn        m_lastSideToMove = Turn::Black;
    bool        m_hasLastValues = false;    ///< showTimes を一度でも呼んだか
};

#endif // TIMEDISPLAYPRESENTER_H
