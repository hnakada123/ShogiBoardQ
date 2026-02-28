/// @file evaluationchartconfigurator.cpp
/// @brief 評価値グラフの設定パネル管理クラスの実装

#include "evaluationchartconfigurator.h"
#include "analysissettings.h"
#include "buttonstyles.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

// 利用可能な上限値のリスト（Y軸：評価値上限）
// 100から1000まで100刻み、1000から30000まで1000刻み
// 31111は詰み評価値（やねうら王系エンジンの優等局面スコア）
const QList<int> EvaluationChartConfigurator::s_availableYLimits = {
    100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
    2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
    11000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000,
    21000, 22000, 23000, 24000, 25000, 26000, 27000, 28000, 29000, 30000,
    31111, 32000
};

// 利用可能な間隔値のリスト（Y軸：評価値間隔）
const QList<int> EvaluationChartConfigurator::s_availableYIntervals = {
    100, 200, 500, 1000, 2000, 5000, 10000
};

// 利用可能な上限値のリスト（X軸：手数上限）
const QList<int> EvaluationChartConfigurator::s_availableXLimits = {
    100, 200, 300, 400, 500, 600, 700, 800, 900, 1000
};

// 利用可能な間隔値のリスト（X軸：手数間隔）
const QList<int> EvaluationChartConfigurator::s_availableXIntervals = {
    2, 5, 10, 20, 25, 50, 100
};

// 利用可能なフォントサイズのリスト
const QList<int> EvaluationChartConfigurator::s_availableFontSizes = {
    5, 6, 7, 8, 9, 10, 11, 12
};

EvaluationChartConfigurator::EvaluationChartConfigurator(QObject* parent)
    : QObject(parent)
{
}

const QList<int>& EvaluationChartConfigurator::availableYLimits()
{
    return s_availableYLimits;
}

const QList<int>& EvaluationChartConfigurator::availableXLimits()
{
    return s_availableXLimits;
}

