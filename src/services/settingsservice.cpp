/// @file settingsservice.cpp
/// @brief アプリケーション設定の永続化サービスの実装

#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QWidget>
#include <QStandardPaths>
#include "settingskeys.h"
#include "shogiview.h"

namespace SettingsService {

static const char* kIniName = "ShogiBoardQ.ini";
QString settingsFilePath();

namespace {

QSettings& openSettings()
{
    static QSettings s(SettingsService::settingsFilePath(), QSettings::IniFormat);
    return s;
}

} // namespace

QString settingsFilePath()
{
    static QString path;
    if (path.isEmpty()) {
        const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configDir);
        path = configDir + "/" + kIniName;
    }
    return path;
}

void loadWindowSize(QWidget* mainWindow)
{
    if (!mainWindow) return;
    QSettings& s = openSettings();
    const QSize sz = s.value(SettingsKeys::kMainWindowSize, QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, ShogiView* view)
{
    if (!mainWindow || !view) return;
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMainWindowSize, mainWindow->size());
    s.setValue(SettingsKeys::kSquareSize,     view->squareSize());
    if (s.status() != QSettings::NoError) {
        qWarning("SettingsService: Failed to save settings (status=%d)", static_cast<int>(s.status()));
    }
}

QString lastKifuDirectory()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kLastKifuDirectory, QString()).toString();
}

void setLastKifuDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kLastKifuDirectory, dir);
}

QString lastKifuSaveDirectory()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kLastKifuSaveDirectory, QString()).toString();
}

void setLastKifuSaveDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kLastKifuSaveDirectory, dir);
}

// USI通信ログタブのフォントサイズを取得
int usiLogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeUsiLog, 10).toInt();
}

// USI通信ログタブのフォントサイズを保存
void setUsiLogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeUsiLog, size);
}

// 棋譜コメントタブのフォントサイズを取得
int commentFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeComment, 10).toInt();
}

// 棋譜コメントタブのフォントサイズを保存
void setCommentFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeComment, size);
}

// 対局情報タブのフォントサイズを取得
int gameInfoFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeGameInfo, 10).toInt();
}

// 対局情報タブのフォントサイズを保存
void setGameInfoFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeGameInfo, size);
}

// 思考タブのフォントサイズを取得
int thinkingFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeThinking, 10).toInt();
}

// 思考タブのフォントサイズを保存
void setThinkingFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeThinking, size);
}

// 最後に選択されたタブインデックスを取得
int lastSelectedTabIndex()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kLastSelectedTabIndex, 0).toInt();
}

// 最後に選択されたタブインデックスを保存
void setLastSelectedTabIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kLastSelectedTabIndex, index);
}

// 読み筋表示ウィンドウのサイズを取得
QSize pvBoardDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kPvBoardDialogSize, QSize(620, 720)).toSize();
}

// 読み筋表示ウィンドウのサイズを保存
void setPvBoardDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kPvBoardDialogSize, size);
}

// 局面集ビューアのサイズを取得
QSize sfenCollectionDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kSfenCollectionDialogSize, QSize(620, 780)).toSize();
}

// 局面集ビューアのサイズを保存
void setSfenCollectionDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kSfenCollectionDialogSize, size);
}

// 局面集ビューアの最近使ったファイルリストを取得
QStringList sfenCollectionRecentFiles()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kSfenCollectionRecentFiles, QStringList()).toStringList();
}

// 局面集ビューアの最近使ったファイルリストを保存
void setSfenCollectionRecentFiles(const QStringList& files)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kSfenCollectionRecentFiles, files);
}

// 局面集ビューアの将棋盤マスサイズを取得
int sfenCollectionSquareSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kSfenCollectionSquareSize, 50).toInt();
}

// 局面集ビューアの将棋盤マスサイズを保存
void setSfenCollectionSquareSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kSfenCollectionSquareSize, size);
}

// 局面集ビューアの最後に開いたディレクトリを取得
QString sfenCollectionLastDirectory()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kSfenCollectionLastDirectory, QString()).toString();
}

