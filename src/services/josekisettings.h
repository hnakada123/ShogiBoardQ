#ifndef JOSEKISETTINGS_H
#define JOSEKISETTINGS_H

/// @file josekisettings.h
/// @brief 定跡設定の永続化
///
/// 定跡ウィンドウ・定跡手編集・定跡マージに関する設定を提供します。
/// 呼び出し元: josekiwindow.cpp, josekimovedialog.cpp, josekimergedialog.cpp

#include <QString>
#include <QStringList>
#include <QSize>
#include <QList>
#include "josekiplayoptions.h"
#include "setting.h"

namespace JosekiSettings {

/// 定跡ウィンドウのフォントサイズ（デフォルト: 10）
inline const Setting<int> josekiWindowFontSize{"JosekiWindow/fontSize", 10};

/// 定跡ウィンドウの最後に開いた定跡ファイルパス
inline const Setting<QString> josekiWindowLastFilePath{"JosekiWindow/lastFilePath", QString()};

/// 定跡ウィンドウのサイズ（デフォルト: 800x500）
inline const Setting<QSize> josekiWindowSize{"JosekiWindow/size", QSize(800, 500)};

/// 定跡ファイル自動読込（デフォルト: true）
inline const Setting<bool> josekiWindowAutoLoadEnabled{"JosekiWindow/autoLoadEnabled", true};

/// 定跡ウィンドウの最近使ったファイルリスト（最大5件）
inline const Setting<QStringList> josekiWindowRecentFiles{"JosekiWindow/recentFiles", QStringList()};

/// 定跡ウィンドウの表示停止状態（デフォルト: true＝表示有効）
inline const Setting<bool> josekiWindowDisplayEnabled{"JosekiWindow/displayEnabled", true};

/// 定跡ウィンドウのSFEN詳細表示状態（デフォルト: false）
inline const Setting<bool> josekiWindowSfenDetailVisible{"JosekiWindow/sfenDetailVisible", false};

/// 定跡ウィンドウのテーブル列幅
inline const ListSetting<int> josekiWindowColumnWidths{QStringLiteral("JosekiWindow/columnWidths"), "width"};

/// 定跡手編集ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> josekiMoveDialogFontSize{"JosekiWindow/moveDialogFontSize", 10};

/// 定跡手編集ダイアログのウィンドウサイズ（デフォルト: 500x600）
inline const Setting<QSize> josekiMoveDialogSize{"SizeRelated/josekiMoveDialogSize", QSize(500, 600)};

/// 定跡マージダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> josekiMergeDialogFontSize{"JosekiWindow/mergeDialogFontSize", 10};

/// 定跡マージダイアログのウィンドウサイズ（デフォルト: 600x500）
inline const Setting<QSize> josekiMergeDialogSize{"JosekiWindow/mergeDialogSize", QSize(600, 500)};

/// 定跡を使う条件のダイアログのフォントサイズ（0 ならダイアログの元の大きさから始める）
inline const Setting<int> josekiPlayOptionsDialogFontSize{"JosekiWindow/playOptionsDialogFontSize", 0};

/// 定跡を使う条件のダイアログのウィンドウサイズ（デフォルト: 460x440）
inline const Setting<QSize> josekiPlayOptionsDialogSize{"SizeRelated/josekiPlayOptionsDialogSize", QSize(460, 440)};

/// 棋譜解析の結果をまとめて定跡に登録するダイアログのフォントサイズ（0 ならダイアログの元の大きさから始める）
inline const Setting<int> josekiAnalysisRegisterDialogFontSize{"JosekiWindow/analysisRegisterDialogFontSize", 0};

/// 棋譜解析の結果をまとめて定跡に登録するダイアログのウィンドウサイズ（デフォルト: 460x280）
inline const Setting<QSize> josekiAnalysisRegisterDialogSize{"SizeRelated/josekiAnalysisRegisterDialogSize", QSize(460, 280)};

/// 棋譜解析の結果をまとめて定跡に登録するとき、先手の手番の局面を登録する（デフォルト: true）
inline const Setting<bool> josekiAnalysisRegisterBlack{"JosekiWindow/analysisRegisterBlack", true};

/// 棋譜解析の結果をまとめて定跡に登録するとき、後手の手番の局面を登録する（デフォルト: true）
inline const Setting<bool> josekiAnalysisRegisterWhite{"JosekiWindow/analysisRegisterWhite", true};

/// 複数の棋譜から定跡に登録するダイアログのフォントサイズ（0 ならダイアログの元の大きさから始める）
inline const Setting<int> josekiImportDialogFontSize{"JosekiWindow/importDialogFontSize", 0};

/// 複数の棋譜から定跡に登録するダイアログのウィンドウサイズ（デフォルト: 600x560）
inline const Setting<QSize> josekiImportDialogSize{"SizeRelated/josekiImportDialogSize", QSize(600, 560)};

/// 複数の棋譜から登録する: この手数までの手を登録する（デフォルト: 60）
inline const Setting<int> josekiImportMaxPly{"JosekiImport/maxPly", 60};

/// 複数の棋譜から登録する: 登録する手（0=両方, 1=先手だけ, 2=後手だけ, 3=勝った側だけ。デフォルト: 0）
inline const Setting<int> josekiImportSides{"JosekiImport/sides", 0};

/// 複数の棋譜から登録する: 変化の手も登録する（デフォルト: false）
inline const Setting<bool> josekiImportVariations{"JosekiImport/includeVariations", false};

/// 複数の棋譜から登録する: この局数以上で指された手だけ登録する（デフォルト: 1）
inline const Setting<int> josekiImportMinCount{"JosekiImport/minCount", 1};

/// 複数の棋譜から登録する: 棋譜のコメントを定跡のコメントにする（デフォルト: false）
inline const Setting<bool> josekiImportComments{"JosekiImport/useComments", false};

/// 複数の棋譜から登録する: 最後にファイル・フォルダを選んだフォルダ
inline const Setting<QString> josekiImportLastDirectory{"JosekiImport/lastDirectory", QString()};

/// 定跡ファイルを足すダイアログのフォントサイズ（0 ならダイアログの元の大きさから始める）
inline const Setting<int> josekiBookMergeDialogFontSize{"JosekiWindow/bookMergeDialogFontSize", 0};

/// 定跡ファイルを足すダイアログのウィンドウサイズ（デフォルト: 600x300）
inline const Setting<QSize> josekiBookMergeDialogSize{"SizeRelated/josekiBookMergeDialogSize", QSize(600, 300)};

/// 定跡ファイルを足す: 同じ指し手は足す定跡の予想応手・評価値・深さ・コメントにする（デフォルト: false）
inline const Setting<bool> josekiBookMergePreferIncoming{"JosekiBookMerge/preferIncoming", false};

/// 定跡ファイルを足す: 同じ指し手の出現頻度を足し合わせる（デフォルト: true）
inline const Setting<bool> josekiBookMergeSumFrequency{"JosekiBookMerge/sumFrequency", true};

/// 盤に定跡手の矢印を出す（「表示」メニュー・定跡ウィンドウ。デフォルト: false）
inline const Setting<bool> boardArrowsVisible{"JosekiWindow/boardArrowsVisible", false};

/// 先手／下手の対局者が定跡を使う条件（対局開始ダイアログの対局者の欄）
inline const StructSetting<JosekiPlayOptions> playOptions1{
    {SettingField<JosekiPlayOptions>::of("JosekiPlay1/enabled", &JosekiPlayOptions::enabled),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/maxPly", &JosekiPlayOptions::maxPly),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/method", &JosekiPlayOptions::method),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/valueDiff", &JosekiPlayOptions::valueDiff),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/useMinValue", &JosekiPlayOptions::useMinValue),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/minValue", &JosekiPlayOptions::minValue),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/minDepth", &JosekiPlayOptions::minDepth),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/minRatioPercent", &JosekiPlayOptions::minRatioPercent),
     SettingField<JosekiPlayOptions>::of("JosekiPlay1/minFrequency", &JosekiPlayOptions::minFrequency)},
    &JosekiPlayOptions::normalized};

/// 後手／上手の対局者が定跡を使う条件
inline const StructSetting<JosekiPlayOptions> playOptions2{
    {SettingField<JosekiPlayOptions>::of("JosekiPlay2/enabled", &JosekiPlayOptions::enabled),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/maxPly", &JosekiPlayOptions::maxPly),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/method", &JosekiPlayOptions::method),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/valueDiff", &JosekiPlayOptions::valueDiff),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/useMinValue", &JosekiPlayOptions::useMinValue),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/minValue", &JosekiPlayOptions::minValue),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/minDepth", &JosekiPlayOptions::minDepth),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/minRatioPercent", &JosekiPlayOptions::minRatioPercent),
     SettingField<JosekiPlayOptions>::of("JosekiPlay2/minFrequency", &JosekiPlayOptions::minFrequency)},
    &JosekiPlayOptions::normalized};

/// 対局者（1=先手／下手、2=後手／上手）が定跡を使う条件の保存先
inline const StructSetting<JosekiPlayOptions> &playOptions(int side)
{
    return side == 2 ? playOptions2 : playOptions1;
}

} // namespace JosekiSettings

#endif // JOSEKISETTINGS_H
