#ifndef APPEARANCESETTINGS_H
#define APPEARANCESETTINGS_H

/// @file appearancesettings.h
/// @brief 対局画面の外観（駒のセット・配色・見た目）と外観ダイアログの設定の永続化
///
/// 外観の値の型（BoardColors・BoardVisuals・PieceStyleCatalog）と同じ style の層に置く。
/// 外観を読み書きするのは BoardAppearance（変更を知らせる）で、ほかの部品はそれを使う。

#include <QSize>
#include <QString>

#include "boardcolors.h"
#include "boardvisuals.h"
#include "piecestylecatalog.h"
#include "setting.h"

namespace AppearanceSettings {

// --- 外観 ---

/// 駒画像の種類（PieceStyleCatalog の ID。未設定は standard、旧セット・不正値も standard へ移行）
QString pieceStyle();
void setPieceStyle(const QString& style);

/// 盤面の配色（不正な色は標準色に戻す）
BoardColors boardColors();
void setBoardColors(const BoardColors& colors);
/// 木目・駒の影・駒の大きさ
BoardVisuals boardVisuals();
void setBoardVisuals(const BoardVisuals& visuals);

// --- 外観ダイアログ ---

/// 外観ダイアログの文字の大きさ（0 は元の大きさ）
inline const Setting<int> boardColorsDialogFontSize{"DialogAppearance/boardColors/fontSize", 0};
/// 色の選択ダイアログの文字の大きさ（0 は元の大きさ）
inline const Setting<int> colorPickerDialogFontSize{"DialogAppearance/colorPicker/fontSize", 0};
/// 「詳細」の中で開いていたタブの番号
inline const Setting<int> boardColorDialogTab{"UI/boardColorDialogTab", 5};
/// 外観ダイアログで開いていたタブの id（外観ダイアログが決める。旧版のタブの番号は id に読み替える）
QString appearanceSection();
void setAppearanceSection(const QString& id);
/// 外観ダイアログの駒の絞り込み（PieceStyleCatalog::Family の値。0 は「すべて」）
inline const BoundedSetting<int> appearancePieceFilter{
    "UI/appearancePieceFilter", 0, 0, static_cast<int>(PieceStyleCatalog::Family::Count) - 1};
inline const Setting<QSize> boardColorDialogSize{"SizeRelated/boardColorDialogSize", QSize(1180, 800)};
inline const Setting<QSize> boardColorPickerSize{"SizeRelated/boardColorPickerSize", QSize()};

} // namespace AppearanceSettings

#endif // APPEARANCESETTINGS_H
