/// @file timecontroleditor.cpp
/// @brief 片方の対局者の持ち時間・秒読み・加算の入力欄の実装

#include "timecontroleditor.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

namespace {
constexpr qint64 kSecondMs = 1000;
constexpr qint64 kMinuteMs = 60 * kSecondMs;
constexpr qint64 kHourMs = 60 * kMinuteMs;

QSpinBox* makeSpinBox(QWidget* parent, const QString& name, int maximum, const QString& suffix,
                      const QString& accessibleName)
{
    auto* box = new QSpinBox(parent);
    box->setObjectName(name);
    box->setMinimumWidth(60);
    box->setMaximum(maximum);
    box->setSuffix(suffix);
    box->setAccessibleName(accessibleName);
    return box;
}
} // namespace

TimeControlEditor::TimeControlEditor(int side, QWidget* parent)
    : QWidget(parent)
{
    const QString n = QString::number(side);
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(2, 1);

    m_hours = makeSpinBox(this, QStringLiteral("basicTimeHour") + n, 99, tr(" 時間"), tr("持ち時間（時間）"));
    m_minutes = makeSpinBox(this, QStringLiteral("basicTimeMinutes") + n, 59, tr(" 分"), tr("持ち時間（分）"));
    m_byoyomi = makeSpinBox(this, QStringLiteral("byoyomiSec") + n, 3600, tr(" 秒"), tr("秒読み"));
    m_increment = makeSpinBox(this, QStringLiteral("addEachMoveSec") + n, 3600, tr(" 秒"), tr("1手ごとの加算"));

    const struct {
        QString label;
        QList<QSpinBox*> boxes;
    } rows[] = {
        {tr("持ち時間"), {m_hours, m_minutes}},
        {tr("秒読み"), {m_byoyomi}},
        {tr("1手ごとの加算"), {m_increment}},
    };
    int row = 0;
    for (const auto& entry : rows) {
        auto* label = new QLabel(entry.label, this);
        label->setObjectName(QStringLiteral("timeLabel%1_%2").arg(n).arg(row));
        label->setBuddy(entry.boxes.first());
        auto* boxes = new QHBoxLayout;
        for (QSpinBox* box : entry.boxes) boxes->addWidget(box);
        grid->addWidget(label, row, 0);
        grid->addLayout(boxes, row, 1);
        ++row;
    }

    connect(m_byoyomi, &QSpinBox::valueChanged, this, &TimeControlEditor::onByoyomiChanged);
    connect(m_increment, &QSpinBox::valueChanged, this, &TimeControlEditor::onIncrementChanged);
}

TimeControlSide TimeControlEditor::value() const
{
    TimeControlSide time;
    time.mainMs = m_hours->value() * kHourMs + m_minutes->value() * kMinuteMs;
    time.byoyomiMs = m_byoyomi->value() * kSecondMs;
    time.incrementMs = m_increment->value() * kSecondMs;
    return time;
}

void TimeControlEditor::setValue(const TimeControlSide& time)
{
    m_hours->setValue(static_cast<int>(time.mainMs / kHourMs));
    m_minutes->setValue(static_cast<int>(time.mainMs % kHourMs / kMinuteMs));
    m_byoyomi->setValue(static_cast<int>(time.byoyomiMs / kSecondMs));
    m_increment->setValue(static_cast<int>(time.incrementMs / kSecondMs));
}

void TimeControlEditor::clearIncrement()
{
    m_increment->setValue(0);
}

void TimeControlEditor::clearByoyomi()
{
    m_byoyomi->setValue(0);
}

void TimeControlEditor::onByoyomiChanged(int value)
{
    if (value <= 0) return;
    clearIncrement();
    emit byoyomiEntered();
}

void TimeControlEditor::onIncrementChanged(int value)
{
    if (value <= 0) return;
    clearByoyomi();
    emit incrementEntered();
}
