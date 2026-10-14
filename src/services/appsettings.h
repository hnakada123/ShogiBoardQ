#ifndef APPSETTINGS_H
#define APPSETTINGS_H

/// @file appsettings.h
/// @brief アプリケーション全般設定の永続化
///
/// 言語・UI状態・駒音・メニューウィンドウ・メインウィンドウに関する設定を提供します。
/// 呼び出し元: main.cpp, mainwindowuibootstrapper.cpp, mainwindowlifecyclewiring.cpp,
///             mainwindowappearancecontroller.cpp, languagecontroller.cpp, menuwindow.cpp,
///             playerinfowiring.cpp

#include <QString>
#include <QStringList>
#include <QSize>
#include "boardcolors.h"
#include "boardvisuals.h"
#include "piecesoundtone.h"
#include "setting.h"

class QWidget;

namespace AppSettings {

/// 個別の設定項目を持たない補助ダイアログの表示設定。id は固定の識別子。
int dialogFontSize(const QString& id, int defaultSize);
void setDialogFontSize(const QString& id, int size);
QSize auxiliaryDialogSize(const QString& id);
void setAuxiliaryDialogSize(const QString& id, const QSize& size);

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

/// 最後に選択されたタブインデックス（デフォルト: 0＝対局情報）
inline const Setting<int> lastSelectedTabIndex{"UI/lastSelectedTabIndex", 0};

/// ツールバーの表示状態（デフォルト: true）
inline const Setting<bool> toolbarVisible{"UI/toolbarVisible", true};

/// 選択した駒の合法な移動先を表示（デフォルト: true）
inline const Setting<bool> legalMovesVisible{"UI/legalMovesVisible", true};

/// 選択可能な駒画像の種類（先頭は standard）
QStringList availablePieceStyles();
/// 駒画像の種類（未設定は standard、旧セット・不正値も standard へ移行）
QString pieceStyle();
void setPieceStyle(const QString& style);

/// 駒音（駒を指したときの効果音）の有効/無効（デフォルト: true）
inline const Setting<bool> pieceSoundEnabled{"Sound/pieceSoundEnabled", true};

/// 駒音の音量 0〜100 %（デフォルト: 30。範囲外の値は丸める）
int pieceSoundVolume();
void setPieceSoundVolume(int percent);

/// 駒音の音質（音の高さ・3バンドEQ。デフォルト: すべて 0。範囲外の値は丸める）
PieceSoundTone pieceSoundTone();
void setPieceSoundTone(const PieceSoundTone& tone);

/// 盤面の配色（不正な色は標準色に戻す）
BoardColors boardColors();
void setBoardColors(const BoardColors& colors);
BoardVisuals boardVisuals();
void setBoardVisuals(const BoardVisuals& visuals);
inline const Setting<int> boardColorDialogTab{"UI/boardColorDialogTab", 5};
int appearanceSection();
void setAppearanceSection(int index);
int appearancePieceFilter();
void setAppearancePieceFilter(int index);
inline const Setting<QSize> boardColorDialogSize{"SizeRelated/boardColorDialogSize", QSize(1180, 800)};
inline const Setting<QSize> boardColorPickerSize{"SizeRelated/boardColorPickerSize", QSize()};

// --- メニューウィンドウ ---

/// メニューウィンドウのお気に入りアクションリスト
inline const Setting<QStringList> menuWindowFavorites{"MenuWindow/favorites", QStringList()};

/// メニューウィンドウのサイズ（デフォルト: 500x400）
inline const Setting<QSize> menuWindowSize{"MenuWindow/size", QSize(500, 400)};

/// メニューウィンドウのボタンサイズ（デフォルト: 104）
inline const Setting<int> menuWindowButtonSize{"MenuWindow/buttonSize", 104};

/// メニューウィンドウのフォントサイズ（デフォルト: 12）
inline const Setting<int> menuWindowFontSize{"MenuWindow/fontSize", 12};

/// メニューウィンドウで最後に選択したタブ
inline const Setting<int> menuWindowCurrentTab{"MenuWindow/currentTab", 0};

// --- メインウィンドウ ---

/// メインウィンドウのサイズ（デフォルト: 1100x720）
inline const Setting<QSize> mainWindowSize{"SizeRelated/mainWindowSize", QSize(1100, 720)};

/// 盤のマスの大きさ（ピクセル、デフォルト: 50）。範囲外の保存値は読む側で既定値へ戻す
inline const Setting<int> squareSize{"SizeRelated/squareSize", 50};

/// メインウィンドウのサイズを読み込んで適用
void loadWindowSize(QWidget* mainWindow);

/// メインウィンドウのサイズと盤マスサイズをINIに保存
void saveWindowAndBoard(QWidget* mainWindow, int squareSize);

} // namespace AppSettings

#endif // APPSETTINGS_H
