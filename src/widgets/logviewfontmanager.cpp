/// @file logviewfontmanager.cpp
/// @brief ログビューのフォントサイズ管理ヘルパークラスの実装

#include "logviewfontmanager.h"

#include <QFont>
#include <QWidget>

LogViewFontManager::LogViewFontManager(int& fontSize, QWidget* targetWidget)
    : m_fontSize(fontSize)
    , m_targetWidget(targetWidget)
{
}

void LogViewFontManager::setPostApplyCallback(PostApplyCallback callback)
{
    m_postApply = std::move(callback);
}

void LogViewFontManager::increase()
{
    m_fontSize = qBound(8, m_fontSize + 1, 24);
    apply();
}

void LogViewFontManager::decrease()
{
    m_fontSize = qBound(8, m_fontSize - 1, 24);
    apply();
}

void LogViewFontManager::apply()
{
    if (m_targetWidget) {
        QFont font = m_targetWidget->font();
        font.setPointSize(m_fontSize);
        m_targetWidget->setFont(font);
    }
    if (m_postApply) {
        m_postApply(m_fontSize);
    }
}
