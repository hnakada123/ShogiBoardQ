#ifndef TIMECONTROLEDITOR_H
#define TIMECONTROLEDITOR_H

/// @file timecontroleditor.h
/// @brief 片方の対局者の持ち時間・秒読み・加算の入力欄

#include <QWidget>

#include "timecontrol.h"

class QSpinBox;

/**
 * @brief 片方の対局者の持ち時間（時間・分）・秒読み・1手ごとの加算を入力する欄
 *
 * 秒読みと加算は排他で、一方に値を入れるともう一方を0にする。相手側の欄にも同じことをするよう、
 * 値を入れたことを byoyomiEntered・incrementEntered で知らせる。
 */
class TimeControlEditor : public QWidget
{
    Q_OBJECT

public:
    /// side は部品の objectName に付ける番号（1=先手／下手、2=後手／上手。例: byoyomiSec1）
    explicit TimeControlEditor(int side, QWidget* parent = nullptr);

    /// 入力した時間（ミリ秒。秒より細かい値は持たない）
    TimeControlSide value() const;
    /// 時間を入れる（持ち時間は時間・分、秒読みと加算は秒に切り捨てる）
    void setValue(const TimeControlSide& time);

public slots:
    /// 加算を0にする（相手側で秒読みを入れたとき）
    void clearIncrement();
    /// 秒読みを0にする（相手側で加算を入れたとき）
    void clearByoyomi();

signals:
    /// 秒読みに0より大きい値を入れた
    void byoyomiEntered();
    /// 加算に0より大きい値を入れた
    void incrementEntered();

private slots:
    void onByoyomiChanged(int value);
    void onIncrementChanged(int value);

private:
    QSpinBox* m_hours = nullptr;
    QSpinBox* m_minutes = nullptr;
    QSpinBox* m_byoyomi = nullptr;
    QSpinBox* m_increment = nullptr;
};

#endif // TIMECONTROLEDITOR_H
