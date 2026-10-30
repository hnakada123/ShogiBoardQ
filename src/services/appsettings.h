#ifndef APPSETTINGS_H
#define APPSETTINGS_H

/// @file appsettings.h
/// @brief アプリケーション全般設定の永続化
///
/// 言語・UI状態・駒音・メニューウィンドウ・メインウィンドウに関する設定を提供します。
/// 対局画面の外観（駒のセット・配色・見た目）と外観ダイアログの設定は style の AppearanceSettings。

#include <QString>
#include <QStringList>
#include <QSize>
#include "piecesoundtone.h"
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

// --- メニューウィンドウ ---

/// メニューウィンドウのお気に入りアクションリスト
inline const Setting<QStringList> menuWindowFavorites{"MenuWindow/favorites", QStringList()};

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
