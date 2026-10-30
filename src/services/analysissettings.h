#ifndef ANALYSISSETTINGS_H
#define ANALYSISSETTINGS_H

/// @file analysissettings.h
/// @brief 解析・検討設定の永続化
///
/// 評価値グラフ・解析タブ・棋譜解析・検討モード・読み筋盤面に関する設定を提供します。

#include <QMetaType>
#include <QSize>
#include <QList>
#include <QString>
#include <QtGlobal>
#include "setting.h"

/// 評価値グラフの表示の設定（保存する値。値の範囲はここだけで決める）
struct EvaluationChartSettings {
    static constexpr int kMinYLimit = 100;
    static constexpr int kMaxYLimit = 100000;
    static constexpr int kMinXLimit = 1;
    static constexpr int kMaxXLimit = 10000;
    static constexpr int kMaxYInterval = 100000;
    static constexpr int kMaxXInterval = 10000;
    static constexpr int kMinFontSize = 8;
    static constexpr int kMaxFontSize = 18;

    bool automatic = true;   ///< 表示範囲を点に合わせる（false なら yLimit・xLimit で固定）
    int yLimit = 2000;       ///< 固定の評価値の範囲（±）
    int xLimit = 500;        ///< 固定の手数の上限
    int yInterval = 0;       ///< 評価値の目盛り間隔の希望（0 は自動）
    int xInterval = 0;       ///< 手数の目盛り間隔の希望（0 は自動）
    int labelFontSize = 10;  ///< 軸の文字の大きさ

    /// 範囲に収めた値
    EvaluationChartSettings normalized() const
    {
        EvaluationChartSettings result = *this;
        result.yLimit = qBound(kMinYLimit, yLimit, kMaxYLimit);
        result.xLimit = qBound(kMinXLimit, xLimit, kMaxXLimit);
        result.yInterval = qBound(0, yInterval, kMaxYInterval);
        result.xInterval = qBound(0, xInterval, kMaxXInterval);
        result.labelFontSize = qBound(kMinFontSize, labelFontSize, kMaxFontSize);
        return result;
    }
    static EvaluationChartSettings normalizedOf(const EvaluationChartSettings& settings) { return settings.normalized(); }

    friend bool operator==(const EvaluationChartSettings& a, const EvaluationChartSettings& b)
    {
        return a.automatic == b.automatic && a.yLimit == b.yLimit && a.xLimit == b.xLimit && a.yInterval == b.yInterval
            && a.xInterval == b.xInterval && a.labelFontSize == b.labelFontSize;
    }
    friend bool operator!=(const EvaluationChartSettings& a, const EvaluationChartSettings& b) { return !(a == b); }
};

/// 検討の条件（検討タブで選ぶ。値の範囲はここだけで決める）。検討タブはこの値を正本として持ち、変わると知らせる
struct ConsiderationSettings {
    static constexpr int kMinByoyomiSec = 1;
    static constexpr int kMaxByoyomiSec = 3600;
    static constexpr int kMaxMultiPV = 10;

    /// 検討に使うエンジン（登録名。保存は検討タブのエンジンの選択 EngineListSettings::considerationSelection）
    QString engineName;
    bool unlimitedTime = true;   ///< 時間無制限で検討する
    int byoyomiSec = 20;         ///< 時間を決めるときの1局面の検討時間（秒）
    int multiPV = 1;             ///< 候補手の数（1〜kMaxMultiPV）

    /// 1局面の検討時間（ms。0 は無制限）
    int byoyomiMs() const { return unlimitedTime ? 0 : byoyomiSec * 1000; }

    /// 範囲に収めた値
    ConsiderationSettings normalized() const
    {
        ConsiderationSettings result = *this;
        result.byoyomiSec = qBound(kMinByoyomiSec, byoyomiSec, kMaxByoyomiSec);
        result.multiPV = qBound(1, multiPV, kMaxMultiPV);
        return result;
    }
    static ConsiderationSettings normalizedOf(const ConsiderationSettings& settings) { return settings.normalized(); }

    friend bool operator==(const ConsiderationSettings& a, const ConsiderationSettings& b)
    {
        return a.engineName == b.engineName && a.unlimitedTime == b.unlimitedTime && a.byoyomiSec == b.byoyomiSec
            && a.multiPV == b.multiPV;
    }
    friend bool operator!=(const ConsiderationSettings& a, const ConsiderationSettings& b) { return !(a == b); }
};

Q_DECLARE_METATYPE(ConsiderationSettings)

