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

} // namespace SettingsService