// 局面集ビューアの最後に開いたディレクトリを保存
void setSfenCollectionLastDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kSfenCollectionLastDirectory, dir);
}

// 評価値グラフの設定
int evalChartYLimit()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartYLimit, 2000).toInt();
}

void setEvalChartYLimit(int limit)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartYLimit, limit);
}

int evalChartXLimit()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartXLimit, 500).toInt();
}

void setEvalChartXLimit(int limit)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartXLimit, limit);
}

int evalChartXInterval()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartXInterval, 10).toInt();
}

void setEvalChartXInterval(int interval)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartXInterval, interval);
}

int evalChartLabelFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartLabelFontSize, 7).toInt();
}

void setEvalChartLabelFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartLabelFontSize, size);
}

// エンジン情報ウィジェットの列幅を取得
QList<int> engineInfoColumnWidths(int widgetIndex)
{
    QSettings& s = openSettings();
    QString key = QString(SettingsKeys::kEngineInfoColumnWidthsFmt).arg(widgetIndex);
    QList<int> widths;

    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}

// エンジン情報ウィジェットの列幅を保存
void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths)
{
    QSettings& s = openSettings();
    QString key = QString(SettingsKeys::kEngineInfoColumnWidthsFmt).arg(widgetIndex);

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    // 即座に書き込み
    s.sync();
}

// 思考タブ下段（読み筋テーブル）の列幅を取得
QList<int> thinkingViewColumnWidths(int viewIndex)
{
    QSettings& s = openSettings();
    QString key = QString(SettingsKeys::kThinkingViewColumnWidthsFmt).arg(viewIndex);
    QList<int> widths;

    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}

// 思考タブ下段（読み筋テーブル）の列幅を保存
void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths)
{
    QSettings& s = openSettings();
    QString key = QString(SettingsKeys::kThinkingViewColumnWidthsFmt).arg(viewIndex);

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    // 即座に書き込み
    s.sync();
}

// 棋譜欄・分岐候補欄のフォントサイズを取得
int kifuPaneFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeKifuPane, 10).toInt();
}

// 棋譜欄・分岐候補欄のフォントサイズを保存
void setKifuPaneFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeKifuPane, size);
}

// 棋譜解析ダイアログのフォントサイズを取得
int kifuAnalysisFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisFontSize, 0).toInt();
}

// 棋譜解析ダイアログのフォントサイズを保存
void setKifuAnalysisFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisFontSize, size);
}

// 棋譜解析結果ウィンドウのサイズを取得
QSize kifuAnalysisResultsWindowSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsWindowSize, QSize(1100, 600)).toSize();
}

// 棋譜解析結果ウィンドウのサイズを保存
void setKifuAnalysisResultsWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsWindowSize, size);
}

// 棋譜解析ダイアログの1手あたりの思考時間（秒）を取得
int kifuAnalysisByoyomiSec()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisByoyomiSec, 3).toInt();
}

// 棋譜解析ダイアログの1手あたりの思考時間（秒）を保存
void setKifuAnalysisByoyomiSec(int sec)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisByoyomiSec, sec);
}

// 棋譜解析ダイアログの最後に選択したエンジン番号を取得
int kifuAnalysisEngineIndex()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisEngineIndex, 0).toInt();
}

// 棋譜解析ダイアログの最後に選択したエンジン番号を保存
void setKifuAnalysisEngineIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisEngineIndex, index);
}

// 棋譜解析ダイアログの解析範囲（全局面解析かどうか）を取得
bool kifuAnalysisFullRange()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisFullRange, true).toBool();
}

// 棋譜解析ダイアログの解析範囲（全局面解析かどうか）を保存
void setKifuAnalysisFullRange(bool fullRange)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisFullRange, fullRange);
}

// 棋譜解析ダイアログの解析範囲（開始手数）を取得
int kifuAnalysisStartPly()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisStartPly, 0).toInt();
}

