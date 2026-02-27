#ifndef ENGINEDIALOGSETTINGS_H
#define ENGINEDIALOGSETTINGS_H

/// @file enginedialogsettings.h
/// @brief エンジン設定ダイアログの永続化
///
/// エンジン設定変更・エンジン登録ダイアログに関する設定を提供します。
/// 呼び出し元: changeenginesettingsdialog.cpp, engineregistrationdialog.cpp
///
/// @note namespace 名は EngineDialogSettings（engine/enginesettingscoordinator.h との名前衝突を回避）

#include <QSize>

namespace EngineDialogSettings {

/// エンジン設定ダイアログのフォントサイズ（デフォルト: 12）
int engineSettingsFontSize();
void setEngineSettingsFontSize(int size);

/// エンジン設定ダイアログのウィンドウサイズ（デフォルト: 800x800）
QSize engineSettingsDialogSize();
void setEngineSettingsDialogSize(const QSize& size);

/// エンジン登録ダイアログのフォントサイズ（デフォルト: 12）
int engineRegistrationFontSize();
void setEngineRegistrationFontSize(int size);

/// エンジン登録ダイアログのウィンドウサイズ（デフォルト: 647x421）
QSize engineRegistrationDialogSize();
void setEngineRegistrationDialogSize(const QSize& size);

} // namespace EngineDialogSettings

#endif // ENGINEDIALOGSETTINGS_H