namespace AnalysisSettings {

// --- 分岐ツリー ---

/// 分岐ツリーの表示倍率（%、デフォルト: 100。50〜200）
inline const BoundedSetting<int> branchTreeZoomPercent{"BranchTree/zoomPercent", 100, 50, 200};

/// 変化を空いた段に詰めて表示するか（デフォルト: true）
inline const Setting<bool> branchTreeCompactLayout{"BranchTree/compactLayout", true};

// --- 評価値グラフ ---

/// 評価値グラフの表示の設定（キーは以前の版と同じ。範囲は EvaluationChartSettings で決める）
inline const StructSetting<EvaluationChartSettings> evalChart{
    {SettingField<EvaluationChartSettings>::of("EvalChart/automaticRange", &EvaluationChartSettings::automatic),
     SettingField<EvaluationChartSettings>::of("EvalChart/yLimit", &EvaluationChartSettings::yLimit),
     SettingField<EvaluationChartSettings>::of("EvalChart/xLimit", &EvaluationChartSettings::xLimit),
     SettingField<EvaluationChartSettings>::of("EvalChart/yInterval", &EvaluationChartSettings::yInterval),
     SettingField<EvaluationChartSettings>::of("EvalChart/xInterval", &EvaluationChartSettings::xInterval),
     SettingField<EvaluationChartSettings>::of("EvalChart/labelFontSize", &EvaluationChartSettings::labelFontSize)},
    &EvaluationChartSettings::normalizedOf};
/// 表示設定ダイアログの大きさ
inline const Setting<QSize> evalChartSettingsSize{"EvalChart/settingsSize", QSize(420, 320)};

// --- 解析タブ ---

/// 思考の表とその上のエンジン情報欄の組。列幅は組ごとに保存する
enum class ThinkingTable { Engine1 = 0, Engine2 = 1, Consideration = 2 };

/// エンジン情報欄の列幅（列ごと。0 は自動。読み書きは widgets の ColumnWidthMemory）
ListSetting<int> engineInfoColumnWidths(ThinkingTable table);

/// 思考の表（読み筋の表）の列幅（列ごと。0 は自動。読み筋の列は表示領域に合わせるので使わない）
ListSetting<int> thinkingViewColumnWidths(ThinkingTable table);

/// USI通信ログタブのフォントサイズ（デフォルト: 10）
inline const Setting<int> usiLogFontSize{"FontSize/usiLog", 10};

inline const Setting<bool> usiLogWrapLines{"UsiLog/wrapLines", false};
/// コマンド送信先（0=E1、1=E2、2=両方）
inline const BoundedSetting<int> usiLogCommandTarget{"UsiLog/commandTarget", 0, 0, 2};

/// 思考タブのフォントサイズ（デフォルト: 10）
inline const Setting<int> thinkingFontSize{"FontSize/thinking", 10};

/// 対局のエンジンの手番側の読みを盤に矢印で出す（思考タブの「対局中の矢印表示」。デフォルト: false）
inline const Setting<bool> matchArrowsVisible{"ThinkingView/matchArrowsVisible", false};
/// 対局のエンジンの先読み側の読みも盤に矢印で出す（思考タブの「先読み側も表示」。デフォルト: false）
inline const Setting<bool> ponderArrowsVisible{"ThinkingView/ponderArrowsVisible", false};

// --- 棋譜解析 ---

/// 棋譜解析ダイアログのフォントサイズ（0 はアプリケーションの文字サイズを使用）
inline const Setting<int> kifuAnalysisFontSize{"KifuAnalysis/fontSize", 0};

/// 解析結果の表の読み筋より前の列の幅（列ごと。0 は内容に合わせる）。
/// 以前の版が1つの値（QVariantList）で保存していれば、初めて読むときに配列に移す
ListSetting<int> kifuAnalysisColumnWidths();

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


/// 検討の条件（エンジンを除く。キーは以前の版と同じ。範囲は ConsiderationSettings で決める）
inline const StructSetting<ConsiderationSettings> consideration{
    {SettingField<ConsiderationSettings>::of("Consideration/unlimitedTime", &ConsiderationSettings::unlimitedTime),
     SettingField<ConsiderationSettings>::of("Consideration/byoyomiSec", &ConsiderationSettings::byoyomiSec),
     SettingField<ConsiderationSettings>::of("Consideration/multiPV", &ConsiderationSettings::multiPV)},
    &ConsiderationSettings::normalizedOf};

/// 検討の候補手を盤に矢印で出す（検討タブの「矢印表示」。デフォルト: true）
inline const Setting<bool> considerationArrowsVisible{"Consideration/arrowsVisible", true};

/// 検討タブのフォントサイズ（デフォルト: 10）
inline const Setting<int> considerationFontSize{"ConsiderationTab/fontSize", 10};

// --- 詰み探索（検討とは独立して保存） ---


/// 探索時間（デフォルト: 時間制限あり・20秒）
inline const Setting<bool> tsumeSearchUnlimitedTime{"TsumeSearch/unlimitedTime", false};
inline const Setting<int> tsumeSearchTimeLimitSec{"TsumeSearch/timeLimitSec", 20};

/// 0 はアプリケーションの文字サイズを使用
inline const Setting<int> tsumeSearchFontSize{"TsumeSearch/fontSize", 0};
inline const Setting<QSize> tsumeSearchDialogSize{"TsumeSearch/dialogSize", QSize(560, 360)};

} // namespace AnalysisSettings

#endif // ANALYSISSETTINGS_H