// 棋譜解析ダイアログの解析範囲（開始手数）を保存
void setKifuAnalysisStartPly(int ply)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisStartPly, ply);
}

// 棋譜解析ダイアログの解析範囲（終了手数）を取得
int kifuAnalysisEndPly()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisEndPly, 0).toInt();
}

// 棋譜解析ダイアログの解析範囲（終了手数）を保存
void setKifuAnalysisEndPly(int ply)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisEndPly, ply);
}

// 定跡ウィンドウのフォントサイズを取得
int josekiWindowFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowFontSize, 10).toInt();
}

// 定跡ウィンドウのフォントサイズを保存
void setJosekiWindowFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowFontSize, size);
}

// 定跡ウィンドウのSFEN表示フォントサイズを取得
int josekiWindowSfenFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowSfenFontSize, 9).toInt();
}

// 定跡ウィンドウのSFEN表示フォントサイズを保存
void setJosekiWindowSfenFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowSfenFontSize, size);
}

// 定跡ウィンドウの最後に開いた定跡ファイルパスを取得
QString josekiWindowLastFilePath()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowLastFilePath, QString()).toString();
}

// 定跡ウィンドウの最後に開いた定跡ファイルパスを保存
void setJosekiWindowLastFilePath(const QString& path)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowLastFilePath, path);
}

// 定跡ウィンドウのサイズを取得
QSize josekiWindowSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowSize, QSize(800, 500)).toSize();
}

// 定跡ウィンドウのサイズを保存
void setJosekiWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowSize, size);
}

// 定跡ファイル自動読込が有効かどうかを取得
bool josekiWindowAutoLoadEnabled()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowAutoLoadEnabled, true).toBool();
}

// 定跡ファイル自動読込が有効かどうかを保存
void setJosekiWindowAutoLoadEnabled(bool enabled)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowAutoLoadEnabled, enabled);
}

// 定跡ウィンドウの表示停止状態を取得
bool josekiWindowDisplayEnabled()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowDisplayEnabled, true).toBool();
}

// 定跡ウィンドウの表示停止状態を保存
void setJosekiWindowDisplayEnabled(bool enabled)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDisplayEnabled, enabled);
}

// 定跡ウィンドウのSFEN詳細表示状態を取得
bool josekiWindowSfenDetailVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowSfenDetailVisible, false).toBool();
}

// 定跡ウィンドウのSFEN詳細表示状態を保存
void setJosekiWindowSfenDetailVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowSfenDetailVisible, visible);
}

// 定跡ウィンドウの最近使ったファイルリストを取得
QStringList josekiWindowRecentFiles()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowRecentFiles, QStringList()).toStringList();
}

// 定跡ウィンドウの最近使ったファイルリストを保存
void setJosekiWindowRecentFiles(const QStringList& files)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowRecentFiles, files);
}

// 定跡手編集ダイアログのフォントサイズを取得
int josekiMoveDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowMoveDialogFontSize, 10).toInt();
}

// 定跡手編集ダイアログのフォントサイズを保存
void setJosekiMoveDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowMoveDialogFontSize, size);
}

// CSA通信ログのフォントサイズを取得
int csaLogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeCsaLog, 10).toInt();
}

// CSA通信ログのフォントサイズを保存
void setCsaLogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaLog, size);
}

// CSA待機ダイアログのフォントサイズを取得
int csaWaitingDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeCsaWaitingDialog, 10).toInt();
}

// CSA待機ダイアログのフォントサイズを保存
void setCsaWaitingDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaWaitingDialog, size);
}

// CSA通信対局ダイアログのフォントサイズを取得
int csaGameDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeCsaGameDialog, 10).toInt();
}

// CSA通信対局ダイアログのフォントサイズを保存
void setCsaGameDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaGameDialog, size);
}

// 持将棋の点数ダイアログのフォントサイズを取得
int jishogiScoreFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeJishogiScore, 10).toInt();
}

