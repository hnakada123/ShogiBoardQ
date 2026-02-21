/// @file settingsservice.cpp
/// @brief アプリケーション設定の永続化サービスの実装

#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QWidget>
#include <QStandardPaths>
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
    const QSize sz = s.value("SizeRelated/mainWindowSize", QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, ShogiView* view)
{
    if (!mainWindow || !view) return;
    QSettings& s = openSettings();
    s.beginGroup("SizeRelated");
    s.setValue("mainWindowSize", mainWindow->size());
    s.setValue("squareSize",     view->squareSize());
    s.endGroup();
    if (s.status() != QSettings::NoError) {
        qWarning("SettingsService: Failed to save settings (status=%d)", static_cast<int>(s.status()));
    }
}

QString lastKifuDirectory()
{
    QSettings& s = openSettings();
    return s.value("General/lastKifuDirectory", QString()).toString();
}

void setLastKifuDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue("General/lastKifuDirectory", dir);
}

QString lastKifuSaveDirectory()
{
    QSettings& s = openSettings();
    return s.value("General/lastKifuSaveDirectory", QString()).toString();
}

void setLastKifuSaveDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue("General/lastKifuSaveDirectory", dir);
}

// USI通信ログタブのフォントサイズを取得
int usiLogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/usiLog", 10).toInt();
}

// USI通信ログタブのフォントサイズを保存
void setUsiLogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/usiLog", size);
}

// 棋譜コメントタブのフォントサイズを取得
int commentFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/comment", 10).toInt();
}

// 棋譜コメントタブのフォントサイズを保存
void setCommentFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/comment", size);
}

// 対局情報タブのフォントサイズを取得
int gameInfoFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/gameInfo", 10).toInt();
}

// 対局情報タブのフォントサイズを保存
void setGameInfoFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/gameInfo", size);
}

// 思考タブのフォントサイズを取得
int thinkingFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/thinking", 10).toInt();
}

// 思考タブのフォントサイズを保存
void setThinkingFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/thinking", size);
}

// 最後に選択されたタブインデックスを取得
int lastSelectedTabIndex()
{
    QSettings& s = openSettings();
    return s.value("UI/lastSelectedTabIndex", 0).toInt();
}

// 最後に選択されたタブインデックスを保存
void setLastSelectedTabIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue("UI/lastSelectedTabIndex", index);
}

// 読み筋表示ウィンドウのサイズを取得
QSize pvBoardDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/pvBoardDialogSize", QSize(620, 720)).toSize();
}

// 読み筋表示ウィンドウのサイズを保存
void setPvBoardDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/pvBoardDialogSize", size);
}

// 局面集ビューアのサイズを取得
QSize sfenCollectionDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/sfenCollectionDialogSize", QSize(620, 780)).toSize();
}

// 局面集ビューアのサイズを保存
void setSfenCollectionDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/sfenCollectionDialogSize", size);
}

// 局面集ビューアの最近使ったファイルリストを取得
QStringList sfenCollectionRecentFiles()
{
    QSettings& s = openSettings();
    return s.value("SfenCollection/recentFiles", QStringList()).toStringList();
}

// 局面集ビューアの最近使ったファイルリストを保存
void setSfenCollectionRecentFiles(const QStringList& files)
{
    QSettings& s = openSettings();
    s.setValue("SfenCollection/recentFiles", files);
}

// 局面集ビューアの将棋盤マスサイズを取得
int sfenCollectionSquareSize()
{
    QSettings& s = openSettings();
    return s.value("SfenCollection/squareSize", 50).toInt();
}

// 局面集ビューアの将棋盤マスサイズを保存
void setSfenCollectionSquareSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("SfenCollection/squareSize", size);
}

// 局面集ビューアの最後に開いたディレクトリを取得
QString sfenCollectionLastDirectory()
{
    QSettings& s = openSettings();
    return s.value("SfenCollection/lastDirectory", QString()).toString();
}

// 局面集ビューアの最後に開いたディレクトリを保存
void setSfenCollectionLastDirectory(const QString& dir)
{
    QSettings& s = openSettings();
    s.setValue("SfenCollection/lastDirectory", dir);
}

// 評価値グラフの設定
int evalChartYLimit()
{
    QSettings& s = openSettings();
    return s.value("EvalChart/yLimit", 2000).toInt();
}

