#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QWidget>
#include "shogiview.h"

namespace SettingsService {

static const char* kIniName = "ShogiBoardQ.ini";

void loadWindowSize(QWidget* mainWindow)
{
    if (!mainWindow) return;
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    const QSize sz = s.value("SizeRelated/mainWindowSize", QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, ShogiView* view)
{
    if (!mainWindow || !view) return;
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.beginGroup("SizeRelated");
    s.setValue("mainWindowSize", mainWindow->size());
    s.setValue("squareSize",     view->squareSize());
    s.endGroup();
}

QString lastKifuDirectory()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("General/lastKifuDirectory", QString()).toString();
}

void setLastKifuDirectory(const QString& dir)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("General/lastKifuDirectory", dir);
}

// ★ 追加: USI通信ログタブのフォントサイズを取得
int usiLogFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/usiLog", 10).toInt();
}

// ★ 追加: USI通信ログタブのフォントサイズを保存
void setUsiLogFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/usiLog", size);
}

// ★ 追加: 棋譜コメントタブのフォントサイズを取得
int commentFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/comment", 10).toInt();
}

// ★ 追加: 棋譜コメントタブのフォントサイズを保存
void setCommentFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/comment", size);
}

// ★ 追加: 対局情報タブのフォントサイズを取得
int gameInfoFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/gameInfo", 10).toInt();
}

// ★ 追加: 対局情報タブのフォントサイズを保存
void setGameInfoFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/gameInfo", size);
}

// ★ 追加: 思考タブのフォントサイズを取得
int thinkingFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/thinking", 10).toInt();
}

// ★ 追加: 思考タブのフォントサイズを保存
void setThinkingFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/thinking", size);
}

// ★ 追加: 最後に選択されたタブインデックスを取得
int lastSelectedTabIndex()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("UI/lastSelectedTabIndex", 0).toInt();
}

// ★ 追加: 最後に選択されたタブインデックスを保存
void setLastSelectedTabIndex(int index)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("UI/lastSelectedTabIndex", index);
}

// ★ 追加: 読み筋表示ウィンドウのサイズを取得
QSize pvBoardDialogSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("SizeRelated/pvBoardDialogSize", QSize(620, 720)).toSize();
}

// ★ 追加: 読み筋表示ウィンドウのサイズを保存
void setPvBoardDialogSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("SizeRelated/pvBoardDialogSize", size);
}

// ★ 追加: 評価値グラフの設定
int evalChartYLimit()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChart/yLimit", 2000).toInt();
}

void setEvalChartYLimit(int limit)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChart/yLimit", limit);
}

int evalChartXLimit()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChart/xLimit", 500).toInt();
}

void setEvalChartXLimit(int limit)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChart/xLimit", limit);
}

int evalChartXInterval()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChart/xInterval", 10).toInt();
}

void setEvalChartXInterval(int interval)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChart/xInterval", interval);
}

int evalChartLabelFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChart/labelFontSize", 7).toInt();
}

void setEvalChartLabelFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChart/labelFontSize", size);
}

// ★ 追加: エンジン情報ウィジェットの列幅を取得
QList<int> engineInfoColumnWidths(int widgetIndex)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
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

// ★ 追加: エンジン情報ウィジェットの列幅を保存
void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
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

// ★ 追加: 思考タブ下段（読み筋テーブル）の列幅を取得
QList<int> thinkingViewColumnWidths(int viewIndex)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
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

// ★ 追加: 思考タブ下段（読み筋テーブル）の列幅を保存
void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
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

// ★ 追加: 棋譜欄・分岐候補欄のフォントサイズを取得
int kifuPaneFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/kifuPane", 10).toInt();
}

// ★ 追加: 棋譜欄・分岐候補欄のフォントサイズを保存
void setKifuPaneFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/kifuPane", size);
}

// ★ 追加: 棋譜解析ダイアログのフォントサイズを取得
int kifuAnalysisFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/fontSize", 10).toInt();
}

// ★ 追加: 棋譜解析ダイアログのフォントサイズを保存
void setKifuAnalysisFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/fontSize", size);
}

// ★ 追加: 棋譜解析結果ウィンドウのサイズを取得
QSize kifuAnalysisResultsWindowSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/resultsWindowSize", QSize(1100, 600)).toSize();
}