// 持将棋の点数ダイアログのフォントサイズを保存
void setJishogiScoreFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeJishogiScore, size);
}

// 持将棋の点数ダイアログのウィンドウサイズを取得
QSize jishogiScoreDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJishogiScoreDialogSize, QSize(250, 280)).toSize();
}

// 持将棋の点数ダイアログのウィンドウサイズを保存
void setJishogiScoreDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJishogiScoreDialogSize, size);
}

// 言語コードを取得
QString language()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kLanguage, "system").toString();
}

// 言語コードを保存
void setLanguage(const QString& lang)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kLanguage, lang);
}

// メニューウィンドウのお気に入りアクションリストを取得
QStringList menuWindowFavorites()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowFavorites, QStringList()).toStringList();
}

// メニューウィンドウのお気に入りアクションリストを保存
void setMenuWindowFavorites(const QStringList& favorites)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowFavorites, favorites);
}

// メニューウィンドウのサイズを取得
QSize menuWindowSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowSize, QSize(500, 400)).toSize();
}

// メニューウィンドウのサイズを保存
void setMenuWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowSize, size);
}

// メニューウィンドウのボタンサイズを取得
int menuWindowButtonSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowButtonSize, 72).toInt();
}

// メニューウィンドウのボタンサイズを保存
void setMenuWindowButtonSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowButtonSize, size);
}

// メニューウィンドウのフォントサイズを取得
int menuWindowFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowFontSize, 9).toInt();
}

// メニューウィンドウのフォントサイズを保存
void setMenuWindowFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowFontSize, size);
}

// ツールバーの表示状態を取得
bool toolbarVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kToolbarVisible, true).toBool();
}

// ツールバーの表示状態を保存
void setToolbarVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kToolbarVisible, visible);
}

// エンジン設定ダイアログのフォントサイズを取得
int engineSettingsFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeEngineSettings, 12).toInt();
}

// エンジン設定ダイアログのフォントサイズを保存
void setEngineSettingsFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeEngineSettings, size);
}

// エンジン設定ダイアログのウィンドウサイズを取得
QSize engineSettingsDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEngineSettingsDialogSize, QSize(800, 800)).toSize();
}

// エンジン設定ダイアログのウィンドウサイズを保存
void setEngineSettingsDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEngineSettingsDialogSize, size);
}

// エンジン登録ダイアログのフォントサイズを取得
int engineRegistrationFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeEngineRegistration, 12).toInt();
}

// エンジン登録ダイアログのフォントサイズを保存
void setEngineRegistrationFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeEngineRegistration, size);
}

// エンジン登録ダイアログのウィンドウサイズを取得
QSize engineRegistrationDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEngineRegistrationDialogSize, QSize(647, 421)).toSize();
}

// エンジン登録ダイアログのウィンドウサイズを保存
void setEngineRegistrationDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEngineRegistrationDialogSize, size);
}

// 検討ダイアログのフォントサイズを取得
int considerationDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeConsiderationDialog, 10).toInt();
}

// 検討ダイアログのフォントサイズを保存
void setConsiderationDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeConsiderationDialog, size);
}

// 検討ダイアログの最後に選択したエンジン番号を取得
int considerationEngineIndex()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kConsiderationEngineIndex, 0).toInt();
}

// 検討ダイアログの最後に選択したエンジン番号を保存
void setConsiderationEngineIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kConsiderationEngineIndex, index);
}

// 検討ダイアログの時間無制限フラグを取得
bool considerationUnlimitedTime()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kConsiderationUnlimitedTime, true).toBool();
}

// 検討ダイアログの時間無制限フラグを保存
void setConsiderationUnlimitedTime(bool unlimited)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kConsiderationUnlimitedTime, unlimited);
}

// 検討ダイアログの検討時間（秒）を取得
int considerationByoyomiSec()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kConsiderationByoyomiSec, 0).toInt();
}

// 検討ダイアログの検討時間（秒）を保存
void setConsiderationByoyomiSec(int sec)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kConsiderationByoyomiSec, sec);
}

