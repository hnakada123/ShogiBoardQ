#ifndef ANALYSISSETTINGS_H
#define ANALYSISSETTINGS_H

/// @file analysissettings.h
/// @brief 解析・検討設定の永続化
///
/// 評価値グラフ・解析タブ・棋譜解析・検討モード・読み筋盤面に関する設定を提供します。
/// 呼び出し元: evaluationchartwidget.cpp, engineanalysistab.cpp, kifuanalysisdialog.cpp,
///             analysisresultspresenter.cpp, considerationdialog.cpp, considerationtabmanager.cpp,
///             pvboarddialog.cpp

#include <QSize>
#include <QList>

namespace AnalysisSettings {

// --- 評価値グラフ ---

/// 評価値上限（デフォルト: 2000）
int evalChartYLimit();
void setEvalChartYLimit(int limit);

/// 手数上限（デフォルト: 500）
int evalChartXLimit();
void setEvalChartXLimit(int limit);

/// 手数間隔（デフォルト: 10）
int evalChartXInterval();
void setEvalChartXInterval(int interval);

/// 軸ラベルフォントサイズ（デフォルト: 7）
int evalChartLabelFontSize();
void setEvalChartLabelFontSize(int size);

// --- 解析タブ ---

/// エンジン情報ウィジェットの列幅（widgetIndex: 0=上段, 1=下段）
QList<int> engineInfoColumnWidths(int widgetIndex);
void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths);

/// 思考タブ下段（読み筋テーブル）の列幅（viewIndex: 0=エンジン1, 1=エンジン2）
QList<int> thinkingViewColumnWidths(int viewIndex);
void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths);

/// USI通信ログタブのフォントサイズ（デフォルト: 10）
int usiLogFontSize();
void setUsiLogFontSize(int size);

/// 思考タブのフォントサイズ（デフォルト: 10）
int thinkingFontSize();
void setThinkingFontSize(int size);

// --- 棋譜解析 ---

/// 棋譜解析ダイアログのフォントサイズ（デフォルト: 10）
int kifuAnalysisFontSize();
void setKifuAnalysisFontSize(int size);

/// 棋譜解析結果ウィンドウのサイズ（デフォルト: 1100x600）
QSize kifuAnalysisResultsWindowSize();
void setKifuAnalysisResultsWindowSize(const QSize& size);

/// 1手あたりの思考時間・秒（デフォルト: 3）
int kifuAnalysisByoyomiSec();
void setKifuAnalysisByoyomiSec(int sec);

/// 最後に選択したエンジン番号（デフォルト: 0）
int kifuAnalysisEngineIndex();
void setKifuAnalysisEngineIndex(int index);

/// 解析範囲: 全局面解析（デフォルト: true）
bool kifuAnalysisFullRange();
void setKifuAnalysisFullRange(bool fullRange);

/// 解析範囲: 開始手数（デフォルト: 0）
int kifuAnalysisStartPly();
void setKifuAnalysisStartPly(int ply);

/// 解析範囲: 終了手数（デフォルト: 0）
int kifuAnalysisEndPly();
void setKifuAnalysisEndPly(int ply);

/// 棋譜解析ダイアログのウィンドウサイズ（デフォルト: 500x340）
QSize kifuAnalysisDialogSize();
void setKifuAnalysisDialogSize(const QSize& size);

// --- 検討モード ---

/// 検討ダイアログのフォントサイズ（デフォルト: 10）
int considerationDialogFontSize();
void setConsiderationDialogFontSize(int size);

/// 検討ダイアログの最後に選択したエンジン番号（デフォルト: 0）
int considerationEngineIndex();
void setConsiderationEngineIndex(int index);

/// 検討ダイアログの時間無制限フラグ（デフォルト: true）
bool considerationUnlimitedTime();
void setConsiderationUnlimitedTime(bool unlimited);

/// 検討ダイアログの検討時間・秒（デフォルト: 0）
int considerationByoyomiSec();
void setConsiderationByoyomiSec(int sec);

/// 検討の候補手の数（デフォルト: 1）
int considerationMultiPV();
void setConsiderationMultiPV(int multiPV);

/// 検討タブのフォントサイズ（デフォルト: 10）
int considerationFontSize();
void setConsiderationFontSize(int size);

// --- 読み筋盤面 ---

/// 読み筋表示ウィンドウのサイズ（デフォルト: 620x720）
QSize pvBoardDialogSize();
void setPvBoardDialogSize(const QSize& size);

} // namespace AnalysisSettings

#endif // ANALYSISSETTINGS_H