void setEvalChartYLimit(int limit)
{
    QSettings& s = openSettings();
    s.setValue("EvalChart/yLimit", limit);
}

int evalChartXLimit()
{
    QSettings& s = openSettings();
    return s.value("EvalChart/xLimit", 500).toInt();
}

void setEvalChartXLimit(int limit)
{
    QSettings& s = openSettings();
    s.setValue("EvalChart/xLimit", limit);
}

int evalChartXInterval()
{
    QSettings& s = openSettings();
    return s.value("EvalChart/xInterval", 10).toInt();
}

void setEvalChartXInterval(int interval)
{
    QSettings& s = openSettings();
    s.setValue("EvalChart/xInterval", interval);
}

int evalChartLabelFontSize()
{
    QSettings& s = openSettings();
    return s.value("EvalChart/labelFontSize", 7).toInt();
}

void setEvalChartLabelFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("EvalChart/labelFontSize", size);
}

// エンジン情報ウィジェットの列幅を取得
QList<int> engineInfoColumnWidths(int widgetIndex)
{
    QSettings& s = openSettings();
    QString key = QString("EngineInfo/columnWidths%1").arg(widgetIndex);
    QList<int> widths;
    
    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value("width", 0).toInt());
    }
    s.endArray();
    
    return widths;
}

// エンジン情報ウィジェットの列幅を保存
void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths)
{
    QSettings& s = openSettings();
    QString key = QString("EngineInfo/columnWidths%1").arg(widgetIndex);
    
    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue("width", widths.at(i));
    }
    s.endArray();
    
    // 即座に書き込み
    s.sync();
}

// 思考タブ下段（読み筋テーブル）の列幅を取得
QList<int> thinkingViewColumnWidths(int viewIndex)
{
    QSettings& s = openSettings();
    QString key = QString("ThinkingView/columnWidths%1").arg(viewIndex);
    QList<int> widths;
    
    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value("width", 0).toInt());
    }
    s.endArray();
    
    return widths;
}

// 思考タブ下段（読み筋テーブル）の列幅を保存
void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths)
{
    QSettings& s = openSettings();
    QString key = QString("ThinkingView/columnWidths%1").arg(viewIndex);
    
    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue("width", widths.at(i));
    }
    s.endArray();
    
    // 即座に書き込み
    s.sync();
}

// 棋譜欄・分岐候補欄のフォントサイズを取得
int kifuPaneFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/kifuPane", 10).toInt();
}

// 棋譜欄・分岐候補欄のフォントサイズを保存
void setKifuPaneFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/kifuPane", size);
}

// 棋譜解析ダイアログのフォントサイズを取得
int kifuAnalysisFontSize()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/fontSize", 0).toInt();
}

// 棋譜解析ダイアログのフォントサイズを保存
void setKifuAnalysisFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/fontSize", size);
}

// 棋譜解析結果ウィンドウのサイズを取得
QSize kifuAnalysisResultsWindowSize()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/resultsWindowSize", QSize(1100, 600)).toSize();
}

// 棋譜解析結果ウィンドウのサイズを保存
void setKifuAnalysisResultsWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/resultsWindowSize", size);
}

// 棋譜解析ダイアログの1手あたりの思考時間（秒）を取得
int kifuAnalysisByoyomiSec()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/byoyomiSec", 3).toInt();
}

// 棋譜解析ダイアログの1手あたりの思考時間（秒）を保存
void setKifuAnalysisByoyomiSec(int sec)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/byoyomiSec", sec);
}

// 棋譜解析ダイアログの最後に選択したエンジン番号を取得
int kifuAnalysisEngineIndex()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/engineIndex", 0).toInt();
}

// 棋譜解析ダイアログの最後に選択したエンジン番号を保存
void setKifuAnalysisEngineIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/engineIndex", index);
}

// 棋譜解析ダイアログの解析範囲（全局面解析かどうか）を取得
bool kifuAnalysisFullRange()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/fullRange", true).toBool();
}

// 棋譜解析ダイアログの解析範囲（全局面解析かどうか）を保存
void setKifuAnalysisFullRange(bool fullRange)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/fullRange", fullRange);
}

// 棋譜解析ダイアログの解析範囲（開始手数）を取得
int kifuAnalysisStartPly()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/startPly", 0).toInt();
}

// 棋譜解析ダイアログの解析範囲（開始手数）を保存
void setKifuAnalysisStartPly(int ply)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/startPly", ply);
}

