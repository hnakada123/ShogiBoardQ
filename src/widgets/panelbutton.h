#ifndef PANELBUTTON_H
#define PANELBUTTON_H

/// @file panelbutton.h
/// @brief ドックのパネルの小さなボタン（アイコン・短い文字）の見た目と大きさ

#include <QSize>
#include <QString>

class QAbstractButton;

/**
 * @brief ドックのパネルのツールバーに置く小さなボタンの見た目と大きさ（全パネルで共通）
 *
 * 大きさは 32×24 を基本に、文字が収まるように広げる（長い文字・大きな書体）。書体が変わると測り直す。
 * A-/A+ は FontSizeControl がこれで整える。パネルはボタンの大きさを自分で決めない。
 */
namespace PanelButton {

/// 基本の大きさ（アイコン・2文字までの文字のボタン）
inline constexpr QSize kSize{32, 24};
/// アイコンの大きさ
inline constexpr QSize kIconSize{18, 18};

/// button をパネルのボタンにする（スタイルは styleSheet、空なら ButtonStyles::panelToolButton。アイコンの大きさ・
/// 文字が収まる大きさを付け、書体が変わったら大きさを測り直す）
void setup(QAbstractButton* button, const QString& styleSheet = QString());

/// 文字が収まる大きさ（基本の大きさより小さくしない。アイコンだけのボタンは基本の大きさ）
QSize sizeFor(const QAbstractButton* button);

} // namespace PanelButton

#endif // PANELBUTTON_H
