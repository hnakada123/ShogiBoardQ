#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

/// @file gamesettings.h
/// @brief 対局・棋譜設定の永続化
///
/// 棋譜ファイルI/O・棋譜欄・対局開始・棋譜貼り付け・局面集・持将棋に関する設定を提供します。
/// 呼び出し元: kifufilecontroller.cpp, kifusavecoordinator.cpp, recordpane.cpp,
///             startgamedialog.cpp, kifupastedialog.cpp, sfencollectiondialog.cpp,
///             jishogiscoredialogcontroller.cpp, commenteditorpanel.cpp, gameinfopanecontroller.cpp

#include <QString>
#include <QStringList>
#include <QSize>

namespace GameSettings {

// --- 棋譜ファイルI/O ---

/// 最後に開いた棋譜ファイルのディレクトリ
QString lastKifuDirectory();
void setLastKifuDirectory(const QString& dir);

/// 最後に保存した棋譜ファイルのディレクトリ
QString lastKifuSaveDirectory();
void setLastKifuSaveDirectory(const QString& dir);

// --- 棋譜欄 ---

/// 棋譜欄・分岐候補欄のフォントサイズ（デフォルト: 10）
int kifuPaneFontSize();
void setKifuPaneFontSize(int size);

/// 棋譜欄の消費時間列の表示状態（デフォルト: true）
bool kifuTimeColumnVisible();
void setKifuTimeColumnVisible(bool visible);

/// 棋譜欄のしおり列の表示状態（デフォルト: true）
bool kifuBookmarkColumnVisible();
void setKifuBookmarkColumnVisible(bool visible);

/// 棋譜欄のコメント列の表示状態（デフォルト: true）
bool kifuCommentColumnVisible();
void setKifuCommentColumnVisible(bool visible);

// --- メインタブ（コメント・対局情報） ---

/// 棋譜コメントタブのフォントサイズ（デフォルト: 10）
int commentFontSize();
void setCommentFontSize(int size);

/// 対局情報タブのフォントサイズ（デフォルト: 10）
int gameInfoFontSize();
void setGameInfoFontSize(int size);

// --- 対局開始 ---

/// 対局開始ダイアログのウィンドウサイズ（デフォルト: 1000x580）
QSize startGameDialogSize();
void setStartGameDialogSize(const QSize& size);

/// 対局開始ダイアログのフォントサイズ（デフォルト: 9）
int startGameDialogFontSize();
void setStartGameDialogFontSize(int size);

// --- 棋譜貼り付け ---

/// 棋譜貼り付けダイアログのウィンドウサイズ（デフォルト: 600x500）
QSize kifuPasteDialogSize();
void setKifuPasteDialogSize(const QSize& size);

/// 棋譜貼り付けダイアログのフォントサイズ（デフォルト: 10）
int kifuPasteDialogFontSize();
void setKifuPasteDialogFontSize(int size);

// --- 局面集 ---

/// 局面集ビューアのサイズ（デフォルト: 620x780）
QSize sfenCollectionDialogSize();
void setSfenCollectionDialogSize(const QSize& size);

/// 局面集ビューアの最近使ったファイルリスト（最大5件）
QStringList sfenCollectionRecentFiles();
void setSfenCollectionRecentFiles(const QStringList& files);

/// 局面集ビューアの将棋盤マスサイズ（デフォルト: 50）
int sfenCollectionSquareSize();
void setSfenCollectionSquareSize(int size);

/// 局面集ビューアの最後に開いたディレクトリ
QString sfenCollectionLastDirectory();
void setSfenCollectionLastDirectory(const QString& dir);

// --- 持将棋 ---

/// 持将棋の点数ダイアログのフォントサイズ（デフォルト: 10）
int jishogiScoreFontSize();
void setJishogiScoreFontSize(int size);

/// 持将棋の点数ダイアログのウィンドウサイズ（デフォルト: 250x280）
QSize jishogiScoreDialogSize();
void setJishogiScoreDialogSize(const QSize& size);

} // namespace GameSettings

#endif // GAMESETTINGS_H