// 棋譜解析ダイアログの解析範囲（終了手数）を取得
int kifuAnalysisEndPly()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysis/endPly", 0).toInt();
}

// 棋譜解析ダイアログの解析範囲（終了手数）を保存
void setKifuAnalysisEndPly(int ply)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysis/endPly", ply);
}

// 定跡ウィンドウのフォントサイズを取得
int josekiWindowFontSize()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/fontSize", 10).toInt();
}

// 定跡ウィンドウのフォントサイズを保存
void setJosekiWindowFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/fontSize", size);
}

// 定跡ウィンドウのSFEN表示フォントサイズを取得
int josekiWindowSfenFontSize()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/sfenFontSize", 9).toInt();
}

// 定跡ウィンドウのSFEN表示フォントサイズを保存
void setJosekiWindowSfenFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/sfenFontSize", size);
}

// 定跡ウィンドウの最後に開いた定跡ファイルパスを取得
QString josekiWindowLastFilePath()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/lastFilePath", QString()).toString();
}

// 定跡ウィンドウの最後に開いた定跡ファイルパスを保存
void setJosekiWindowLastFilePath(const QString& path)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/lastFilePath", path);
}

// 定跡ウィンドウのサイズを取得
QSize josekiWindowSize()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/size", QSize(800, 500)).toSize();
}

// 定跡ウィンドウのサイズを保存
void setJosekiWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/size", size);
}

// 定跡ファイル自動読込が有効かどうかを取得
bool josekiWindowAutoLoadEnabled()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/autoLoadEnabled", true).toBool();
}

// 定跡ファイル自動読込が有効かどうかを保存
void setJosekiWindowAutoLoadEnabled(bool enabled)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/autoLoadEnabled", enabled);
}

// 定跡ウィンドウの表示停止状態を取得
bool josekiWindowDisplayEnabled()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/displayEnabled", true).toBool();
}

// 定跡ウィンドウの表示停止状態を保存
void setJosekiWindowDisplayEnabled(bool enabled)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/displayEnabled", enabled);
}

// 定跡ウィンドウのSFEN詳細表示状態を取得
bool josekiWindowSfenDetailVisible()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/sfenDetailVisible", false).toBool();
}

// 定跡ウィンドウのSFEN詳細表示状態を保存
void setJosekiWindowSfenDetailVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/sfenDetailVisible", visible);
}

// 定跡ウィンドウの最近使ったファイルリストを取得
QStringList josekiWindowRecentFiles()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/recentFiles", QStringList()).toStringList();
}

// 定跡ウィンドウの最近使ったファイルリストを保存
void setJosekiWindowRecentFiles(const QStringList& files)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/recentFiles", files);
}

// 定跡手編集ダイアログのフォントサイズを取得
int josekiMoveDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/moveDialogFontSize", 10).toInt();
}

// 定跡手編集ダイアログのフォントサイズを保存
void setJosekiMoveDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/moveDialogFontSize", size);
}

// CSA通信ログのフォントサイズを取得
int csaLogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/csaLog", 10).toInt();
}

// CSA通信ログのフォントサイズを保存
void setCsaLogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/csaLog", size);
}

// CSA待機ダイアログのフォントサイズを取得
int csaWaitingDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/csaWaitingDialog", 10).toInt();
}

// CSA待機ダイアログのフォントサイズを保存
void setCsaWaitingDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/csaWaitingDialog", size);
}

// CSA通信対局ダイアログのフォントサイズを取得
int csaGameDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/csaGameDialog", 10).toInt();
}

// CSA通信対局ダイアログのフォントサイズを保存
void setCsaGameDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/csaGameDialog", size);
}

// 持将棋の点数ダイアログのフォントサイズを取得
int jishogiScoreFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/jishogiScore", 10).toInt();
}

// 持将棋の点数ダイアログのフォントサイズを保存
void setJishogiScoreFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/jishogiScore", size);
}

// 持将棋の点数ダイアログのウィンドウサイズを取得
QSize jishogiScoreDialogSize()
{
    QSettings& s = openSettings();
    return s.value("JishogiScore/dialogSize", QSize(250, 280)).toSize();
}

// 持将棋の点数ダイアログのウィンドウサイズを保存
void setJishogiScoreDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("JishogiScore/dialogSize", size);
}