// 検討の候補手の数を取得
int considerationMultiPV()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kConsiderationMultiPV, 1).toInt();
}

// 検討の候補手の数を保存
void setConsiderationMultiPV(int multiPV)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kConsiderationMultiPV, multiPV);
}

// 検討タブのフォントサイズを取得
int considerationFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kConsiderationTabFontSize, 10).toInt();
}

// 検討タブのフォントサイズを保存
void setConsiderationFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kConsiderationTabFontSize, size);
}

// 評価値グラフドックの状態を取得
QByteArray evalChartDockState()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartDockState, QByteArray()).toByteArray();
}

// 評価値グラフドックの状態を保存
void setEvalChartDockState(const QByteArray& state)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartDockState, state);
}

// 評価値グラフドックのフローティング状態を取得
bool evalChartDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartDockFloating, false).toBool();
}

// 評価値グラフドックのフローティング状態を保存
void setEvalChartDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartDockFloating, floating);
}

// 評価値グラフドックのジオメトリを取得
QByteArray evalChartDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartDockGeometry, QByteArray()).toByteArray();
}

// 評価値グラフドックのジオメトリを保存
void setEvalChartDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartDockGeometry, geometry);
}

// 評価値グラフドックの表示状態を取得
bool evalChartDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kEvalChartDockVisible, true).toBool();
}

// 評価値グラフドックの表示状態を保存
void setEvalChartDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kEvalChartDockVisible, visible);
}

// 棋譜欄ドックのフローティング状態を取得
bool recordPaneDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kRecordPaneDockFloating, false).toBool();
}

// 棋譜欄ドックのフローティング状態を保存
void setRecordPaneDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kRecordPaneDockFloating, floating);
}

// 棋譜欄ドックのジオメトリを取得
QByteArray recordPaneDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kRecordPaneDockGeometry, QByteArray()).toByteArray();
}

// 棋譜欄ドックのジオメトリを保存
void setRecordPaneDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kRecordPaneDockGeometry, geometry);
}

// 棋譜欄ドックの表示状態を取得
bool recordPaneDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kRecordPaneDockVisible, true).toBool();
}

// 棋譜欄ドックの表示状態を保存
void setRecordPaneDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kRecordPaneDockVisible, visible);
}

// 解析タブドックのフローティング状態を取得
bool analysisTabDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kAnalysisTabDockFloating, false).toBool();
}

// 解析タブドックのフローティング状態を保存
void setAnalysisTabDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kAnalysisTabDockFloating, floating);
}

// 解析タブドックのジオメトリを取得
QByteArray analysisTabDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kAnalysisTabDockGeometry, QByteArray()).toByteArray();
}

// 解析タブドックのジオメトリを保存
void setAnalysisTabDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kAnalysisTabDockGeometry, geometry);
}

// 解析タブドックの表示状態を取得
bool analysisTabDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kAnalysisTabDockVisible, true).toBool();
}

// 解析タブドックの表示状態を保存
void setAnalysisTabDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kAnalysisTabDockVisible, visible);
}

// 将棋盤ドックのフローティング状態を取得
bool boardDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kBoardDockFloating, false).toBool();
}

// 将棋盤ドックのフローティング状態を保存
void setBoardDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kBoardDockFloating, floating);
}

// 将棋盤ドックのジオメトリを取得
QByteArray boardDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kBoardDockGeometry, QByteArray()).toByteArray();
}

// 将棋盤ドックのジオメトリを保存
void setBoardDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kBoardDockGeometry, geometry);
}

// 将棋盤ドックの表示状態を取得
bool boardDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kBoardDockVisible, true).toBool();
}

// 将棋盤ドックの表示状態を保存
void setBoardDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kBoardDockVisible, visible);
}

// メニューウィンドウドックのフローティング状態を取得
bool menuWindowDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowDockFloating, false).toBool();
}

