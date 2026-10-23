#ifndef ENGINEDIALOGSETTINGS_H
#define ENGINEDIALOGSETTINGS_H

/// @file enginedialogsettings.h
/// @brief エンジン設定ダイアログの永続化
///
/// エンジン設定変更・エンジン登録ダイアログに関する設定を提供します。

#include <QSize>
#include "setting.h"

namespace EngineDialogSettings {

/// エンジン設定ダイアログのフォントサイズ（デフォルト: 12）
inline const Setting<int> engineSettingsFontSize{"FontSize/engineSettings", 12};

/// エンジン設定ダイアログのウィンドウサイズ（デフォルト: 800x800）
inline const Setting<QSize> engineSettingsDialogSize{"EngineSettings/dialogSize", QSize(800, 800)};

/// エンジン登録ダイアログのフォントサイズ（デフォルト: 12）
inline const Setting<int> engineRegistrationFontSize{"FontSize/engineRegistration", 12};

/// エンジン登録ダイアログのウィンドウサイズ（デフォルト: 647x421）
inline const Setting<QSize> engineRegistrationDialogSize{"EngineRegistration/dialogSize", QSize(647, 421)};

} // namespace EngineDialogSettings

#endif // ENGINEDIALOGSETTINGS_H