// 言語コードを取得
QString language()
{
    QSettings& s = openSettings();
    return s.value("General/language", "system").toString();
}

// 言語コードを保存
void setLanguage(const QString& lang)
{
    QSettings& s = openSettings();
    s.setValue("General/language", lang);
}

// メニューウィンドウのお気に入りアクションリストを取得
QStringList menuWindowFavorites()
{
    QSettings& s = openSettings();
    return s.value("MenuWindow/favorites", QStringList()).toStringList();
}

// メニューウィンドウのお気に入りアクションリストを保存
void setMenuWindowFavorites(const QStringList& favorites)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindow/favorites", favorites);
}

// メニューウィンドウのサイズを取得
QSize menuWindowSize()
{
    QSettings& s = openSettings();
    return s.value("MenuWindow/size", QSize(500, 400)).toSize();
}

// メニューウィンドウのサイズを保存
void setMenuWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindow/size", size);
}

// メニューウィンドウのボタンサイズを取得
int menuWindowButtonSize()
{
    QSettings& s = openSettings();
    return s.value("MenuWindow/buttonSize", 72).toInt();
}

// メニューウィンドウのボタンサイズを保存
void setMenuWindowButtonSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindow/buttonSize", size);
}

// メニューウィンドウのフォントサイズを取得
int menuWindowFontSize()
{
    QSettings& s = openSettings();
    return s.value("MenuWindow/fontSize", 9).toInt();
}

// メニューウィンドウのフォントサイズを保存
void setMenuWindowFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindow/fontSize", size);
}

// ツールバーの表示状態を取得
bool toolbarVisible()
{
    QSettings& s = openSettings();
    return s.value("UI/toolbarVisible", true).toBool();
}

// ツールバーの表示状態を保存
void setToolbarVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("UI/toolbarVisible", visible);
}

// エンジン設定ダイアログのフォントサイズを取得
int engineSettingsFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/engineSettings", 12).toInt();
}

// エンジン設定ダイアログのフォントサイズを保存
void setEngineSettingsFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/engineSettings", size);
}

// エンジン設定ダイアログのウィンドウサイズを取得
QSize engineSettingsDialogSize()
{
    QSettings& s = openSettings();
    return s.value("EngineSettings/dialogSize", QSize(800, 800)).toSize();
}

// エンジン設定ダイアログのウィンドウサイズを保存
void setEngineSettingsDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("EngineSettings/dialogSize", size);
}

// エンジン登録ダイアログのフォントサイズを取得
int engineRegistrationFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/engineRegistration", 12).toInt();
}

// エンジン登録ダイアログのフォントサイズを保存
void setEngineRegistrationFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/engineRegistration", size);
}

// エンジン登録ダイアログのウィンドウサイズを取得
QSize engineRegistrationDialogSize()
{
    QSettings& s = openSettings();
    return s.value("EngineRegistration/dialogSize", QSize(647, 421)).toSize();
}

// エンジン登録ダイアログのウィンドウサイズを保存
void setEngineRegistrationDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("EngineRegistration/dialogSize", size);
}

// 検討ダイアログのフォントサイズを取得
int considerationDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/considerationDialog", 10).toInt();
}

// 検討ダイアログのフォントサイズを保存
void setConsiderationDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/considerationDialog", size);
}

// 検討ダイアログの最後に選択したエンジン番号を取得
int considerationEngineIndex()
{
    QSettings& s = openSettings();
    return s.value("Consideration/engineIndex", 0).toInt();
}

// 検討ダイアログの最後に選択したエンジン番号を保存
void setConsiderationEngineIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue("Consideration/engineIndex", index);
}

// 検討ダイアログの時間無制限フラグを取得
bool considerationUnlimitedTime()
{
    QSettings& s = openSettings();
    return s.value("Consideration/unlimitedTime", true).toBool();
}

// 検討ダイアログの時間無制限フラグを保存
void setConsiderationUnlimitedTime(bool unlimited)
{
    QSettings& s = openSettings();
    s.setValue("Consideration/unlimitedTime", unlimited);
}

// 検討ダイアログの検討時間（秒）を取得
int considerationByoyomiSec()
{
    QSettings& s = openSettings();
    return s.value("Consideration/byoyomiSec", 0).toInt();
}