// ★ 追加: 棋譜解析結果ウィンドウのサイズを保存
void setKifuAnalysisResultsWindowSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/resultsWindowSize", size);
}

// ★ 追加: 棋譜解析ダイアログの1手あたりの思考時間（秒）を取得
int kifuAnalysisByoyomiSec()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/byoyomiSec", 3).toInt();
}

// ★ 追加: 棋譜解析ダイアログの1手あたりの思考時間（秒）を保存
void setKifuAnalysisByoyomiSec(int sec)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/byoyomiSec", sec);
}

// ★ 追加: 棋譜解析ダイアログの最後に選択したエンジン番号を取得
int kifuAnalysisEngineIndex()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/engineIndex", 0).toInt();
}

// ★ 追加: 棋譜解析ダイアログの最後に選択したエンジン番号を保存
void setKifuAnalysisEngineIndex(int index)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/engineIndex", index);
}

// ★ 追加: 棋譜解析ダイアログの解析範囲（全局面解析かどうか）を取得
bool kifuAnalysisFullRange()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/fullRange", true).toBool();
}

// ★ 追加: 棋譜解析ダイアログの解析範囲（全局面解析かどうか）を保存
void setKifuAnalysisFullRange(bool fullRange)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/fullRange", fullRange);
}

// ★ 追加: 棋譜解析ダイアログの解析範囲（開始手数）を取得
int kifuAnalysisStartPly()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/startPly", 0).toInt();
}

// ★ 追加: 棋譜解析ダイアログの解析範囲（開始手数）を保存
void setKifuAnalysisStartPly(int ply)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/startPly", ply);
}

// ★ 追加: 棋譜解析ダイアログの解析範囲（終了手数）を取得
int kifuAnalysisEndPly()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("KifuAnalysis/endPly", 0).toInt();
}

// ★ 追加: 棋譜解析ダイアログの解析範囲（終了手数）を保存
void setKifuAnalysisEndPly(int ply)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("KifuAnalysis/endPly", ply);
}

// ★ 追加: 定跡ウィンドウのフォントサイズを取得
int josekiWindowFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/fontSize", 10).toInt();
}

// ★ 追加: 定跡ウィンドウのフォントサイズを保存
void setJosekiWindowFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/fontSize", size);
}

// ★ 追加: 定跡ウィンドウのSFEN表示フォントサイズを取得
int josekiWindowSfenFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/sfenFontSize", 9).toInt();
}

// ★ 追加: 定跡ウィンドウのSFEN表示フォントサイズを保存
void setJosekiWindowSfenFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/sfenFontSize", size);
}

// ★ 追加: 定跡ウィンドウの最後に開いた定跡ファイルパスを取得
QString josekiWindowLastFilePath()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/lastFilePath", QString()).toString();
}

// ★ 追加: 定跡ウィンドウの最後に開いた定跡ファイルパスを保存
void setJosekiWindowLastFilePath(const QString& path)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/lastFilePath", path);
}

// ★ 追加: 定跡ウィンドウのサイズを取得
QSize josekiWindowSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/size", QSize(800, 500)).toSize();
}

// ★ 追加: 定跡ウィンドウのサイズを保存
void setJosekiWindowSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/size", size);
}

// ★ 追加: 定跡ファイル自動読込が有効かどうかを取得
bool josekiWindowAutoLoadEnabled()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/autoLoadEnabled", true).toBool();
}

// ★ 追加: 定跡ファイル自動読込が有効かどうかを保存
void setJosekiWindowAutoLoadEnabled(bool enabled)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/autoLoadEnabled", enabled);
}

// ★ 追加: 定跡ウィンドウの最近使ったファイルリストを取得
QStringList josekiWindowRecentFiles()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/recentFiles", QStringList()).toStringList();
}

// ★ 追加: 定跡ウィンドウの最近使ったファイルリストを保存
void setJosekiWindowRecentFiles(const QStringList& files)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/recentFiles", files);
}

// ★ 追加: 定跡手編集ダイアログのフォントサイズを取得
int josekiMoveDialogFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JosekiWindow/moveDialogFontSize", 10).toInt();
}