// メニューウィンドウドックのフローティング状態を保存
void setMenuWindowDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowDockFloating, floating);
}

// メニューウィンドウドックのジオメトリを取得
QByteArray menuWindowDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowDockGeometry, QByteArray()).toByteArray();
}

// メニューウィンドウドックのジオメトリを保存
void setMenuWindowDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowDockGeometry, geometry);
}

// メニューウィンドウドックの表示状態を取得
bool menuWindowDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kMenuWindowDockVisible, false).toBool();  // デフォルトは非表示
}

// メニューウィンドウドックの表示状態を保存
void setMenuWindowDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kMenuWindowDockVisible, visible);
}

// 定跡ウィンドウドックのフローティング状態を取得
bool josekiWindowDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowDockFloating, false).toBool();
}

// 定跡ウィンドウドックのフローティング状態を保存
void setJosekiWindowDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDockFloating, floating);
}

// 定跡ウィンドウドックのジオメトリを取得
QByteArray josekiWindowDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowDockGeometry, QByteArray()).toByteArray();
}

// 定跡ウィンドウドックのジオメトリを保存
void setJosekiWindowDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDockGeometry, geometry);
}

// 定跡ウィンドウドックの表示状態を取得
bool josekiWindowDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowDockVisible, false).toBool();  // デフォルトは非表示
}

// 定跡ウィンドウドックの表示状態を保存
void setJosekiWindowDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDockVisible, visible);
}

// 保存されているカスタムドックレイアウト名のリストを取得
QStringList savedDockLayoutNames()
{
    QSettings& s = openSettings();
    s.beginGroup(SettingsKeys::kCustomDockLayoutsGroup);
    QStringList names = s.childKeys();
    s.endGroup();
    return names;
}

// 指定した名前でドックレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state)
{
    QSettings& s = openSettings();
    s.setValue(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name, state);
}

// 指定した名前のドックレイアウトを取得
QByteArray loadDockLayout(const QString& name)
{
    QSettings& s = openSettings();
    return s.value(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name, QByteArray()).toByteArray();
}

// 指定した名前のドックレイアウトを削除
void deleteDockLayout(const QString& name)
{
    QSettings& s = openSettings();
    s.remove(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name);
}

// 起動時に使用するレイアウト名を取得
QString startupDockLayoutName()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kStartupDockLayoutName, QString()).toString();
}

// 起動時に使用するレイアウト名を保存
void setStartupDockLayoutName(const QString& name)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kStartupDockLayoutName, name);
}

// 棋譜解析結果ドックのフローティング状態を取得
bool kifuAnalysisResultsDockFloating()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsDockFloating, false).toBool();
}

// 棋譜解析結果ドックのフローティング状態を保存
void setKifuAnalysisResultsDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsDockFloating, floating);
}

// 棋譜解析結果ドックのジオメトリを取得
QByteArray kifuAnalysisResultsDockGeometry()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsDockGeometry, QByteArray()).toByteArray();
}

// 棋譜解析結果ドックのジオメトリを保存
void setKifuAnalysisResultsDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsDockGeometry, geometry);
}

// 棋譜解析結果ドックの表示状態を取得
bool kifuAnalysisResultsDockVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsDockVisible, false).toBool();  // デフォルトは非表示
}

// 棋譜解析結果ドックの表示状態を保存
void setKifuAnalysisResultsDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsDockVisible, visible);
}

// 全ドックの固定設定を取得
bool docksLocked()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kDocksLocked, false).toBool();  // デフォルトは非固定
}

// 全ドックの固定設定を保存
void setDocksLocked(bool locked)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kDocksLocked, locked);
}

// 棋譜欄の消費時間列の表示状態を取得
bool kifuTimeColumnVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kRecordPaneTimeColumnVisible, true).toBool();
}

// 棋譜欄の消費時間列の表示状態を保存
void setKifuTimeColumnVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kRecordPaneTimeColumnVisible, visible);
}