QWidget* EvaluationChartConfigurator::createControlPanel(QWidget* parentWidget)
{
    auto* controlPanel = new QWidget(parentWidget);
    controlPanel->setFixedHeight(26);  // コンパクトな高さに変更

    auto* layout = new QHBoxLayout(controlPanel);
    layout->setContentsMargins(4, 0, 4, 0);  // 上下のマージンを削除
    layout->setSpacing(4);

    // ラベルスタイル（少し大きめのフォント）
    const QString labelStyle = QStringLiteral("QLabel { color: #333; font-size: 10pt; }");

    // ComboBoxスタイル
    const QString comboStyle = QStringLiteral(
        "QComboBox { "
        "  background-color: #f0f0f0; "
        "  border: 1px solid #999; "
        "  border-radius: 2px; "
        "  padding: 2px 4px; "
        "  font-size: 10pt; "
        "  min-width: 65px; "
        "} "
        "QComboBox:hover { background-color: #e8e8e8; } "
        "QComboBox::drop-down { "
        "  border: none; "
        "  width: 16px; "
        "} "
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 4px solid transparent; "
        "  border-right: 4px solid transparent; "
        "  border-top: 5px solid #666; "
        "}"
    );

    // ボタンスタイル（フォントサイズ用）
    const QString btnStyle = ButtonStyles::fontButton();

    // 評価値上限ComboBox
    auto* lblYLimit = new QLabel(tr("評価値上限:"), controlPanel);
    lblYLimit->setStyleSheet(labelStyle);
    m_comboYLimit = new QComboBox(controlPanel);
    m_comboYLimit->setStyleSheet(comboStyle);
    m_comboYLimit->setToolTip(tr("評価値の表示上限を選択"));
    for (int val : s_availableYLimits) {
        m_comboYLimit->addItem(QString::number(val), val);
    }

    // 評価値間隔ComboBox
    m_lblYInterval = new QLabel(tr("評価値間隔:"), controlPanel);
    m_lblYInterval->setStyleSheet(labelStyle);
    m_comboYInterval = new QComboBox(controlPanel);
    m_comboYInterval->setStyleSheet(comboStyle);
    m_comboYInterval->setToolTip(tr("評価値の目盛り間隔を選択"));
    for (int val : s_availableYIntervals) {
        m_comboYInterval->addItem(QString::number(val), val);
    }

    // 手数上限ComboBox
    auto* lblXLimit = new QLabel(tr("手数上限:"), controlPanel);
    lblXLimit->setStyleSheet(labelStyle);
    m_comboXLimit = new QComboBox(controlPanel);
    m_comboXLimit->setStyleSheet(comboStyle);
    m_comboXLimit->setToolTip(tr("手数の表示上限を選択"));
    for (int val : s_availableXLimits) {
        m_comboXLimit->addItem(QString::number(val), val);
    }

    // 手数間隔ComboBox
    auto* lblXInterval = new QLabel(tr("手数間隔:"), controlPanel);
    lblXInterval->setStyleSheet(labelStyle);
    m_comboXInterval = new QComboBox(controlPanel);
    m_comboXInterval->setStyleSheet(comboStyle);
    m_comboXInterval->setToolTip(tr("手数の目盛り間隔を選択"));
    for (int val : s_availableXIntervals) {
        m_comboXInterval->addItem(QString::number(val), val);
    }

    // フォントサイズボタン（USI通信ログと同じサイズ 28x24）
    m_btnFontDown = new QPushButton(QStringLiteral("A-"), controlPanel);
    m_btnFontUp = new QPushButton(QStringLiteral("A+"), controlPanel);
    m_btnFontDown->setFixedSize(28, 24);
    m_btnFontUp->setFixedSize(28, 24);
    m_btnFontDown->setStyleSheet(btnStyle);
    m_btnFontUp->setStyleSheet(btnStyle);
    m_btnFontDown->setToolTip(tr("目盛りの文字を小さく"));
    m_btnFontUp->setToolTip(tr("目盛りの文字を大きく"));

    // レイアウトに追加（左端から配置）
    layout->addWidget(lblYLimit);
    layout->addWidget(m_comboYLimit);
    layout->addSpacing(8);

    layout->addWidget(m_lblYInterval);
    layout->addWidget(m_comboYInterval);
    layout->addSpacing(8);

    layout->addWidget(lblXLimit);
    layout->addWidget(m_comboXLimit);
    layout->addSpacing(8);

    layout->addWidget(lblXInterval);
    layout->addWidget(m_comboXInterval);
    layout->addSpacing(8);

    layout->addWidget(m_btnFontDown);
    layout->addWidget(m_btnFontUp);

    layout->addStretch();  // 残りのスペースを右側に

    controlPanel->setLayout(layout);

    // 現在の値でComboBoxを初期化
    updateComboBoxSelections();

    // 評価値上限の選択肢を初期化
    updateYLimitComboItems();

    // シグナル接続（ラムダ不使用）
    connect(m_comboYLimit, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartConfigurator::onYLimitChanged);
    connect(m_comboYInterval, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartConfigurator::onYIntervalChanged);
    connect(m_comboXLimit, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartConfigurator::onXLimitChanged);
    connect(m_comboXInterval, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartConfigurator::onXIntervalChanged);

    connect(m_btnFontUp, &QPushButton::clicked,
            this, &EvaluationChartConfigurator::onIncreaseFontSizeClicked);
    connect(m_btnFontDown, &QPushButton::clicked,
            this, &EvaluationChartConfigurator::onDecreaseFontSizeClicked);

    return controlPanel;
}

void EvaluationChartConfigurator::saveSettings()
{
    AnalysisSettings::setEvalChartYLimit(m_yLimit);
    // yIntervalは保存しない（上限の半分に固定）
    AnalysisSettings::setEvalChartXLimit(m_xLimit);
    AnalysisSettings::setEvalChartXInterval(m_xInterval);
    AnalysisSettings::setEvalChartLabelFontSize(m_labelFontSize);
}

void EvaluationChartConfigurator::loadSettings()
{
    m_yLimit = AnalysisSettings::evalChartYLimit();
    m_xLimit = AnalysisSettings::evalChartXLimit();
    m_xInterval = AnalysisSettings::evalChartXInterval();
    m_labelFontSize = AnalysisSettings::evalChartLabelFontSize();

    // 範囲チェック
    if (!s_availableYLimits.contains(m_yLimit)) m_yLimit = 2000;
    if (!s_availableXLimits.contains(m_xLimit)) m_xLimit = 500;
    if (!s_availableXIntervals.contains(m_xInterval)) m_xInterval = 10;
    if (!s_availableFontSizes.contains(m_labelFontSize)) m_labelFontSize = 7;

    // 評価値間隔は上限の半分に固定（目盛り数5個）
    m_yInterval = calculateAppropriateYInterval(m_yLimit);

    // 手数間隔が上限を超えないように調整
    if (m_xInterval > m_xLimit) m_xInterval = m_xLimit;
}

int EvaluationChartConfigurator::calculateAppropriateYInterval(int yLimit) const
{
    // 評価値上限の半分を間隔とする（目盛り数5個固定）
    return yLimit / 2;
}

