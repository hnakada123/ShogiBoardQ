#ifndef TOOLBARWIDTH_H
#define TOOLBARWIDTH_H

/// @file toolbarwidth.h
/// @brief ドックのパネルの操作列の幅を、ドックの幅に合わせて縮められるようにする

#include <QSizePolicy>
#include <QWidget>

namespace ToolbarWidth {

/// 操作列（ツールバー・コマンドの欄）の最小幅でドックを広げない（狭いドックでは操作列のほうを縮める）
inline void relax(QWidget* toolbar)
{
    if (!toolbar) return;
    toolbar->setMinimumWidth(0);
    QSizePolicy policy = toolbar->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Ignored);
    toolbar->setSizePolicy(policy);
}

} // namespace ToolbarWidth

#endif // TOOLBARWIDTH_H