// 検討ダイアログの検討時間（秒）を保存
void setConsiderationByoyomiSec(int sec)
{
    QSettings& s = openSettings();
    s.setValue("Consideration/byoyomiSec", sec);
}

// 検討の候補手の数を取得
int considerationMultiPV()
{
    QSettings& s = openSettings();
    return s.value("Consideration/multiPV", 1).toInt();
}

// 検討の候補手の数を保存
void setConsiderationMultiPV(int multiPV)
{
    QSettings& s = openSettings();
    s.setValue("Consideration/multiPV", multiPV);
}

// 検討タブのフォントサイズを取得
int considerationFontSize()
{
    QSettings& s = openSettings();
    return s.value("ConsiderationTab/fontSize", 10).toInt();
}

// 検討タブのフォントサイズを保存
void setConsiderationFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("ConsiderationTab/fontSize", size);
}

// 評価値グラフドックの状態を取得
QByteArray evalChartDockState()
{
    QSettings& s = openSettings();
    return s.value("EvalChartDock/state", QByteArray()).toByteArray();
}

// 評価値グラフドックの状態を保存
void setEvalChartDockState(const QByteArray& state)
{
    QSettings& s = openSettings();
    s.setValue("EvalChartDock/state", state);
}

// 評価値グラフドックのフローティング状態を取得
bool evalChartDockFloating()
{
    QSettings& s = openSettings();
    return s.value("EvalChartDock/floating", false).toBool();
}

// 評価値グラフドックのフローティング状態を保存
void setEvalChartDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("EvalChartDock/floating", floating);
}

// 評価値グラフドックのジオメトリを取得
QByteArray evalChartDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("EvalChartDock/geometry", QByteArray()).toByteArray();
}

// 評価値グラフドックのジオメトリを保存
void setEvalChartDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("EvalChartDock/geometry", geometry);
}

// 評価値グラフドックの表示状態を取得
bool evalChartDockVisible()
{
    QSettings& s = openSettings();
    return s.value("EvalChartDock/visible", true).toBool();
}

// 評価値グラフドックの表示状態を保存
void setEvalChartDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("EvalChartDock/visible", visible);
}

// 棋譜欄ドックのフローティング状態を取得
bool recordPaneDockFloating()
{
    QSettings& s = openSettings();
    return s.value("RecordPaneDock/floating", false).toBool();
}

// 棋譜欄ドックのフローティング状態を保存
void setRecordPaneDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("RecordPaneDock/floating", floating);
}

// 棋譜欄ドックのジオメトリを取得
QByteArray recordPaneDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("RecordPaneDock/geometry", QByteArray()).toByteArray();
}

// 棋譜欄ドックのジオメトリを保存
void setRecordPaneDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("RecordPaneDock/geometry", geometry);
}

// 棋譜欄ドックの表示状態を取得
bool recordPaneDockVisible()
{
    QSettings& s = openSettings();
    return s.value("RecordPaneDock/visible", true).toBool();
}

// 棋譜欄ドックの表示状態を保存
void setRecordPaneDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("RecordPaneDock/visible", visible);
}

// 解析タブドックのフローティング状態を取得
bool analysisTabDockFloating()
{
    QSettings& s = openSettings();
    return s.value("AnalysisTabDock/floating", false).toBool();
}

// 解析タブドックのフローティング状態を保存
void setAnalysisTabDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("AnalysisTabDock/floating", floating);
}

// 解析タブドックのジオメトリを取得
QByteArray analysisTabDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("AnalysisTabDock/geometry", QByteArray()).toByteArray();
}

// 解析タブドックのジオメトリを保存
void setAnalysisTabDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("AnalysisTabDock/geometry", geometry);
}

// 解析タブドックの表示状態を取得
bool analysisTabDockVisible()
{
    QSettings& s = openSettings();
    return s.value("AnalysisTabDock/visible", true).toBool();
}

// 解析タブドックの表示状態を保存
void setAnalysisTabDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("AnalysisTabDock/visible", visible);
}

// 将棋盤ドックのフローティング状態を取得
bool boardDockFloating()
{
    QSettings& s = openSettings();
    return s.value("BoardDock/floating", false).toBool();
}

// 将棋盤ドックのフローティング状態を保存
void setBoardDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("BoardDock/floating", floating);
}

// 将棋盤ドックのジオメトリを取得
QByteArray boardDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("BoardDock/geometry", QByteArray()).toByteArray();
}

