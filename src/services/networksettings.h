#ifndef NETWORKSETTINGS_H
#define NETWORKSETTINGS_H

/// @file networksettings.h
/// @brief CSAネットワーク設定の永続化
///
/// CSA通信対局・待機・ログに関する設定を提供します。
/// 呼び出し元: csawaitingdialog.cpp, csagamedialog.cpp, engineanalysistab.cpp（CSAログタブ共用）

#include <QSize>

namespace NetworkSettings {

/// CSA通信ログのフォントサイズ（デフォルト: 10）
int csaLogFontSize();
void setCsaLogFontSize(int size);

/// CSA待機ダイアログのフォントサイズ（デフォルト: 10）
int csaWaitingDialogFontSize();
void setCsaWaitingDialogFontSize(int size);

/// CSA通信対局ダイアログのフォントサイズ（デフォルト: 10）
int csaGameDialogFontSize();
void setCsaGameDialogFontSize(int size);

/// CSA通信ログウィンドウのサイズ（デフォルト: 550x450）
QSize csaLogWindowSize();
void setCsaLogWindowSize(const QSize& size);

/// CSA通信対局ダイアログのウィンドウサイズ（デフォルト: 450x380）
QSize csaGameDialogSize();
void setCsaGameDialogSize(const QSize& size);

} // namespace NetworkSettings

#endif // NETWORKSETTINGS_H
