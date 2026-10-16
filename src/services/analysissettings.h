#ifndef ANALYSISSETTINGS_H
#define ANALYSISSETTINGS_H

/// @file analysissettings.h
/// @brief 解析・検討設定の永続化
///
/// 評価値グラフ・解析タブ・棋譜解析・検討モード・読み筋盤面に関する設定を提供します。
/// 呼び出し元: evaluationchartwidget.cpp, thinkingpanel.cpp, kifuanalysisdialog.cpp,
///             analysisresultspanel.cpp, considerationtabmanager.cpp,
///             pvboarddialog.cpp

#include <QSize>
#include <QList>
#include <QString>
#include "setting.h"

namespace AnalysisSettings {

// --- 分岐ツリー ---

/// 分岐ツリーの表示倍率（%、デフォルト: 100）
inline const Setting<int> branchTreeZoomPercent{"BranchTree/zoomPercent", 100};

/// 変化を空いた段に詰めて表示するか（デフォルト: true）
inline const Setting<bool> branchTreeCompactLayout{"BranchTree/compactLayout", true};

// --- 評価値グラフ ---

/// 評価値上限（デフォルト: 2000）
inline const Setting<int> evalChartYLimit{"EvalChart/yLimit", 2000};

/// 手数上限（デフォルト: 500）
inline const Setting<int> evalChartXLimit{"EvalChart/xLimit", 500};

/// 手数間隔（デフォルト: 0 = 自動）
inline const Setting<int> evalChartXInterval{"EvalChart/xInterval", 0};

/// 軸ラベルフォントサイズ（デフォルト: 10）
inline const Setting<int> evalChartLabelFontSize{"EvalChart/labelFontSize", 10};

inline const Setting<bool> evalChartAutomaticRange{"EvalChart/automaticRange", true};
/// 0 は画面サイズに応じた自動目盛り
inline const Setting<int> evalChartYInterval{"EvalChart/yInterval", 0};
inline const Setting<QSize> evalChartSettingsSize{"EvalChart/settingsSize", QSize(420, 320)};

// --- 解析タブ ---

/// 思考の表とその上のエンジン情報欄の組。列幅は組ごとに保存する
enum class ThinkingTable { Engine1 = 0, Engine2 = 1, Consideration = 2 };

/// エンジン情報欄の列幅
QList<int> engineInfoColumnWidths(ThinkingTable table);
void setEngineInfoColumnWidths(ThinkingTable table, const QList<int>& widths);

/// 思考の表（読み筋の表）の列幅
QList<int> thinkingViewColumnWidths(ThinkingTable table);
void setThinkingViewColumnWidths(ThinkingTable table, const QList<int>& widths);

/// USI通信ログタブのフォントサイズ（デフォルト: 10）
inline const Setting<int> usiLogFontSize{"FontSize/usiLog", 10};

inline const Setting<bool> usiLogWrapLines{"UsiLog/wrapLines", false};
/// コマンド送信先（0=E1、1=E2、2=両方）
int usiLogCommandTarget();
void setUsiLogCommandTarget(int target);

/// 思考タブのフォントサイズ（デフォルト: 10）
inline const Setting<int> thinkingFontSize{"FontSize/thinking", 10};

inline const Setting<bool> matchArrowsVisible{"ThinkingView/matchArrowsVisible", false};
inline const Setting<bool> ponderArrowsVisible{"ThinkingView/ponderArrowsVisible", false};

// --- 棋譜解析 ---

/// 棋譜解析ダイアログのフォントサイズ（0 はアプリケーションの文字サイズを使用）
inline const Setting<int> kifuAnalysisFontSize{"KifuAnalysis/fontSize", 0};

/// 解析結果の先頭7列の幅（0は内容に合わせて調整）
QList<int> kifuAnalysisColumnWidths();
void setKifuAnalysisColumnWidths(const QList<int>& widths);

/// 1手あたりの思考時間・秒（デフォルト: 3）
inline const Setting<int> kifuAnalysisByoyomiSec{"KifuAnalysis/byoyomiSec", 3};


/// 解析範囲: 全局面解析（デフォルト: true）
inline const Setting<bool> kifuAnalysisFullRange{"KifuAnalysis/fullRange", true};

/// 解析範囲: 開始手数（デフォルト: 0）
inline const Setting<int> kifuAnalysisStartPly{"KifuAnalysis/startPly", 0};

/// 解析範囲: 終了手数（デフォルト: -1 = 最終局面）
inline const Setting<int> kifuAnalysisEndPly{"KifuAnalysis/endPly", -1};

/// 棋譜解析ダイアログのウィンドウサイズ（デフォルト: 560x380）
inline const Setting<QSize> kifuAnalysisDialogSize{"SizeRelated/kifuAnalysisDialogSize", QSize(560, 380)};

// --- 検討モード ---


/// 検討タブの時間無制限フラグ（デフォルト: true）
inline const Setting<bool> considerationUnlimitedTime{"Consideration/unlimitedTime", true};

/// 検討タブの検討時間・秒（デフォルト: 0）
inline const Setting<int> considerationByoyomiSec{"Consideration/byoyomiSec", 0};

/// 検討の候補手の数（デフォルト: 1）
inline const Setting<int> considerationMultiPV{"Consideration/multiPV", 1};

/// 検討タブのフォントサイズ（デフォルト: 10）
inline const Setting<int> considerationFontSize{"ConsiderationTab/fontSize", 10};

// --- 詰み探索（検討とは独立して保存） ---


/// 探索時間（デフォルト: 時間制限あり・20秒）
inline const Setting<bool> tsumeSearchUnlimitedTime{"TsumeSearch/unlimitedTime", false};
inline const Setting<int> tsumeSearchTimeLimitSec{"TsumeSearch/timeLimitSec", 20};

/// 0 はアプリケーションの文字サイズを使用
inline const Setting<int> tsumeSearchFontSize{"TsumeSearch/fontSize", 0};
inline const Setting<QSize> tsumeSearchDialogSize{"TsumeSearch/dialogSize", QSize(560, 360)};

// --- 読み筋盤面 ---

/// 読み筋表示ウィンドウのサイズ（デフォルト: 620x720）
inline const Setting<QSize> pvBoardDialogSize{"SizeRelated/pvBoardDialogSize", QSize(620, 720)};

} // namespace AnalysisSettings

#endif // ANALYSISSETTINGS_H
