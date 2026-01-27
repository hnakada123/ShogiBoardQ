#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

#include <QString>
#include <QSize>
#include <QByteArray>

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

// 棋譜解析結果ウィンドウのサイズを取得（デフォルト: 1100x600）
QSize kifuAnalysisResultsWindowSize();
// 棋譜解析結果ウィンドウのサイズを保存
void setKifuAnalysisResultsWindowSize(const QSize& size);

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

// 定跡ウィンドウのSFEN表示フォントサイズを取得（デフォルト: 9）
int josekiWindowSfenFontSize();
// 定跡ウィンドウのSFEN表示フォントサイズを保存
void setJosekiWindowSfenFontSize(int size);

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

// 定跡ウィンドウの最近使ったファイルリストを取得（最大5件）
QStringList josekiWindowRecentFiles();
// 定跡ウィンドウの最近使ったファイルリストを保存
void setJosekiWindowRecentFiles(const QStringList& files);

// 定跡手編集ダイアログのフォントサイズを取得（デフォルト: 10）
int josekiMoveDialogFontSize();
// 定跡手編集ダイアログのフォントサイズを保存
void setJosekiMoveDialogFontSize(int size);

// ★ 追加: CSA通信ログのフォントサイズを取得（デフォルト: 10）
int csaLogFontSize();
// ★ 追加: CSA通信ログのフォントサイズを保存
void setCsaLogFontSize(int size);

// ★ 追加: CSA待機ダイアログのフォントサイズを取得（デフォルト: 10）
int csaWaitingDialogFontSize();
// ★ 追加: CSA待機ダイアログのフォントサイズを保存
void setCsaWaitingDialogFontSize(int size);

// ★ 追加: CSA通信対局ダイアログのフォントサイズを取得（デフォルト: 10）
int csaGameDialogFontSize();
// ★ 追加: CSA通信対局ダイアログのフォントサイズを保存
void setCsaGameDialogFontSize(int size);

// ★ 追加: 持将棋の点数ダイアログのフォントサイズを取得（デフォルト: 10）
int jishogiScoreFontSize();
// ★ 追加: 持将棋の点数ダイアログのフォントサイズを保存
void setJishogiScoreFontSize(int size);

// ★ 追加: 持将棋の点数ダイアログのウィンドウサイズを取得（デフォルト: 250x280）
QSize jishogiScoreDialogSize();
// ★ 追加: 持将棋の点数ダイアログのウィンドウサイズを保存
void setJishogiScoreDialogSize(const QSize& size);

// ★ 追加: 言語設定
// 言語コードを取得（デフォルト: "system"）
// "system" = システムロケールに従う
// "ja_JP" = 日本語
// "en" = 英語
QString language();
// 言語コードを保存
void setLanguage(const QString& lang);

// ★ 追加: メニューウィンドウ設定
// メニューウィンドウのお気に入りアクションリストを取得
QStringList menuWindowFavorites();
// メニューウィンドウのお気に入りアクションリストを保存
void setMenuWindowFavorites(const QStringList& favorites);

// メニューウィンドウのサイズを取得（デフォルト: 500x400）
QSize menuWindowSize();
// メニューウィンドウのサイズを保存
void setMenuWindowSize(const QSize& size);

// メニューウィンドウのボタンサイズを取得（デフォルト: 72）
int menuWindowButtonSize();
// メニューウィンドウのボタンサイズを保存
void setMenuWindowButtonSize(int size);

// メニューウィンドウのフォントサイズを取得（デフォルト: 9）
int menuWindowFontSize();
// メニューウィンドウのフォントサイズを保存
void setMenuWindowFontSize(int size);

// ★ 追加: ツールバー表示設定
// ツールバーの表示状態を取得（デフォルト: true = 表示）
bool toolbarVisible();
// ツールバーの表示状態を保存
void setToolbarVisible(bool visible);

// ★ 追加: エンジン設定ダイアログのフォントサイズを取得（デフォルト: 12）
int engineSettingsFontSize();
// ★ 追加: エンジン設定ダイアログのフォントサイズを保存
void setEngineSettingsFontSize(int size);

// ★ 追加: エンジン設定ダイアログのウィンドウサイズを取得（デフォルト: 800x800）
QSize engineSettingsDialogSize();
// ★ 追加: エンジン設定ダイアログのウィンドウサイズを保存
void setEngineSettingsDialogSize(const QSize& size);