// 将棋盤ドックのジオメトリを保存
void setBoardDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("BoardDock/geometry", geometry);
}

// 将棋盤ドックの表示状態を取得
bool boardDockVisible()
{
    QSettings& s = openSettings();
    return s.value("BoardDock/visible", true).toBool();
}

// 将棋盤ドックの表示状態を保存
void setBoardDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("BoardDock/visible", visible);
}

// メニューウィンドウドックのフローティング状態を取得
bool menuWindowDockFloating()
{
    QSettings& s = openSettings();
    return s.value("MenuWindowDock/floating", false).toBool();
}

// メニューウィンドウドックのフローティング状態を保存
void setMenuWindowDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindowDock/floating", floating);
}

// メニューウィンドウドックのジオメトリを取得
QByteArray menuWindowDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("MenuWindowDock/geometry", QByteArray()).toByteArray();
}

// メニューウィンドウドックのジオメトリを保存
void setMenuWindowDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindowDock/geometry", geometry);
}

// メニューウィンドウドックの表示状態を取得
bool menuWindowDockVisible()
{
    QSettings& s = openSettings();
    return s.value("MenuWindowDock/visible", false).toBool();  // デフォルトは非表示
}

// メニューウィンドウドックの表示状態を保存
void setMenuWindowDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("MenuWindowDock/visible", visible);
}

// 定跡ウィンドウドックのフローティング状態を取得
bool josekiWindowDockFloating()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindowDock/floating", false).toBool();
}

// 定跡ウィンドウドックのフローティング状態を保存
void setJosekiWindowDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindowDock/floating", floating);
}

// 定跡ウィンドウドックのジオメトリを取得
QByteArray josekiWindowDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindowDock/geometry", QByteArray()).toByteArray();
}

// 定跡ウィンドウドックのジオメトリを保存
void setJosekiWindowDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindowDock/geometry", geometry);
}

// 定跡ウィンドウドックの表示状態を取得
bool josekiWindowDockVisible()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindowDock/visible", false).toBool();  // デフォルトは非表示
}

// 定跡ウィンドウドックの表示状態を保存
void setJosekiWindowDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindowDock/visible", visible);
}

// 保存されているカスタムドックレイアウト名のリストを取得
QStringList savedDockLayoutNames()
{
    QSettings& s = openSettings();
    s.beginGroup("CustomDockLayouts");
    QStringList names = s.childKeys();
    s.endGroup();
    return names;
}

// 指定した名前でドックレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state)
{
    QSettings& s = openSettings();
    s.setValue("CustomDockLayouts/" + name, state);
}

// 指定した名前のドックレイアウトを取得
QByteArray loadDockLayout(const QString& name)
{
    QSettings& s = openSettings();
    return s.value("CustomDockLayouts/" + name, QByteArray()).toByteArray();
}

// 指定した名前のドックレイアウトを削除
void deleteDockLayout(const QString& name)
{
    QSettings& s = openSettings();
    s.remove("CustomDockLayouts/" + name);
}

// 起動時に使用するレイアウト名を取得
QString startupDockLayoutName()
{
    QSettings& s = openSettings();
    return s.value("DockLayout/startupLayoutName", QString()).toString();
}

// 起動時に使用するレイアウト名を保存
void setStartupDockLayoutName(const QString& name)
{
    QSettings& s = openSettings();
    s.setValue("DockLayout/startupLayoutName", name);
}

// 棋譜解析結果ドックのフローティング状態を取得
bool kifuAnalysisResultsDockFloating()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysisResultsDock/floating", false).toBool();
}

// 棋譜解析結果ドックのフローティング状態を保存
void setKifuAnalysisResultsDockFloating(bool floating)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysisResultsDock/floating", floating);
}

// 棋譜解析結果ドックのジオメトリを取得
QByteArray kifuAnalysisResultsDockGeometry()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysisResultsDock/geometry", QByteArray()).toByteArray();
}

// 棋譜解析結果ドックのジオメトリを保存
void setKifuAnalysisResultsDockGeometry(const QByteArray& geometry)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysisResultsDock/geometry", geometry);
}

// 棋譜解析結果ドックの表示状態を取得
bool kifuAnalysisResultsDockVisible()
{
    QSettings& s = openSettings();
    return s.value("KifuAnalysisResultsDock/visible", false).toBool();  // デフォルトは非表示
}