void EvaluationChartConfigurator::setYAxisLimit(int limit)
{
    if (limit > 0 && limit != m_yLimit) {
        m_yLimit = limit;
        m_yInterval = calculateAppropriateYInterval(m_yLimit);
        syncComboBoxToYLimit();
        syncComboBoxToYInterval();
        emit yAxisSettingsChanged(m_yLimit, m_yInterval);
    }
}

void EvaluationChartConfigurator::setYAxisInterval(int interval)
{
    if (interval > 0 && interval != m_yInterval) {
        m_yInterval = interval;
        syncComboBoxToYInterval();
        emit yAxisSettingsChanged(m_yLimit, m_yInterval);
    }
}

void EvaluationChartConfigurator::setXAxisLimit(int limit)
{
    if (limit > 0 && limit != m_xLimit) {
        m_xLimit = limit;
        // 間隔が上限を超えないように調整
        if (m_xInterval > m_xLimit) {
            m_xInterval = m_xLimit;
        }
        syncComboBoxToXLimit();
        syncComboBoxToXInterval();
        emit xAxisSettingsChanged(m_xLimit, m_xInterval);
    }
}

void EvaluationChartConfigurator::setXAxisInterval(int interval)
{
    if (interval > 0 && interval <= m_xLimit && interval != m_xInterval) {
        m_xInterval = interval;
        syncComboBoxToXInterval();
        emit xAxisSettingsChanged(m_xLimit, m_xInterval);
    }
}

void EvaluationChartConfigurator::setLabelFontSize(int size)
{
    if (size > 0 && size != m_labelFontSize) {
        m_labelFontSize = size;
        emit fontSizeChanged(m_labelFontSize);
    }
}

void EvaluationChartConfigurator::autoExpandYAxisIfNeeded(int cp)
{
    const int absCp = qAbs(cp);
    if (absCp > m_yLimit) {
        // 評価値が上限を超えた場合、自動で拡大
        for (int limit : s_availableYLimits) {
            if (limit >= absCp) {
                setYAxisLimit(limit);
                return;
            }
        }
        // リストにない場合は最大値を設定
        setYAxisLimit(s_availableYLimits.last());
    }
}

void EvaluationChartConfigurator::autoExpandXAxisIfNeeded(int ply)
{
    if (ply > m_xLimit) {
        // 手数が上限を超えた場合、自動で拡大
        for (int limit : s_availableXLimits) {
            if (limit >= ply) {
                setXAxisLimit(limit);
                return;
            }
        }
        // リストにない場合は最大値を設定
        setXAxisLimit(s_availableXLimits.last());
    }
}

void EvaluationChartConfigurator::onYLimitChanged(int index)
{
    if (!m_comboYLimit || index < 0 || index >= m_comboYLimit->count()) return;
    const int newLimit = m_comboYLimit->itemData(index).toInt();
    if (newLimit != m_yLimit) {
        m_yLimit = newLimit;
        m_yInterval = calculateAppropriateYInterval(m_yLimit);
        syncComboBoxToYInterval();
        emit yAxisSettingsChanged(m_yLimit, m_yInterval);
    }
}

void EvaluationChartConfigurator::onYIntervalChanged(int index)
{
    if (index < 0 || index >= s_availableYIntervals.size()) return;
    const int newInterval = s_availableYIntervals.at(index);
    if (newInterval != m_yInterval && newInterval <= m_yLimit) {
        m_yInterval = newInterval;
        emit yAxisSettingsChanged(m_yLimit, m_yInterval);
    }
}

void EvaluationChartConfigurator::onXLimitChanged(int index)
{
    if (index < 0 || index >= s_availableXLimits.size()) return;
    const int newLimit = s_availableXLimits.at(index);
    if (newLimit != m_xLimit) {
        m_xLimit = newLimit;
        if (m_xInterval > m_xLimit) {
            m_xInterval = m_xLimit;
            syncComboBoxToXInterval();
        }
        emit xAxisSettingsChanged(m_xLimit, m_xInterval);
    }
}

void EvaluationChartConfigurator::onXIntervalChanged(int index)
{
    if (index < 0 || index >= s_availableXIntervals.size()) return;
    const int newInterval = s_availableXIntervals.at(index);
    if (newInterval != m_xInterval && newInterval <= m_xLimit) {
        m_xInterval = newInterval;
        emit xAxisSettingsChanged(m_xLimit, m_xInterval);
    }
}