// ★ 追加: 定跡手編集ダイアログのフォントサイズを保存
void setJosekiMoveDialogFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JosekiWindow/moveDialogFontSize", size);
}

// ★ 追加: CSA通信ログのフォントサイズを取得
int csaLogFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/csaLog", 10).toInt();
}

// ★ 追加: CSA通信ログのフォントサイズを保存
void setCsaLogFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/csaLog", size);
}

// ★ 追加: CSA待機ダイアログのフォントサイズを取得
int csaWaitingDialogFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/csaWaitingDialog", 10).toInt();
}

// ★ 追加: CSA待機ダイアログのフォントサイズを保存
void setCsaWaitingDialogFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/csaWaitingDialog", size);
}

// ★ 追加: CSA通信対局ダイアログのフォントサイズを取得
int csaGameDialogFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/csaGameDialog", 10).toInt();
}

// ★ 追加: CSA通信対局ダイアログのフォントサイズを保存
void setCsaGameDialogFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/csaGameDialog", size);
}

// ★ 追加: 持将棋の点数ダイアログのフォントサイズを取得
int jishogiScoreFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/jishogiScore", 10).toInt();
}

// ★ 追加: 持将棋の点数ダイアログのフォントサイズを保存
void setJishogiScoreFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/jishogiScore", size);
}

// ★ 追加: 持将棋の点数ダイアログのウィンドウサイズを取得
QSize jishogiScoreDialogSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("JishogiScore/dialogSize", QSize(250, 280)).toSize();
}

// ★ 追加: 持将棋の点数ダイアログのウィンドウサイズを保存
void setJishogiScoreDialogSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("JishogiScore/dialogSize", size);
}

// ★ 追加: 言語コードを取得
QString language()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("General/language", "system").toString();
}

// ★ 追加: 言語コードを保存
void setLanguage(const QString& lang)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("General/language", lang);
}

// ★ 追加: メニューウィンドウのお気に入りアクションリストを取得
QStringList menuWindowFavorites()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindow/favorites", QStringList()).toStringList();
}

// ★ 追加: メニューウィンドウのお気に入りアクションリストを保存
void setMenuWindowFavorites(const QStringList& favorites)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindow/favorites", favorites);
}

// ★ 追加: メニューウィンドウのサイズを取得
QSize menuWindowSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindow/size", QSize(500, 400)).toSize();
}

// ★ 追加: メニューウィンドウのサイズを保存
void setMenuWindowSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindow/size", size);
}

// ★ 追加: メニューウィンドウのボタンサイズを取得
int menuWindowButtonSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindow/buttonSize", 72).toInt();
}

// ★ 追加: メニューウィンドウのボタンサイズを保存
void setMenuWindowButtonSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindow/buttonSize", size);
}

// ★ 追加: メニューウィンドウのフォントサイズを取得
int menuWindowFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindow/fontSize", 9).toInt();
}

// ★ 追加: メニューウィンドウのフォントサイズを保存
void setMenuWindowFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindow/fontSize", size);
}

// ★ 追加: ツールバーの表示状態を取得
bool toolbarVisible()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("UI/toolbarVisible", true).toBool();
}

// ★ 追加: ツールバーの表示状態を保存
void setToolbarVisible(bool visible)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("UI/toolbarVisible", visible);
}

// ★ 追加: エンジン設定ダイアログのフォントサイズを取得
int engineSettingsFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/engineSettings", 12).toInt();
}

// ★ 追加: エンジン設定ダイアログのフォントサイズを保存
void setEngineSettingsFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/engineSettings", size);
}

// ★ 追加: エンジン設定ダイアログのウィンドウサイズを取得
QSize engineSettingsDialogSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EngineSettings/dialogSize", QSize(800, 800)).toSize();
}

// ★ 追加: エンジン設定ダイアログのウィンドウサイズを保存
void setEngineSettingsDialogSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EngineSettings/dialogSize", size);
}

// ★ 追加: エンジン登録ダイアログのフォントサイズを取得
int engineRegistrationFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/engineRegistration", 12).toInt();
}

// ★ 追加: エンジン登録ダイアログのフォントサイズを保存
void setEngineRegistrationFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/engineRegistration", size);
}

// ★ 追加: エンジン登録ダイアログのウィンドウサイズを取得
QSize engineRegistrationDialogSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EngineRegistration/dialogSize", QSize(647, 421)).toSize();
}

