/// @file considerationtabmanager.cpp
/// @brief 検討タブ管理クラスの実装（状態管理・設定・イベント処理）

#include "considerationtabmanager.h"
#include "engineinfowidget.h"
#include "logviewfontmanager.h"
#include "logcategories.h"
#include "settingscommon.h"
#include "analysissettings.h"
#include "enginesettingsconstants.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>

ConsiderationTabManager::ConsiderationTabManager(QObject* parent)
    : QObject(parent)
{
}

ConsiderationTabManager::~ConsiderationTabManager()
{
}

// ===================== 候補手の数 =====================

int ConsiderationTabManager::considerationMultiPV() const
{
    if (m_multiPVComboBox) {
        return m_multiPVComboBox->currentData().toInt();
    }
    return 1;
}

void ConsiderationTabManager::setConsiderationMultiPV(int value)
{
    if (m_multiPVComboBox) {
        int index = qBound(0, value - 1, m_multiPVComboBox->count() - 1);
        m_multiPVComboBox->setCurrentIndex(index);
    }
}

// ===================== 時間設定 =====================

void ConsiderationTabManager::setConsiderationTimeLimit(bool unlimited, int byoyomiSecVal)
{
    m_considerationTimeLimitSec = unlimited ? 0 : byoyomiSecVal;

    if (m_unlimitedTimeRadioButton && m_considerationTimeRadioButton && m_byoyomiSecSpinBox) {
        m_unlimitedTimeRadioButton->blockSignals(true);
        m_considerationTimeRadioButton->blockSignals(true);
        m_byoyomiSecSpinBox->blockSignals(true);

        if (unlimited) {
            m_unlimitedTimeRadioButton->setChecked(true);
        } else {
            m_considerationTimeRadioButton->setChecked(true);
            m_byoyomiSecSpinBox->setValue(byoyomiSecVal);
        }

        m_unlimitedTimeRadioButton->blockSignals(false);
        m_considerationTimeRadioButton->blockSignals(false);
        m_byoyomiSecSpinBox->blockSignals(false);
    }
}

bool ConsiderationTabManager::isUnlimitedTime() const
{
    if (m_unlimitedTimeRadioButton) {
        return m_unlimitedTimeRadioButton->isChecked();
    }
    return true;
}

int ConsiderationTabManager::byoyomiSec() const
{
    if (m_byoyomiSecSpinBox) {
        return m_byoyomiSecSpinBox->value();
    }
    return 20;
}

// ===================== エンジン名・選択 =====================

void ConsiderationTabManager::setConsiderationEngineName(const QString& name)
{
    if (m_considerationInfo) {
        m_considerationInfo->setDisplayNameFallback(name);
    }
}

int ConsiderationTabManager::selectedEngineIndex() const
{
    if (m_engineComboBox) {
        return m_engineComboBox->currentIndex();
    }
    return 0;
}

QString ConsiderationTabManager::selectedEngineName() const
{
    if (m_engineComboBox) {
        return m_engineComboBox->currentText();
    }
    return QString();
}

// ===================== 矢印表示 =====================

bool ConsiderationTabManager::isShowArrowsChecked() const
{
    if (m_showArrowsCheckBox) {
        return m_showArrowsCheckBox->isChecked();
    }
    return true;
}

// ===================== エンジンリスト =====================

void ConsiderationTabManager::loadEngineList()
{
    if (!m_engineComboBox) return;

    m_engineComboBox->clear();

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const int size = settings.beginReadArray(EngineSettingsConstants::EnginesGroupName);
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        const QString name = settings.value(EngineSettingsConstants::EngineNameKey).toString();
        m_engineComboBox->addItem(name);
    }
    settings.endArray();
}

// ===================== 設定の保存・復元 =====================

void ConsiderationTabManager::loadConsiderationTabSettings()
{
    const int engineIndex = AnalysisSettings::considerationEngineIndex();
    if (m_engineComboBox && engineIndex >= 0 && engineIndex < m_engineComboBox->count()) {
        m_engineComboBox->setCurrentIndex(engineIndex);
    }

    const bool unlimitedTime = AnalysisSettings::considerationUnlimitedTime();
    const int byoyomiSecVal = AnalysisSettings::considerationByoyomiSec();

    if (m_unlimitedTimeRadioButton && m_considerationTimeRadioButton && m_byoyomiSecSpinBox) {
        if (unlimitedTime) {
            m_unlimitedTimeRadioButton->setChecked(true);
        } else {
            m_considerationTimeRadioButton->setChecked(true);
        }
        m_byoyomiSecSpinBox->setValue(byoyomiSecVal > 0 ? byoyomiSecVal : 20);
    }

    const int multiPV = AnalysisSettings::considerationMultiPV();
    if (m_multiPVComboBox && multiPV >= 1 && multiPV <= 10) {
        m_multiPVComboBox->setCurrentIndex(multiPV - 1);
    }
}

