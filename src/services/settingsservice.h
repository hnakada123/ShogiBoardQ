#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

/// @file settingsservice.h
/// @brief アプリケーション設定の永続化サービス（互換アダプタ）
///
/// 全関数はドメイン別 namespace に移行済みです。
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