void EvaluationChartConfigurator::onIncreaseFontSizeClicked()
{
    for (int size : s_availableFontSizes) {
        if (size > m_labelFontSize) {
            setLabelFontSize(size);
            return;
        }
    }
}

void EvaluationChartConfigurator::onDecreaseFontSizeClicked()
{
    int newSize = s_availableFontSizes.first();
    for (int size : s_availableFontSizes) {
        if (size >= m_labelFontSize) {
            break;
        }
        newSize = size;
    }
    if (newSize < m_labelFontSize) {
        setLabelFontSize(newSize);
    }
}

void EvaluationChartConfigurator::updateComboBoxSelections()
{
    // シグナルを一時的にブロックして無限ループを防ぐ
    if (m_comboYLimit) {
        m_comboYLimit->blockSignals(true);
        qsizetype idx = s_availableYLimits.indexOf(m_yLimit);
        if (idx >= 0) m_comboYLimit->setCurrentIndex(static_cast<int>(idx));
        m_comboYLimit->blockSignals(false);
    }

    if (m_comboYInterval) {
        m_comboYInterval->blockSignals(true);
        qsizetype idx = s_availableYIntervals.indexOf(m_yInterval);
        if (idx >= 0) m_comboYInterval->setCurrentIndex(static_cast<int>(idx));
        m_comboYInterval->blockSignals(false);
    }

    if (m_comboXLimit) {
        m_comboXLimit->blockSignals(true);
        qsizetype idx = s_availableXLimits.indexOf(m_xLimit);
        if (idx >= 0) m_comboXLimit->setCurrentIndex(static_cast<int>(idx));
        m_comboXLimit->blockSignals(false);
    }

    if (m_comboXInterval) {
        m_comboXInterval->blockSignals(true);
        qsizetype idx = s_availableXIntervals.indexOf(m_xInterval);
        if (idx >= 0) m_comboXInterval->setCurrentIndex(static_cast<int>(idx));
        m_comboXInterval->blockSignals(false);
    }
}

void EvaluationChartConfigurator::updateYLimitComboItems()
{
    if (!m_comboYLimit) return;

    m_comboYLimit->blockSignals(true);
    m_comboYLimit->clear();

    // 全ての上限値を追加（制限なし）
    // 間隔は上限変更時に自動調整されるため、上限は自由に選択可能
    for (int limit : s_availableYLimits) {
        m_comboYLimit->addItem(QString::number(limit), limit);
    }

    // 現在の値を選択
    int currentIdx = -1;
    for (int i = 0; i < m_comboYLimit->count(); ++i) {
        if (m_comboYLimit->itemData(i).toInt() == m_yLimit) {
            currentIdx = i;
            break;
        }
    }
    if (currentIdx >= 0) {
        m_comboYLimit->setCurrentIndex(currentIdx);
    } else if (m_comboYLimit->count() > 0) {
        // 現在の値がリストにない場合は最初の項目を選択
        m_comboYLimit->setCurrentIndex(0);
    }

    m_comboYLimit->blockSignals(false);
}

void EvaluationChartConfigurator::syncComboBoxToYLimit()
{
    if (!m_comboYLimit) return;
    m_comboYLimit->blockSignals(true);
    qsizetype idx = s_availableYLimits.indexOf(m_yLimit);
    if (idx >= 0) m_comboYLimit->setCurrentIndex(static_cast<int>(idx));
    m_comboYLimit->blockSignals(false);
}

void EvaluationChartConfigurator::syncComboBoxToYInterval()
{
    if (!m_comboYInterval) return;
    m_comboYInterval->blockSignals(true);
    qsizetype idx = s_availableYIntervals.indexOf(m_yInterval);
    if (idx >= 0) m_comboYInterval->setCurrentIndex(static_cast<int>(idx));
    m_comboYInterval->blockSignals(false);
}

void EvaluationChartConfigurator::syncComboBoxToXLimit()
{
    if (!m_comboXLimit) return;
    m_comboXLimit->blockSignals(true);
    qsizetype idx = s_availableXLimits.indexOf(m_xLimit);
    if (idx >= 0) m_comboXLimit->setCurrentIndex(static_cast<int>(idx));
    m_comboXLimit->blockSignals(false);
}

void EvaluationChartConfigurator::syncComboBoxToXInterval()
{
    if (!m_comboXInterval) return;
    m_comboXInterval->blockSignals(true);
    qsizetype idx = s_availableXIntervals.indexOf(m_xInterval);
    if (idx >= 0) m_comboXInterval->setCurrentIndex(static_cast<int>(idx));
    m_comboXInterval->blockSignals(false);
}