// ★ 追加: エンジン登録ダイアログのウィンドウサイズを保存
void setEngineRegistrationDialogSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EngineRegistration/dialogSize", size);
}

// ★ 追加: 検討ダイアログのフォントサイズを取得
int considerationDialogFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("FontSize/considerationDialog", 10).toInt();
}

// ★ 追加: 検討ダイアログのフォントサイズを保存
void setConsiderationDialogFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("FontSize/considerationDialog", size);
}

// ★ 追加: 検討ダイアログの最後に選択したエンジン番号を取得
int considerationEngineIndex()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("Consideration/engineIndex", 0).toInt();
}

// ★ 追加: 検討ダイアログの最後に選択したエンジン番号を保存
void setConsiderationEngineIndex(int index)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("Consideration/engineIndex", index);
}

// ★ 追加: 検討ダイアログの時間無制限フラグを取得
bool considerationUnlimitedTime()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("Consideration/unlimitedTime", true).toBool();
}

// ★ 追加: 検討ダイアログの時間無制限フラグを保存
void setConsiderationUnlimitedTime(bool unlimited)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("Consideration/unlimitedTime", unlimited);
}

// ★ 追加: 検討ダイアログの検討時間（秒）を取得
int considerationByoyomiSec()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("Consideration/byoyomiSec", 0).toInt();
}

// ★ 追加: 検討ダイアログの検討時間（秒）を保存
void setConsiderationByoyomiSec(int sec)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("Consideration/byoyomiSec", sec);
}

// ★ 追加: 検討の候補手の数を取得
int considerationMultiPV()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("Consideration/multiPV", 1).toInt();
}

// ★ 追加: 検討の候補手の数を保存
void setConsiderationMultiPV(int multiPV)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("Consideration/multiPV", multiPV);
}

// ★ 追加: 検討タブのフォントサイズを取得
int considerationFontSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("ConsiderationTab/fontSize", 10).toInt();
}

// ★ 追加: 検討タブのフォントサイズを保存
void setConsiderationFontSize(int size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("ConsiderationTab/fontSize", size);
}

// ★ 追加: 評価値グラフドックの状態を取得
QByteArray evalChartDockState()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChartDock/state", QByteArray()).toByteArray();
}

// ★ 追加: 評価値グラフドックの状態を保存
void setEvalChartDockState(const QByteArray& state)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChartDock/state", state);
}

// ★ 追加: 評価値グラフドックのフローティング状態を取得
bool evalChartDockFloating()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChartDock/floating", false).toBool();
}

// ★ 追加: 評価値グラフドックのフローティング状態を保存
void setEvalChartDockFloating(bool floating)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChartDock/floating", floating);
}

// ★ 追加: 評価値グラフドックのジオメトリを取得
QByteArray evalChartDockGeometry()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChartDock/geometry", QByteArray()).toByteArray();
}

// ★ 追加: 評価値グラフドックのジオメトリを保存
void setEvalChartDockGeometry(const QByteArray& geometry)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChartDock/geometry", geometry);
}

// ★ 追加: 評価値グラフドックの表示状態を取得
bool evalChartDockVisible()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("EvalChartDock/visible", true).toBool();
}

// ★ 追加: 評価値グラフドックの表示状態を保存
void setEvalChartDockVisible(bool visible)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("EvalChartDock/visible", visible);
}

// ★ 追加: 棋譜欄ドックのフローティング状態を取得
bool recordPaneDockFloating()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("RecordPaneDock/floating", false).toBool();
}

// ★ 追加: 棋譜欄ドックのフローティング状態を保存
void setRecordPaneDockFloating(bool floating)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("RecordPaneDock/floating", floating);
}

// ★ 追加: 棋譜欄ドックのジオメトリを取得
QByteArray recordPaneDockGeometry()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("RecordPaneDock/geometry", QByteArray()).toByteArray();
}

// ★ 追加: 棋譜欄ドックのジオメトリを保存
void setRecordPaneDockGeometry(const QByteArray& geometry)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("RecordPaneDock/geometry", geometry);
}

// ★ 追加: 棋譜欄ドックの表示状態を取得
bool recordPaneDockVisible()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("RecordPaneDock/visible", true).toBool();
}