// 棋譜欄のしおり列の表示状態を取得
bool kifuBookmarkColumnVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kRecordPaneBookmarkColumnVisible, true).toBool();
}

// 棋譜欄のしおり列の表示状態を保存
void setKifuBookmarkColumnVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kRecordPaneBookmarkColumnVisible, visible);
}

// 棋譜欄のコメント列の表示状態を取得
bool kifuCommentColumnVisible()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kRecordPaneCommentColumnVisible, true).toBool();
}

// 棋譜欄のコメント列の表示状態を保存
void setKifuCommentColumnVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kRecordPaneCommentColumnVisible, visible);
}

// 定跡マージダイアログのフォントサイズを取得
int josekiMergeDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowMergeDialogFontSize, 10).toInt();
}

// 定跡マージダイアログのフォントサイズを保存
void setJosekiMergeDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowMergeDialogFontSize, size);
}

// 定跡マージダイアログのウィンドウサイズを取得
QSize josekiMergeDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiWindowMergeDialogSize, QSize(600, 500)).toSize();
}

// 定跡マージダイアログのウィンドウサイズを保存
void setJosekiMergeDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiWindowMergeDialogSize, size);
}

// 対局開始ダイアログのウィンドウサイズを取得
QSize startGameDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kStartGameDialogSize, QSize(1000, 580)).toSize();
}

// 対局開始ダイアログのウィンドウサイズを保存
void setStartGameDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kStartGameDialogSize, size);
}

// 対局開始ダイアログのフォントサイズを取得
int startGameDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeStartGameDialog, 9).toInt();
}

// 対局開始ダイアログのフォントサイズを保存
void setStartGameDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeStartGameDialog, size);
}

// 棋譜貼り付けダイアログのウィンドウサイズを取得
QSize kifuPasteDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuPasteDialogSize, QSize(600, 500)).toSize();
}

// 棋譜貼り付けダイアログのウィンドウサイズを保存
void setKifuPasteDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuPasteDialogSize, size);
}

// 棋譜貼り付けダイアログのフォントサイズを取得
int kifuPasteDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeKifuPasteDialog, 10).toInt();
}

// 棋譜貼り付けダイアログのフォントサイズを保存
void setKifuPasteDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeKifuPasteDialog, size);
}

// CSA通信ログウィンドウのサイズを取得
QSize csaLogWindowSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kCsaLogWindowSize, QSize(550, 450)).toSize();
}

// CSA通信ログウィンドウのサイズを保存
void setCsaLogWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kCsaLogWindowSize, size);
}

// 定跡ウィンドウのテーブル列幅を取得
QList<int> josekiWindowColumnWidths()
{
    QSettings& s = openSettings();
    QList<int> widths;

    int size = s.beginReadArray(SettingsKeys::kJosekiWindowColumnWidths);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}

// 定跡ウィンドウのテーブル列幅を保存
void setJosekiWindowColumnWidths(const QList<int>& widths)
{
    QSettings& s = openSettings();

    s.beginWriteArray(SettingsKeys::kJosekiWindowColumnWidths);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    s.sync();
}

// CSA通信対局ダイアログのウィンドウサイズを取得
QSize csaGameDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kCsaGameDialogSize, QSize(450, 380)).toSize();
}

// CSA通信対局ダイアログのウィンドウサイズを保存
void setCsaGameDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kCsaGameDialogSize, size);
}

// 棋譜解析ダイアログのウィンドウサイズを取得
QSize kifuAnalysisDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kKifuAnalysisDialogSize, QSize(500, 340)).toSize();
}

// 棋譜解析ダイアログのウィンドウサイズを保存
void setKifuAnalysisDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisDialogSize, size);
}

// 定跡手編集ダイアログのウィンドウサイズを取得
QSize josekiMoveDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kJosekiMoveDialogSize, QSize(500, 600)).toSize();
}

// 定跡手編集ダイアログのウィンドウサイズを保存
void setJosekiMoveDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kJosekiMoveDialogSize, size);
}

