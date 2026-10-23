#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

/// @file gamesettings.h
/// @brief 対局・棋譜設定の永続化
///
/// 棋譜ファイルI/O・棋譜欄・対局開始・棋譜貼り付け・局面集・持将棋に関する設定を提供します。

#include <QString>
#include <QStringList>
#include <QSize>
#include "setting.h"

namespace GameSettings {

// --- 棋譜ファイルI/O ---

/// 最後に開いた棋譜ファイルのディレクトリ
inline const Setting<QString> lastKifuDirectory{"General/lastKifuDirectory", QString()};

/// 最後に保存した棋譜ファイルのディレクトリ
inline const Setting<QString> lastKifuSaveDirectory{"General/lastKifuSaveDirectory", QString()};

// --- 棋譜欄 ---

/// 棋譜欄・分岐候補欄のフォントサイズ（デフォルト: 10）
inline const Setting<int> kifuPaneFontSize{"FontSize/kifuPane", 10};

/// 棋譜欄の消費時間列の表示状態（デフォルト: true）
inline const Setting<bool> kifuTimeColumnVisible{"RecordPane/timeColumnVisible", true};

/// 棋譜欄のしおり列の表示状態（デフォルト: true）
inline const Setting<bool> kifuBookmarkColumnVisible{"RecordPane/bookmarkColumnVisible", true};

/// 棋譜欄のコメント列の表示状態（デフォルト: true）
inline const Setting<bool> kifuCommentColumnVisible{"RecordPane/commentColumnVisible", true};

/// 分岐候補欄の開閉状態（デフォルト: 展開）
inline const Setting<bool> kifuBranchExpanded{"RecordPane/branchExpanded", true};

// --- メインタブ（コメント・対局情報） ---

/// 棋譜コメントタブのフォントサイズ（デフォルト: 10）
inline const Setting<int> commentFontSize{"FontSize/comment", 10};

/// 対局情報タブのフォントサイズ（デフォルト: 10）
inline const Setting<int> gameInfoFontSize{"FontSize/gameInfo", 10};

/// 対局情報の項目列幅（0: 内容に合わせて自動調整。最大 600）
inline const BoundedSetting<int> gameInfoKeyColumnWidth{"GameInfo/keyColumnWidth", 0, 0, 600};

// --- 対局開始 ---

/// 対局開始ダイアログのウィンドウサイズ（デフォルト: 1000x580）
inline const Setting<QSize> startGameDialogSize{"SizeRelated/startGameDialogSize", QSize(1000, 580)};

/// 対局開始ダイアログのフォントサイズ（デフォルト: 9）
inline const Setting<int> startGameDialogFontSize{"FontSize/startGameDialog", 9};

// --- 対局開始ダイアログの対局条件（前回の条件を次の対局で復元する） ---

namespace StartGame {

/// 対局者ごとの項目。1 は先手／下手、2 は後手／上手
struct Player {
    Setting<bool> isHuman;
    Setting<bool> isEngine;          ///< isHuman の反対。旧版との互換のため保存だけする
    Setting<QString> humanName;      ///< 未保存なら読む側で翻訳した「You」を補う
    Setting<int> engineNumber;       ///< 以前の版の登録一覧の番号（名前が無いときだけ読む）
    Setting<QString> engineName;     ///< エンジンの名前（id name）
    Setting<QString> enginePath;     ///< 以前の版の実行ファイルのパス（名前が無いときだけ読む）
    Setting<int> basicTimeHour;
    Setting<int> basicTimeMinutes;
    Setting<int> byoyomiSec;
    Setting<int> addEachMoveSec;
};

/// 対局者 number（'1' か '2'）の項目（キーは "GameSettings/<項目><番号>"）。human は人間が指すかの既定値
inline Player playerSettings(char number, bool human)
{
    const auto key = [number](const char* name) { return QByteArray("GameSettings/") + name + number; };
    return {{key("isHuman"), human}, {key("isEngine"), !human},
            {key("humanName"), QString()}, {key("engineNumber"), 0},
            {key("engineName"), QString()}, {key("enginePath"), QString()},
            {key("basicTimeHour"), 0}, {key("basicTimeMinutes"), 0},
            {key("byoyomiSec"), 0}, {key("addEachMoveSec"), 0}};
}

inline const Player player1 = playerSettings('1', true);
inline const Player player2 = playerSettings('2', false);

/// 後手／上手の持ち時間を先手と別に設定するか
inline const Setting<bool> separateSecondPlayerTime{"GameSettings/isGroupBoxSecondPlayerTimeSettingsChecked", false};

/// 開始局面の手合割の名前（Handicaps の名前。空なら現在の局面。デフォルト: 平手）
inline const Setting<QString> startingPositionName{"GameSettings/startingPositionName", QStringLiteral("平手")};
/// 以前の版の開始局面の番号（0=現在の局面、1..14=以前の一覧の並び）。読むだけで、保存すると消す
inline const Setting<int> startingPositionNumber{"GameSettings/startingPositionNumber", 1};

/// 最大手数（デフォルト: 1000）
inline const Setting<int> maxMoves{"GameSettings/maxMoves", 1000};

/// 連続対局数（エンジン同士のとき。デフォルト: 1）
inline const Setting<int> consecutiveGames{"GameSettings/consecutiveGames", 1};

/// 人間を手前に表示するか（デフォルト: true）
inline const Setting<bool> showHumanInFront{"GameSettings/isShowHumanInFront", true};

/// 終局時に棋譜を自動保存するか（デフォルト: false）
inline const Setting<bool> autoSaveKifu{"GameSettings/isAutoSaveKifu", false};

/// 自動保存先。未保存なら読む側でドキュメントフォルダを補う
inline const Setting<QString> kifuSaveDir{"GameSettings/kifuSaveDir", QString()};

/// 時間切れを負けにするか（デフォルト: true）
inline const Setting<bool> loseOnTimeout{"GameSettings/isLoseOnTimeout", true};

/// 連続対局で1局ごとに手番を入れ替えるか（デフォルト: false）
inline const Setting<bool> switchTurnEachGame{"GameSettings/isSwitchTurnEachGame", false};

/// 持将棋の判定規則（NyugyokuJudgement::Rule。デフォルト: 0 = なし）
inline const Setting<int> jishogiRule{"GameSettings/jishogiRule", 0};

} // namespace StartGame

// --- 棋譜貼り付け ---

/// 棋譜貼り付けダイアログのウィンドウサイズ（デフォルト: 600x500）
inline const Setting<QSize> kifuPasteDialogSize{"SizeRelated/kifuPasteDialogSize", QSize(600, 500)};

/// 棋譜貼り付けダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> kifuPasteDialogFontSize{"FontSize/kifuPasteDialog", 10};

// --- 局面集 ---

/// 局面集ビューアの最近使ったファイルリスト（最大5件）
inline const Setting<QStringList> sfenCollectionRecentFiles{"SfenCollection/recentFiles", QStringList()};

/// 局面集ビューアの将棋盤マスサイズ（デフォルト: 50）
inline const Setting<int> sfenCollectionSquareSize{"SfenCollection/squareSize", 50};

/// 局面集ビューアの最後に開いたディレクトリ
inline const Setting<QString> sfenCollectionLastDirectory{"SfenCollection/lastDirectory", QString()};

// --- 持将棋 ---

/// 持将棋の点数ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> jishogiScoreFontSize{"FontSize/jishogiScore", 10};

/// 持将棋の点数ダイアログのウィンドウサイズ（デフォルト: 660x640）
inline const Setting<QSize> jishogiScoreDialogSize{"JishogiScore/dialogSize", QSize(660, 640)};

} // namespace GameSettings

#endif // GAMESETTINGS_H