// ★ 追加: エンジン登録ダイアログのフォントサイズを取得（デフォルト: 12）
int engineRegistrationFontSize();
// ★ 追加: エンジン登録ダイアログのフォントサイズを保存
void setEngineRegistrationFontSize(int size);

// ★ 追加: エンジン登録ダイアログのウィンドウサイズを取得（デフォルト: 647x421）
QSize engineRegistrationDialogSize();
// ★ 追加: エンジン登録ダイアログのウィンドウサイズを保存
void setEngineRegistrationDialogSize(const QSize& size);

// ★ 追加: 検討ダイアログのフォントサイズを取得（デフォルト: 10）
int considerationDialogFontSize();
// ★ 追加: 検討ダイアログのフォントサイズを保存
void setConsiderationDialogFontSize(int size);

// ★ 追加: 検討ダイアログの最後に選択したエンジン番号を取得（デフォルト: 0）
int considerationEngineIndex();
// ★ 追加: 検討ダイアログの最後に選択したエンジン番号を保存
void setConsiderationEngineIndex(int index);

// ★ 追加: 検討ダイアログの時間無制限フラグを取得（デフォルト: true）
bool considerationUnlimitedTime();
// ★ 追加: 検討ダイアログの時間無制限フラグを保存
void setConsiderationUnlimitedTime(bool unlimited);

// ★ 追加: 検討ダイアログの検討時間（秒）を取得（デフォルト: 0）
int considerationByoyomiSec();
// ★ 追加: 検討ダイアログの検討時間（秒）を保存
void setConsiderationByoyomiSec(int sec);

// ★ 追加: 検討の候補手の数を取得（デフォルト: 1）
int considerationMultiPV();
// ★ 追加: 検討の候補手の数を保存
void setConsiderationMultiPV(int multiPV);

// ★ 追加: 検討タブのフォントサイズを取得（デフォルト: 10）
int considerationFontSize();
// ★ 追加: 検討タブのフォントサイズを保存
void setConsiderationFontSize(int size);

// ★ 追加: 評価値グラフドックの状態（QMainWindow::saveState()のバイト列）
QByteArray evalChartDockState();
void setEvalChartDockState(const QByteArray& state);

// ★ 追加: 評価値グラフドックのフローティング状態
bool evalChartDockFloating();
void setEvalChartDockFloating(bool floating);

// ★ 追加: 評価値グラフドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray evalChartDockGeometry();
void setEvalChartDockGeometry(const QByteArray& geometry);

// ★ 追加: 評価値グラフドックの表示状態
bool evalChartDockVisible();
void setEvalChartDockVisible(bool visible);

// ★ 追加: 棋譜欄ドックのフローティング状態
bool recordPaneDockFloating();
void setRecordPaneDockFloating(bool floating);

// ★ 追加: 棋譜欄ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray recordPaneDockGeometry();
void setRecordPaneDockGeometry(const QByteArray& geometry);

// ★ 追加: 棋譜欄ドックの表示状態
bool recordPaneDockVisible();
void setRecordPaneDockVisible(bool visible);

// ★ 追加: 解析タブドックのフローティング状態
bool analysisTabDockFloating();
void setAnalysisTabDockFloating(bool floating);

// ★ 追加: 解析タブドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray analysisTabDockGeometry();
void setAnalysisTabDockGeometry(const QByteArray& geometry);

// ★ 追加: 解析タブドックの表示状態
bool analysisTabDockVisible();
void setAnalysisTabDockVisible(bool visible);

// ★ 追加: 将棋盤ドックのフローティング状態
bool boardDockFloating();
void setBoardDockFloating(bool floating);

// ★ 追加: 将棋盤ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray boardDockGeometry();
void setBoardDockGeometry(const QByteArray& geometry);

// ★ 追加: 将棋盤ドックの表示状態
bool boardDockVisible();
void setBoardDockVisible(bool visible);

// ★ 追加: メニューウィンドウドックのフローティング状態
bool menuWindowDockFloating();
void setMenuWindowDockFloating(bool floating);

// ★ 追加: メニューウィンドウドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray menuWindowDockGeometry();
void setMenuWindowDockGeometry(const QByteArray& geometry);

// ★ 追加: メニューウィンドウドックの表示状態
bool menuWindowDockVisible();
void setMenuWindowDockVisible(bool visible);

} // namespace SettingsService

#endif // SETTINGSSERVICE_H
