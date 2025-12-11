#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

#include <QString>

class QWidget;
class ShogiView;

namespace SettingsService {

// INI (ShogiBoardQ.ini) からメインウィンドウのサイズを読み込んで適用します。
// - mainWindow: 対象となる QMainWindow（や QWidget）
// 失敗時は何もしません（デフォルトサイズのまま）。
void loadWindowSize(QWidget* mainWindow);

// メインウィンドウのサイズと盤マスサイズ（ShogiView::squareSize）を INI に保存します。
// - mainWindow: 対象となる QMainWindow（や QWidget）
// - view      : 盤ビュー（nullptr の場合は何もしません）
void saveWindowAndBoard(QWidget* mainWindow, ShogiView* view);

// 最後に開いた棋譜ファイルのディレクトリを取得します。
// 未設定の場合は空文字列を返します。
QString lastKifuDirectory();

// 最後に開いた棋譜ファイルのディレクトリを保存します。
void setLastKifuDirectory(const QString& dir);

} // namespace SettingsService

#endif // SETTINGSSERVICE_H
