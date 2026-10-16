/// @file fontsizehelper.cpp
/// @brief 文字の大きさ（A-/A+）の状態と保存の実装

#include "fontsizehelper.h"

#include <algorithm>

FontSizeHelper::FontSizeHelper(const Config& config, QObject* parent)
    : QObject(parent)
    , m_fontSize(std::clamp(config.initialSize, config.minSize, config.maxSize))
    , m_minSize(config.minSize)
    , m_maxSize(config.maxSize)
    , m_step(config.step)
    , m_saveFn(config.saveFn)
{
}

int FontSizeHelper::fontSize() const
{
    return m_fontSize;
}

void FontSizeHelper::setFontSize(int size)
{
    const int clamped = std::clamp(size, m_minSize, m_maxSize);
    if (clamped == m_fontSize) return;
    m_fontSize = clamped;
    if (m_saveFn) m_saveFn(m_fontSize);
    emit fontSizeChanged(m_fontSize);
}

bool FontSizeHelper::increase()
{
    if (m_fontSize + m_step > m_maxSize) return false;
    setFontSize(m_fontSize + m_step);
    return true;
}

bool FontSizeHelper::decrease()
{
    if (m_fontSize - m_step < m_minSize) return false;
    setFontSize(m_fontSize - m_step);
    return true;
}
