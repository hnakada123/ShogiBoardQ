#ifndef AUTOMATIONINPUT_H
#define AUTOMATIONINPUT_H

/// @file automationinput.h
/// @brief 自動化 API が送る通常のマウス入力（クリックとドラッグ）

#include <QList>
#include <QPoint>
#include <Qt>

class QWidget;

namespace AutomationInput {

/// receiver の pos（receiver の座標）へ、移動・押す・離すの通常のマウス入力を送る
void sendClick(QWidget* receiver, const QPoint& pos, Qt::MouseButton button = Qt::LeftButton);

/// receiver の上で、globalPath の最初の点で押し、途中の点を通って最後の点で離す（座標はグローバル）
void sendDrag(QWidget* receiver, const QList<QPoint>& globalPath);

} // namespace AutomationInput

#endif // AUTOMATIONINPUT_H
