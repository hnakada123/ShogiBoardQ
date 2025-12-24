#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

#include <QString>
#include <QSize>

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

// ★ 追加: フォントサイズ関連
// USI通信ログタブのフォントサイズを取得（デフォルト: 10）
int usiLogFontSize();
// USI通信ログタブのフォントサイズを保存
void setUsiLogFontSize(int size);

// 棋譜コメントタブのフォントサイズを取得（デフォルト: 10）
int commentFontSize();
// 棋譜コメントタブのフォントサイズを保存
void setCommentFontSize(int size);

// 対局情報タブのフォントサイズを取得（デフォルト: 10）
int gameInfoFontSize();
// 対局情報タブのフォントサイズを保存
void setGameInfoFontSize(int size);

// 思考タブのフォントサイズを取得（デフォルト: 10）
int thinkingFontSize();
// 思考タブのフォントサイズを保存
void setThinkingFontSize(int size);

// ★ 追加: 最後に選択されたタブインデックスを取得（デフォルト: 0＝対局情報）
int lastSelectedTabIndex();
// ★ 追加: 最後に選択されたタブインデックスを保存
void setLastSelectedTabIndex(int index);

// ★ 追加: 読み筋表示ウィンドウのサイズを取得（デフォルト: 620x720）
QSize pvBoardDialogSize();
// ★ 追加: 読み筋表示ウィンドウのサイズを保存
void setPvBoardDialogSize(const QSize& size);

// ★ 追加: 評価値グラフの設定
// 評価値上限を取得（デフォルト: 2000）
int evalChartYLimit();
// 評価値上限を保存
void setEvalChartYLimit(int limit);

// 手数上限を取得（デフォルト: 500）
int evalChartXLimit();
// 手数上限を保存
void setEvalChartXLimit(int limit);

// 手数間隔を取得（デフォルト: 10）
int evalChartXInterval();
// 手数間隔を保存
void setEvalChartXInterval(int interval);

// 軸ラベルフォントサイズを取得（デフォルト: 7）
int evalChartLabelFontSize();
// 軸ラベルフォントサイズを保存
void setEvalChartLabelFontSize(int size);

} // namespace SettingsService

#endif // SETTINGSSERVICE_H