// 棋譜解析結果ドックの表示状態を保存
void setKifuAnalysisResultsDockVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("KifuAnalysisResultsDock/visible", visible);
}

// 全ドックの固定設定を取得
bool docksLocked()
{
    QSettings& s = openSettings();
    return s.value("Dock/docksLocked", false).toBool();  // デフォルトは非固定
}

// 全ドックの固定設定を保存
void setDocksLocked(bool locked)
{
    QSettings& s = openSettings();
    s.setValue("Dock/docksLocked", locked);
}

// 棋譜欄の消費時間列の表示状態を取得
bool kifuTimeColumnVisible()
{
    QSettings& s = openSettings();
    return s.value("RecordPane/timeColumnVisible", true).toBool();
}

// 棋譜欄の消費時間列の表示状態を保存
void setKifuTimeColumnVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("RecordPane/timeColumnVisible", visible);
}

// 棋譜欄のしおり列の表示状態を取得
bool kifuBookmarkColumnVisible()
{
    QSettings& s = openSettings();
    return s.value("RecordPane/bookmarkColumnVisible", true).toBool();
}

// 棋譜欄のしおり列の表示状態を保存
void setKifuBookmarkColumnVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("RecordPane/bookmarkColumnVisible", visible);
}

// 棋譜欄のコメント列の表示状態を取得
bool kifuCommentColumnVisible()
{
    QSettings& s = openSettings();
    return s.value("RecordPane/commentColumnVisible", true).toBool();
}

// 棋譜欄のコメント列の表示状態を保存
void setKifuCommentColumnVisible(bool visible)
{
    QSettings& s = openSettings();
    s.setValue("RecordPane/commentColumnVisible", visible);
}

// 定跡マージダイアログのフォントサイズを取得
int josekiMergeDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/mergeDialogFontSize", 10).toInt();
}

// 定跡マージダイアログのフォントサイズを保存
void setJosekiMergeDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/mergeDialogFontSize", size);
}

// 定跡マージダイアログのウィンドウサイズを取得
QSize josekiMergeDialogSize()
{
    QSettings& s = openSettings();
    return s.value("JosekiWindow/mergeDialogSize", QSize(600, 500)).toSize();
}

// 定跡マージダイアログのウィンドウサイズを保存
void setJosekiMergeDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("JosekiWindow/mergeDialogSize", size);
}

// 対局開始ダイアログのウィンドウサイズを取得
QSize startGameDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/startGameDialogSize", QSize(1000, 580)).toSize();
}

// 対局開始ダイアログのウィンドウサイズを保存
void setStartGameDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/startGameDialogSize", size);
}

// 対局開始ダイアログのフォントサイズを取得
int startGameDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/startGameDialog", 9).toInt();
}

// 対局開始ダイアログのフォントサイズを保存
void setStartGameDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/startGameDialog", size);
}

// 棋譜貼り付けダイアログのウィンドウサイズを取得
QSize kifuPasteDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/kifuPasteDialogSize", QSize(600, 500)).toSize();
}

// 棋譜貼り付けダイアログのウィンドウサイズを保存
void setKifuPasteDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/kifuPasteDialogSize", size);
}

// 棋譜貼り付けダイアログのフォントサイズを取得
int kifuPasteDialogFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/kifuPasteDialog", 10).toInt();
}

// 棋譜貼り付けダイアログのフォントサイズを保存
void setKifuPasteDialogFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/kifuPasteDialog", size);
}

// CSA通信ログウィンドウのサイズを取得
QSize csaLogWindowSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/csaLogWindowSize", QSize(550, 450)).toSize();
}

// CSA通信ログウィンドウのサイズを保存
void setCsaLogWindowSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/csaLogWindowSize", size);
}

// 定跡ウィンドウのテーブル列幅を取得
QList<int> josekiWindowColumnWidths()
{
    QSettings& s = openSettings();
    QString key = QStringLiteral("JosekiWindow/columnWidths");
    QList<int> widths;

    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value("width", 0).toInt());
    }
    s.endArray();

    return widths;
}

// 定跡ウィンドウのテーブル列幅を保存
void setJosekiWindowColumnWidths(const QList<int>& widths)
{
    QSettings& s = openSettings();
    QString key = QStringLiteral("JosekiWindow/columnWidths");

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue("width", widths.at(i));
    }
    s.endArray();

    s.sync();
}

