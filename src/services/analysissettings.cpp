/// @file analysissettings.cpp
/// @brief 解析・検討設定の永続化実装

#include "analysissettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace AnalysisSettings {

// --- 評価値グラフ ---

int evalChartYLimit()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartYLimit, 2000).toInt();
}

void setEvalChartYLimit(int limit)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartYLimit, limit);
}

int evalChartXLimit()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartXLimit, 500).toInt();
}

void setEvalChartXLimit(int limit)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartXLimit, limit);
}

int evalChartXInterval()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartXInterval, 0).toInt();
}

void setEvalChartXInterval(int interval)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartXInterval, interval);
}

int evalChartLabelFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartLabelFontSize, 10).toInt();
}

void setEvalChartLabelFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartLabelFontSize, size);
}

bool evalChartAutomaticRange()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kEvalChartAutomaticRange, true).toBool();
}

void setEvalChartAutomaticRange(bool automatic)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kEvalChartAutomaticRange, automatic);
}

int evalChartYInterval()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kEvalChartYInterval, 0).toInt();
}

void setEvalChartYInterval(int interval)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kEvalChartYInterval, interval);
}

QSize evalChartSettingsSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kEvalChartSettingsSize, QSize(420, 320)).toSize();
}

void setEvalChartSettingsSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kEvalChartSettingsSize, size);
}

// --- 解析タブ ---

QList<int> engineInfoColumnWidths(int widgetIndex)
{
    QSettings& s = SettingsCommon::openSettings();
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

void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kEngineInfoColumnWidthsFmt).arg(widgetIndex);

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    s.sync();
}

QList<int> thinkingViewColumnWidths(int viewIndex)
{
    QSettings& s = SettingsCommon::openSettings();
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

void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kThinkingViewColumnWidthsFmt).arg(viewIndex);

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    s.sync();
}

int usiLogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeUsiLog, 10).toInt();
}

void setUsiLogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeUsiLog, size);
}

bool usiLogWrapLines()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kUsiLogWrapLines, false).toBool();
}

void setUsiLogWrapLines(bool wrap)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kUsiLogWrapLines, wrap);
}

int usiLogCommandTarget()
{
    return qBound(0, SettingsCommon::openSettings().value(SettingsKeys::kUsiLogCommandTarget, 0).toInt(), 2);
}

void setUsiLogCommandTarget(int target)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kUsiLogCommandTarget, qBound(0, target, 2));
}

int thinkingFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeThinking, 10).toInt();
}

void setThinkingFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeThinking, size);
}

// --- 棋譜解析 ---

int kifuAnalysisFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisFontSize, 0).toInt();
}

void setKifuAnalysisFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisFontSize, size);
}

QList<int> kifuAnalysisColumnWidths()
{
    const QVariantList values = SettingsCommon::openSettings()
        .value(SettingsKeys::kKifuAnalysisColumnWidths).toList();
    QList<int> widths;
    for (const QVariant& value : values) widths.append(qMax(0, value.toInt()));
    return widths;
}

void setKifuAnalysisColumnWidths(const QList<int>& widths)
{
    QVariantList values;
    for (int width : widths) values.append(width);
    SettingsCommon::openSettings().setValue(SettingsKeys::kKifuAnalysisColumnWidths, values);
}

QSize kifuAnalysisResultsWindowSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsWindowSize, QSize(1100, 600)).toSize();
}

void setKifuAnalysisResultsWindowSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsWindowSize, size);
}

int kifuAnalysisByoyomiSec()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisByoyomiSec, 3).toInt();
}

void setKifuAnalysisByoyomiSec(int sec)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisByoyomiSec, sec);
}

int kifuAnalysisEngineIndex()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisEngineIndex, 0).toInt();
}

void setKifuAnalysisEngineIndex(int index)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisEngineIndex, index);
}

bool kifuAnalysisFullRange()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisFullRange, true).toBool();
}

void setKifuAnalysisFullRange(bool fullRange)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisFullRange, fullRange);
}

int kifuAnalysisStartPly()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisStartPly, 0).toInt();
}

void setKifuAnalysisStartPly(int ply)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisStartPly, ply);
}

int kifuAnalysisEndPly()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisEndPly, -1).toInt();
}

void setKifuAnalysisEndPly(int ply)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisEndPly, ply);
}

QSize kifuAnalysisDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisDialogSize, QSize(560, 380)).toSize();
}

void setKifuAnalysisDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisDialogSize, size);
}

// --- 検討モード ---

int considerationDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeConsiderationDialog, 10).toInt();
}

void setConsiderationDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeConsiderationDialog, size);
}

int considerationEngineIndex()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kConsiderationEngineIndex, 0).toInt();
}

void setConsiderationEngineIndex(int index)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kConsiderationEngineIndex, index);
}

bool considerationUnlimitedTime()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kConsiderationUnlimitedTime, true).toBool();
}

void setConsiderationUnlimitedTime(bool unlimited)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kConsiderationUnlimitedTime, unlimited);
}

int considerationByoyomiSec()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kConsiderationByoyomiSec, 0).toInt();
}

void setConsiderationByoyomiSec(int sec)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kConsiderationByoyomiSec, sec);
}

int considerationMultiPV()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kConsiderationMultiPV, 1).toInt();
}

void setConsiderationMultiPV(int multiPV)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kConsiderationMultiPV, multiPV);
}

int considerationFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kConsiderationTabFontSize, 10).toInt();
}

void setConsiderationFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kConsiderationTabFontSize, size);
}

// --- 詰み探索 ---

QString tsumeSearchEnginePath()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kTsumeSearchEnginePath).toString();
}

void setTsumeSearchEnginePath(const QString& path)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kTsumeSearchEnginePath, path);
}

bool tsumeSearchUnlimitedTime()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kTsumeSearchUnlimitedTime, false).toBool();
}

void setTsumeSearchUnlimitedTime(bool unlimited)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kTsumeSearchUnlimitedTime, unlimited);
}

int tsumeSearchTimeLimitSec()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kTsumeSearchTimeLimitSec, 20).toInt();
}

void setTsumeSearchTimeLimitSec(int sec)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kTsumeSearchTimeLimitSec, sec);
}

int tsumeSearchFontSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kTsumeSearchFontSize, 0).toInt();
}

void setTsumeSearchFontSize(int size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kTsumeSearchFontSize, size);
}

QSize tsumeSearchDialogSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kTsumeSearchDialogSize, QSize(560, 360)).toSize();
}

void setTsumeSearchDialogSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kTsumeSearchDialogSize, size);
}

// --- 読み筋盤面 ---

QSize pvBoardDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kPvBoardDialogSize, QSize(620, 720)).toSize();
}

void setPvBoardDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kPvBoardDialogSize, size);
}

} // namespace AnalysisSettings
