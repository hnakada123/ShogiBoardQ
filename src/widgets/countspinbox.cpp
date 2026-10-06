/// @file countspinbox.cpp
/// @brief 値が ±1 のときに単数形の単位を付けるスピンボックスクラスの実装

#include "countspinbox.h"

#include <utility>

CountSpinBox::CountSpinBox(QString one, QString many, QWidget* parent)
    : QSpinBox(parent)
    , m_one(std::move(one))
    , m_many(std::move(many))
{
    connect(this, &QSpinBox::valueChanged, this, &CountSpinBox::updateSuffix);
    updateSuffix(value());
}

void CountSpinBox::updateSuffix(int value)
{
    setSuffix(qAbs(value) == 1 ? m_one : m_many);
}
