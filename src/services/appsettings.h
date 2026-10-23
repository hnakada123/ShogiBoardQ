#ifndef APPSETTINGS_H
#define APPSETTINGS_H

/// @file appsettings.h
/// @brief アプリケーション全般設定の永続化
///
/// 言語・UI状態・駒音・メニューウィンドウ・メインウィンドウに関する設定を提供します。

#include <QString>
#include <QStringList>
#include <QSize>
#include "boardcolors.h"
#include "boardvisuals.h"
#include "piecesoundtone.h"
#include "piecestylecatalog.h"
#include "setting.h"

namespace AppSettings {

// --- 補助ダイアログ（DialogFontScale）の文字の大きさと大きさ ---
// 文字の大きさの既定値 0 は「ダイアログの元の大きさ」。大きさは保存するダイアログだけ持つ
inline const Setting<int> messageBoxFontSize{"DialogAppearance/messageBox/fontSize", 0};
inline const Setting<int> bookmarkDialogFontSize{"DialogAppearance/bookmark/fontSize", 0};
inline const Setting<QSize> bookmarkDialogSize{"DialogAppearance/bookmark/size", QSize()};
inline const Setting<int> dockLayoutNameDialogFontSize{"DialogAppearance/dockLayoutName/fontSize", 0};
inline const Setting<QSize> dockLayoutNameDialogSize{"DialogAppearance/dockLayoutName/size", QSize()};
inline const Setting<int> pieceSoundDialogFontSize{"DialogAppearance/pieceSound/fontSize", 0};
inline const Setting<QSize> pieceSoundDialogSize{"DialogAppearance/pieceSound/size", QSize()};
inline const Setting<int> promotionDialogFontSize{"DialogAppearance/promotion/fontSize", 0};
inline const Setting<QSize> promotionDialogSize{"DialogAppearance/promotion/size", QSize()};
inline const Setting<int> boardColorsDialogFontSize{"DialogAppearance/boardColors/fontSize", 0};
inline const Setting<int> colorPickerDialogFontSize{"DialogAppearance/colorPicker/fontSize", 0};
inline const Setting<int> pvBoardDialogFontSize{"DialogAppearance/pvBoard/fontSize", 0};
inline const Setting<int> sfenCollectionDialogFontSize{"DialogAppearance/sfenCollection/fontSize", 0};
inline const Setting<int> versionDialogFontSize{"DialogAppearance/version/fontSize", 0};
inline const Setting<int> fontSettingsDialogFontSize{"DialogAppearance/fontSettings/fontSize", 0};
inline const Setting<int> evaluationChartDialogFontSize{"DialogAppearance/evaluationChart/fontSize", 0};

inline const Setting<QSize> versionDialogSize{"SizeRelated/versionDialogSize", QSize(680, 760)};
inline const Setting<int> versionDialogDocument{"UI/versionDialogDocument", 0};

// --- 言語 ---

/// 言語コード（デフォルト: "system"）
inline const Setting<QString> language{"General/language", "system"};
inline const Setting<QString> moveNotation{"General/moveNotation", "auto"};
inline const Setting<bool> notationOrigin{"General/notationOrigin", false};

// --- GUI共通フォント ---

/// 空文字列は標準の書体を使用する。
inline const Setting<QString> uiFontFamily{"General/uiFontFamily", QString()};

/// フォント設定ダイアログのウィンドウサイズ（デフォルト: 520x300）
inline const Setting<QSize> fontSettingsDialogSize{"SizeRelated/fontSettingsDialogSize", QSize(520, 300)};

// --- UI状態 ---

/// ツールバーの表示状態（デフォルト: true）
inline const Setting<bool> toolbarVisible{"UI/toolbarVisible", true};

/// 選択した駒の合法な移動先を表示（デフォルト: true）
inline const Setting<bool> legalMovesVisible{"UI/legalMovesVisible", true};

/// 駒画像の種類（PieceStyleCatalog の ID。未設定は standard、旧セット・不正値も standard へ移行）
QString pieceStyle();
void setPieceStyle(const QString& style);

/// 駒音（駒を指したときの効果音）の有効/無効（デフォルト: true）
inline const Setting<bool> pieceSoundEnabled{"Sound/pieceSoundEnabled", true};

/// 駒音の音量 0〜100 %（デフォルト: 30。範囲外の値は丸める）。標準の駒音（piece_move.wav）は既定の音量で聴くことを前提に調整されている
inline const BoundedSetting<int> pieceSoundVolume{"Sound/pieceSoundVolume", 30, 0, 100};

/// 駒音の音質（音の高さ・3バンドEQ。デフォルト: すべて 0。範囲外の値は丸める）
inline const StructSetting<PieceSoundTone> pieceSoundTone{
    {SettingField<PieceSoundTone>::of("Sound/pieceSoundPitch", &PieceSoundTone::pitchSemitones),
     SettingField<PieceSoundTone>::of("Sound/pieceSoundEqLow", &PieceSoundTone::lowDb),
     SettingField<PieceSoundTone>::of("Sound/pieceSoundEqMid", &PieceSoundTone::midDb),
     SettingField<PieceSoundTone>::of("Sound/pieceSoundEqHigh", &PieceSoundTone::highDb)},
    &PieceSoundTone::clamped};

/// 盤面の配色（不正な色は標準色に戻す）
BoardColors boardColors();
void setBoardColors(const BoardColors& colors);
BoardVisuals boardVisuals();
void setBoardVisuals(const BoardVisuals& visuals);
inline const Setting<int> boardColorDialogTab{"UI/boardColorDialogTab", 5};
/// 外観ダイアログで開いていたタブの id（外観ダイアログが決める。旧版のタブの番号は id に読み替える）
QString appearanceSection();
void setAppearanceSection(const QString& id);
/// 外観ダイアログの駒の絞り込み（PieceStyleCatalog::Family の値。0 は「すべて」）
inline const BoundedSetting<int> appearancePieceFilter{
    "UI/appearancePieceFilter", 0, 0, static_cast<int>(PieceStyleCatalog::Family::Count) - 1};
inline const Setting<QSize> boardColorDialogSize{"SizeRelated/boardColorDialogSize", QSize(1180, 800)};
inline const Setting<QSize> boardColorPickerSize{"SizeRelated/boardColorPickerSize", QSize()};

// --- メニューウィンドウ ---

/// メニューウィンドウのお気に入りアクションリスト
inline const Setting<QStringList> menuWindowFavorites{"MenuWindow/favorites", QStringList()};

/// メニューウィンドウのサイズ（デフォルト: 500x400）
inline const Setting<QSize> menuWindowSize{"MenuWindow/size", QSize(500, 400)};

/// メニューウィンドウのボタンサイズ（デフォルト: 104。48〜120）
inline const BoundedSetting<int> menuWindowButtonSize{"MenuWindow/buttonSize", 104, 48, 120};

/// メニューウィンドウのフォントサイズ（デフォルト: 12）
inline const Setting<int> menuWindowFontSize{"MenuWindow/fontSize", 12};

/// メニューウィンドウで最後に選択したタブ
inline const Setting<int> menuWindowCurrentTab{"MenuWindow/currentTab", 0};

// --- メインウィンドウ ---

/// メインウィンドウのサイズ（デフォルト: 1100x720）
inline const Setting<QSize> mainWindowSize{"SizeRelated/mainWindowSize", QSize(1100, 720)};

/// 盤のマスの大きさ（ピクセル、デフォルト: 50）。範囲外の保存値は読む側で既定値へ戻す
inline const Setting<int> squareSize{"SizeRelated/squareSize", 50};

} // namespace AppSettings

#endif // APPSETTINGS_H