void ConsiderationTabManager::saveConsiderationTabSettings()
{
    if (m_engineComboBox) {
        AnalysisSettings::setConsiderationEngineIndex(m_engineComboBox->currentIndex());
    }

    if (m_unlimitedTimeRadioButton && m_byoyomiSecSpinBox) {
        AnalysisSettings::setConsiderationUnlimitedTime(m_unlimitedTimeRadioButton->isChecked());
        AnalysisSettings::setConsiderationByoyomiSec(m_byoyomiSecSpinBox->value());
    }

    if (m_multiPVComboBox) {
        AnalysisSettings::setConsiderationMultiPV(m_multiPVComboBox->currentData().toInt());
    }
}

// ===================== 経過時間タイマー =====================

void ConsiderationTabManager::startElapsedTimer()
{
    if (m_elapsedTimer) {
        m_elapsedSeconds = 0;
        if (m_elapsedTimeLabel) {
            m_elapsedTimeLabel->setText(tr("経過: 0:00"));
            QPalette palette = m_elapsedTimeLabel->palette();
            palette.setColor(QPalette::WindowText, Qt::red);
            m_elapsedTimeLabel->setPalette(palette);
        }
        m_elapsedTimer->start();
    }
}

void ConsiderationTabManager::stopElapsedTimer()
{
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }
}

void ConsiderationTabManager::resetElapsedTimer()
{
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }
    m_elapsedSeconds = 0;
    if (m_elapsedTimeLabel) {
        m_elapsedTimeLabel->setText(tr("経過: 0:00"));
    }
}

// ===================== 検討実行状態 =====================

void ConsiderationTabManager::setConsiderationRunning(bool running)
{
    qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] ENTER running=" << running;

    m_considerationRunning = running;

    if (!m_btnStopConsideration) {
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] button is null, returning";
        return;
    }

    m_btnStopConsideration->setEnabled(false);
    qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] disconnecting button signals";
    m_btnStopConsideration->disconnect();

    if (running) {
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] setting button to '検討中止'";
        m_btnStopConsideration->setText(tr("検討中止"));
        m_btnStopConsideration->setToolTip(tr("検討を中止してエンジンを停止します"));
        connect(m_btnStopConsideration, &QToolButton::clicked,
                this, &ConsiderationTabManager::stopConsiderationRequested);
        m_btnStopConsideration->setEnabled(true);
    } else {
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] setting button to '検討開始'";
        m_btnStopConsideration->setText(tr("検討開始"));
        m_btnStopConsideration->setToolTip(tr("検討ダイアログを開いて検討を開始します"));
        connect(m_btnStopConsideration, &QToolButton::clicked,
                this, &ConsiderationTabManager::startConsiderationRequested);
        QTimer::singleShot(0, this, [this]() {
            if (m_btnStopConsideration) {
                m_btnStopConsideration->setEnabled(true);
                qCDebug(lcUi).noquote() << "[ConsiderationTabManager] button re-enabled after deferred timer";
            }
        });
    }

    qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] EXIT";
}

// ===================== スロット =====================

void ConsiderationTabManager::onMultiPVComboBoxChanged(int index)
{
    Q_UNUSED(index);
    if (m_multiPVComboBox) {
        int value = m_multiPVComboBox->currentData().toInt();
        emit considerationMultiPVChanged(value);
    }
}

void ConsiderationTabManager::onEngineComboBoxChanged(int index)
{
    qCDebug(lcUi).noquote() << "[ConsiderationTabManager::onEngineComboBoxChanged] index=" << index
                       << "m_considerationRunning=" << m_considerationRunning;

    saveConsiderationTabSettings();

    if (m_considerationRunning && m_engineComboBox) {
        const QString engineName = m_engineComboBox->currentText();
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::onEngineComboBoxChanged] emitting considerationEngineChanged"
                           << "index=" << index << "name=" << engineName;
        emit considerationEngineChanged(index, engineName);
    }
}

void ConsiderationTabManager::onEngineSettingsClicked()
{
    if (!m_engineComboBox) return;

    const int engineNumber = m_engineComboBox->currentIndex();
    const QString engineName = m_engineComboBox->currentText();

    if (engineName.isEmpty()) {
        // parentWidget を探してメッセージボックスの親にする
        QWidget* parentWidget = nullptr;
        if (m_considerationToolbar) {
            parentWidget = m_considerationToolbar->window();
        }
        QMessageBox::critical(parentWidget, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    emit engineSettingsRequested(engineNumber, engineName);
}

void ConsiderationTabManager::onTimeSettingChanged()
{
    saveConsiderationTabSettings();

    const bool unlimited = isUnlimitedTime();
    const int sec = byoyomiSec();
    m_considerationTimeLimitSec = unlimited ? 0 : sec;

    emit considerationTimeSettingsChanged(unlimited, sec);
}

void ConsiderationTabManager::onElapsedTimerTick()
{
    m_elapsedSeconds++;

    if (m_considerationTimeLimitSec > 0 && m_elapsedSeconds >= m_considerationTimeLimitSec) {
        if (m_elapsedTimer) {
            m_elapsedTimer->stop();
        }
        m_elapsedSeconds = m_considerationTimeLimitSec;
    }

    const int minutes = m_elapsedSeconds / 60;
    const int seconds = m_elapsedSeconds % 60;

    if (m_elapsedTimeLabel) {
        m_elapsedTimeLabel->setText(tr("経過: %1:%2")
            .arg(minutes)
            .arg(seconds, 2, 10, QLatin1Char('0')));
    }
}