// 詰将棋局面生成のファイル保存で最後に使用したディレクトリを取得
QString tsumeshogiGeneratorLastSaveDirectory()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorLastSaveDirectory, QString()).toString();
}

// 詰将棋局面生成のファイル保存で最後に使用したディレクトリを保存
void setTsumeshogiGeneratorLastSaveDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorLastSaveDirectory, dir);
}

// 詰将棋局面生成ダイアログのウィンドウサイズを取得
QSize tsumeshogiGeneratorDialogSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorDialogSize, QSize(600, 550)).toSize();
}

// 詰将棋局面生成ダイアログのウィンドウサイズを保存
void setTsumeshogiGeneratorDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorDialogSize, size);
}

// 詰将棋局面生成ダイアログのフォントサイズを取得
int tsumeshogiGeneratorFontSize()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kFontSizeTsumeshogiGenerator, 10).toInt();
}

// 詰将棋局面生成ダイアログのフォントサイズを保存
void setTsumeshogiGeneratorFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kFontSizeTsumeshogiGenerator, size);
}

// 詰将棋局面生成の最後に選択したエンジン番号を取得
int tsumeshogiGeneratorEngineIndex()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorEngineIndex, 0).toInt();
}

// 詰将棋局面生成の最後に選択したエンジン番号を保存
void setTsumeshogiGeneratorEngineIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorEngineIndex, index);
}

// 詰将棋局面生成の目標手数を取得
int tsumeshogiGeneratorTargetMoves()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorTargetMoves, 3).toInt();
}

// 詰将棋局面生成の目標手数を保存
void setTsumeshogiGeneratorTargetMoves(int moves)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorTargetMoves, moves);
}

// 詰将棋局面生成の攻め駒上限を取得
int tsumeshogiGeneratorMaxAttackPieces()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorMaxAttackPieces, 4).toInt();
}

// 詰将棋局面生成の攻め駒上限を保存
void setTsumeshogiGeneratorMaxAttackPieces(int count)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorMaxAttackPieces, count);
}

// 詰将棋局面生成の守り駒上限を取得
int tsumeshogiGeneratorMaxDefendPieces()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorMaxDefendPieces, 1).toInt();
}

// 詰将棋局面生成の守り駒上限を保存
void setTsumeshogiGeneratorMaxDefendPieces(int count)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorMaxDefendPieces, count);
}

// 詰将棋局面生成の配置範囲を取得
int tsumeshogiGeneratorAttackRange()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorAttackRange, 3).toInt();
}

// 詰将棋局面生成の配置範囲を保存
void setTsumeshogiGeneratorAttackRange(int range)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorAttackRange, range);
}

// 詰将棋局面生成の探索時間(秒)を取得
int tsumeshogiGeneratorTimeoutSec()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorTimeoutSec, 5).toInt();
}

// 詰将棋局面生成の探索時間(秒)を保存
void setTsumeshogiGeneratorTimeoutSec(int sec)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorTimeoutSec, sec);
}

// 詰将棋局面生成の生成上限を取得
int tsumeshogiGeneratorMaxPositions()
{
    QSettings& s = openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorMaxPositions, 10).toInt();
}

// 詰将棋局面生成の生成上限を保存
void setTsumeshogiGeneratorMaxPositions(int count)
{
    QSettings& s = openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorMaxPositions, count);
}

// 設定バージョンのマイグレーション
// 将来のバージョンでキー名変更や構造変更が必要になった場合のフック
void migrateSettingsIfNeeded()
{
    QSettings& s = openSettings();
    const int version = s.value(SettingsKeys::kSettingsVersion, 0).toInt();
    if (version >= SettingsKeys::kCurrentSettingsVersion) {
        return;
    }

    // 将来のマイグレーション処理をここに追加:
    // if (version < 2) { ... }
    // if (version < 3) { ... }

    s.setValue(SettingsKeys::kSettingsVersion, SettingsKeys::kCurrentSettingsVersion);
}

} // namespace SettingsService
