#include "fontsizehelper.h"

#include <algorithm>

FontSizeHelper::FontSizeHelper(const Config& config)
    : m_fontSize(std::clamp(config.initialSize, config.minSize, config.maxSize))
    , m_minSize(config.minSize)
    , m_maxSize(config.maxSize)
    , m_step(config.step)
    , m_saveFn(config.saveFn)
{
}

bool FontSizeHelper::increase()
{
    if (m_fontSize + m_step > m_maxSize) return false;
    m_fontSize += m_step;
    if (m_saveFn) m_saveFn(m_fontSize);
    return true;
}

bool FontSizeHelper::decrease()
{
    if (m_fontSize - m_step < m_minSize) return false;
    m_fontSize -= m_step;
    if (m_saveFn) m_saveFn(m_fontSize);
    return true;
}

int FontSizeHelper::fontSize() const
{
    return m_fontSize;
}
