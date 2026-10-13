#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

/// @file settingsservice.h
/// @brief ドメイン別の設定ヘッダーをまとめて取り込むヘッダー
///
/// 設定項目はドメイン別ヘッダーに Setting<T>（setting.h）で1行ずつ宣言しています。
/// 新規コードはドメイン別ヘッダーを直接 include してください:
///   settingscommon.h, appsettings.h, docksettings.h, josekisettings.h,
///   analysissettings.h, gamesettings.h, enginedialogsettings.h,
///   networksettings.h, tsumeshogisettings.h

#include "settingscommon.h"
#include "docksettings.h"
#include "josekisettings.h"
#include "tsumeshogisettings.h"
#include "networksettings.h"
#include "enginedialogsettings.h"
#include "analysissettings.h"
#include "gamesettings.h"
#include "appsettings.h"

#endif // SETTINGSSERVICE_H
