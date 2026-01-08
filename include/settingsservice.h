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

// ★ 追加: エンジン情報ウィジェットの列幅を取得
// widgetIndex: 0=上段(エンジン1), 1=下段(エンジン2)
// 戻り値: 各列の幅のリスト（空の場合はデフォルト値を使用）
QList<int> engineInfoColumnWidths(int widgetIndex);
// ★ 追加: エンジン情報ウィジェットの列幅を保存
void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths);

// ★ 追加: 思考タブ下段（読み筋テーブル）の列幅を取得
// viewIndex: 0=エンジン1のテーブル, 1=エンジン2のテーブル
QList<int> thinkingViewColumnWidths(int viewIndex);
// ★ 追加: 思考タブ下段（読み筋テーブル）の列幅を保存
void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths);

// ★ 追加: 棋譜欄・分岐候補欄のフォントサイズを取得（デフォルト: 10）
int kifuPaneFontSize();
// ★ 追加: 棋譜欄・分岐候補欄のフォントサイズを保存
void setKifuPaneFontSize(int size);

// ★ 追加: 棋譜解析ダイアログの設定
// フォントサイズを取得（デフォルト: 10）
int kifuAnalysisFontSize();
// フォントサイズを保存
void setKifuAnalysisFontSize(int size);

// 1手あたりの思考時間（秒）を取得（デフォルト: 3）
int kifuAnalysisByoyomiSec();
// 1手あたりの思考時間（秒）を保存
void setKifuAnalysisByoyomiSec(int sec);

// 最後に選択したエンジン番号を取得（デフォルト: 0）
int kifuAnalysisEngineIndex();
// 最後に選択したエンジン番号を保存
void setKifuAnalysisEngineIndex(int index);

// 解析範囲: 全局面解析かどうかを取得（デフォルト: true）
bool kifuAnalysisFullRange();
// 解析範囲: 全局面解析かどうかを保存
void setKifuAnalysisFullRange(bool fullRange);

// 解析範囲: 開始手数を取得（デフォルト: 0）
int kifuAnalysisStartPly();
// 解析範囲: 開始手数を保存
void setKifuAnalysisStartPly(int ply);

// 解析範囲: 終了手数を取得（デフォルト: 0）
int kifuAnalysisEndPly();
// 解析範囲: 終了手数を保存
void setKifuAnalysisEndPly(int ply);

// ★ 追加: 定跡ウィンドウの設定
// 定跡ウィンドウのフォントサイズを取得（デフォルト: 10）
int josekiWindowFontSize();
// 定跡ウィンドウのフォントサイズを保存
void setJosekiWindowFontSize(int size);

// 定跡ウィンドウの最後に開いた定跡ファイルパスを取得
QString josekiWindowLastFilePath();
// 定跡ウィンドウの最後に開いた定跡ファイルパスを保存
void setJosekiWindowLastFilePath(const QString& path);

// 定跡ウィンドウのサイズを取得（デフォルト: 800x500）
QSize josekiWindowSize();
// 定跡ウィンドウのサイズを保存
void setJosekiWindowSize(const QSize& size);

// 定跡ファイル自動読込が有効かどうかを取得（デフォルト: true）
bool josekiWindowAutoLoadEnabled();
// 定跡ファイル自動読込が有効かどうかを保存
void setJosekiWindowAutoLoadEnabled(bool enabled);

} // namespace SettingsService

#endif // SETTINGSSERVICE_H