// CSA通信対局ダイアログのウィンドウサイズを取得
QSize csaGameDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/csaGameDialogSize", QSize(450, 380)).toSize();
}

// CSA通信対局ダイアログのウィンドウサイズを保存
void setCsaGameDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/csaGameDialogSize", size);
}

// 棋譜解析ダイアログのウィンドウサイズを取得
QSize kifuAnalysisDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/kifuAnalysisDialogSize", QSize(500, 340)).toSize();
}

// 棋譜解析ダイアログのウィンドウサイズを保存
void setKifuAnalysisDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/kifuAnalysisDialogSize", size);
}

// 定跡手編集ダイアログのウィンドウサイズを取得
QSize josekiMoveDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/josekiMoveDialogSize", QSize(500, 600)).toSize();
}

// 定跡手編集ダイアログのウィンドウサイズを保存
void setJosekiMoveDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/josekiMoveDialogSize", size);
}

// 詰将棋局面生成ダイアログのウィンドウサイズを取得
QSize tsumeshogiGeneratorDialogSize()
{
    QSettings& s = openSettings();
    return s.value("SizeRelated/tsumeshogiGeneratorDialogSize", QSize(600, 550)).toSize();
}

// 詰将棋局面生成ダイアログのウィンドウサイズを保存
void setTsumeshogiGeneratorDialogSize(const QSize& size)
{
    QSettings& s = openSettings();
    s.setValue("SizeRelated/tsumeshogiGeneratorDialogSize", size);
}

// 詰将棋局面生成ダイアログのフォントサイズを取得
int tsumeshogiGeneratorFontSize()
{
    QSettings& s = openSettings();
    return s.value("FontSize/tsumeshogiGenerator", 10).toInt();
}

// 詰将棋局面生成ダイアログのフォントサイズを保存
void setTsumeshogiGeneratorFontSize(int size)
{
    QSettings& s = openSettings();
    s.setValue("FontSize/tsumeshogiGenerator", size);
}

// 詰将棋局面生成の最後に選択したエンジン番号を取得
int tsumeshogiGeneratorEngineIndex()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/engineIndex", 0).toInt();
}

// 詰将棋局面生成の最後に選択したエンジン番号を保存
void setTsumeshogiGeneratorEngineIndex(int index)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/engineIndex", index);
}

// 詰将棋局面生成の目標手数を取得
int tsumeshogiGeneratorTargetMoves()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/targetMoves", 3).toInt();
}

// 詰将棋局面生成の目標手数を保存
void setTsumeshogiGeneratorTargetMoves(int moves)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/targetMoves", moves);
}

// 詰将棋局面生成の攻め駒上限を取得
int tsumeshogiGeneratorMaxAttackPieces()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/maxAttackPieces", 4).toInt();
}

// 詰将棋局面生成の攻め駒上限を保存
void setTsumeshogiGeneratorMaxAttackPieces(int count)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/maxAttackPieces", count);
}

// 詰将棋局面生成の守り駒上限を取得
int tsumeshogiGeneratorMaxDefendPieces()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/maxDefendPieces", 1).toInt();
}

// 詰将棋局面生成の守り駒上限を保存
void setTsumeshogiGeneratorMaxDefendPieces(int count)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/maxDefendPieces", count);
}

// 詰将棋局面生成の配置範囲を取得
int tsumeshogiGeneratorAttackRange()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/attackRange", 3).toInt();
}

// 詰将棋局面生成の配置範囲を保存
void setTsumeshogiGeneratorAttackRange(int range)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/attackRange", range);
}

// 詰将棋局面生成の探索時間(秒)を取得
int tsumeshogiGeneratorTimeoutSec()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/timeoutSec", 5).toInt();
}

// 詰将棋局面生成の探索時間(秒)を保存
void setTsumeshogiGeneratorTimeoutSec(int sec)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/timeoutSec", sec);
}

// 詰将棋局面生成の生成上限を取得
int tsumeshogiGeneratorMaxPositions()
{
    QSettings& s = openSettings();
    return s.value("TsumeshogiGenerator/maxPositions", 10).toInt();
}

// 詰将棋局面生成の生成上限を保存
void setTsumeshogiGeneratorMaxPositions(int count)
{
    QSettings& s = openSettings();
    s.setValue("TsumeshogiGenerator/maxPositions", count);
}

} // namespace SettingsService