// ★ 追加: 棋譜欄ドックの表示状態を保存
void setRecordPaneDockVisible(bool visible)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("RecordPaneDock/visible", visible);
}

// ★ 追加: 解析タブドックのフローティング状態を取得
bool analysisTabDockFloating()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("AnalysisTabDock/floating", false).toBool();
}

// ★ 追加: 解析タブドックのフローティング状態を保存
void setAnalysisTabDockFloating(bool floating)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("AnalysisTabDock/floating", floating);
}

// ★ 追加: 解析タブドックのジオメトリを取得
QByteArray analysisTabDockGeometry()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("AnalysisTabDock/geometry", QByteArray()).toByteArray();
}

// ★ 追加: 解析タブドックのジオメトリを保存
void setAnalysisTabDockGeometry(const QByteArray& geometry)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("AnalysisTabDock/geometry", geometry);
}

// ★ 追加: 解析タブドックの表示状態を取得
bool analysisTabDockVisible()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("AnalysisTabDock/visible", true).toBool();
}

// ★ 追加: 解析タブドックの表示状態を保存
void setAnalysisTabDockVisible(bool visible)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("AnalysisTabDock/visible", visible);
}

// ★ 追加: 将棋盤ドックのフローティング状態を取得
bool boardDockFloating()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("BoardDock/floating", false).toBool();
}

// ★ 追加: 将棋盤ドックのフローティング状態を保存
void setBoardDockFloating(bool floating)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("BoardDock/floating", floating);
}

// ★ 追加: 将棋盤ドックのジオメトリを取得
QByteArray boardDockGeometry()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("BoardDock/geometry", QByteArray()).toByteArray();
}

// ★ 追加: 将棋盤ドックのジオメトリを保存
void setBoardDockGeometry(const QByteArray& geometry)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("BoardDock/geometry", geometry);
}

// ★ 追加: 将棋盤ドックの表示状態を取得
bool boardDockVisible()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("BoardDock/visible", true).toBool();
}

// ★ 追加: 将棋盤ドックの表示状態を保存
void setBoardDockVisible(bool visible)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("BoardDock/visible", visible);
}

// ★ 追加: メニューウィンドウドックのフローティング状態を取得
bool menuWindowDockFloating()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindowDock/floating", false).toBool();
}

// ★ 追加: メニューウィンドウドックのフローティング状態を保存
void setMenuWindowDockFloating(bool floating)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindowDock/floating", floating);
}

// ★ 追加: メニューウィンドウドックのジオメトリを取得
QByteArray menuWindowDockGeometry()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindowDock/geometry", QByteArray()).toByteArray();
}

// ★ 追加: メニューウィンドウドックのジオメトリを保存
void setMenuWindowDockGeometry(const QByteArray& geometry)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindowDock/geometry", geometry);
}

// ★ 追加: メニューウィンドウドックの表示状態を取得
bool menuWindowDockVisible()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MenuWindowDock/visible", false).toBool();  // デフォルトは非表示
}

// ★ 追加: メニューウィンドウドックの表示状態を保存
void setMenuWindowDockVisible(bool visible)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MenuWindowDock/visible", visible);
}

// ★ 追加: 保存されているカスタムドックレイアウト名のリストを取得
QStringList savedDockLayoutNames()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.beginGroup("CustomDockLayouts");
    QStringList names = s.childKeys();
    s.endGroup();
    return names;
}

// ★ 追加: 指定した名前でドックレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("CustomDockLayouts/" + name, state);
}

// ★ 追加: 指定した名前のドックレイアウトを取得
QByteArray loadDockLayout(const QString& name)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("CustomDockLayouts/" + name, QByteArray()).toByteArray();
}

// ★ 追加: 指定した名前のドックレイアウトを削除
void deleteDockLayout(const QString& name)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.remove("CustomDockLayouts/" + name);
}

// ★ 追加: 起動時に使用するレイアウト名を取得
QString startupDockLayoutName()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("DockLayout/startupLayoutName", QString()).toString();
}

// ★ 追加: 起動時に使用するレイアウト名を保存
void setStartupDockLayoutName(const QString& name)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("DockLayout/startupLayoutName", name);
}

} // namespace SettingsService
