/// @file considerationtabmanager.cpp
/// @brief 検討タブ管理クラスの実装

#include "considerationtabmanager.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"
#include "engineinfowidget.h"
#include "flowlayout.h"
#include "logcategories.h"
#include "numericrightaligncommadelegate.h"
#include "settingscommon.h"
#include "analysissettings.h"
#include "shogienginethinkingmodel.h"
#include "enginesettingsconstants.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

ConsiderationTabManager::ConsiderationTabManager(QObject* parent)
    : QObject(parent)
{
}

ConsiderationTabManager::~ConsiderationTabManager()
{
}

void ConsiderationTabManager::buildConsiderationUi(QWidget* parentWidget)
{
    // SettingsServiceからフォントサイズを読み込み
    m_considerationFontSize = AnalysisSettings::considerationFontSize();

    // ツールバー（FlowLayout使用）
    m_considerationToolbar = new QWidget(parentWidget);
    auto* toolbarLayout = new FlowLayout(m_considerationToolbar, 2, 8, 4);

    // フォントサイズ減少ボタン（A-）
    m_btnConsiderationFontDecrease = new QToolButton(m_considerationToolbar);
    m_btnConsiderationFontDecrease->setText(QStringLiteral("A-"));
    m_btnConsiderationFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnConsiderationFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnConsiderationFontDecrease, &QToolButton::clicked,
            this, &ConsiderationTabManager::onConsiderationFontDecrease);

    // フォントサイズ増加ボタン（A+）
    m_btnConsiderationFontIncrease = new QToolButton(m_considerationToolbar);
    m_btnConsiderationFontIncrease->setText(QStringLiteral("A+"));
    m_btnConsiderationFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnConsiderationFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnConsiderationFontIncrease, &QToolButton::clicked,
            this, &ConsiderationTabManager::onConsiderationFontIncrease);

    // エンジン選択コンボボックス
    m_engineComboBox = new QComboBox(m_considerationToolbar);
    m_engineComboBox->setToolTip(tr("検討に使用するエンジンを選択します"));
    m_engineComboBox->setMinimumWidth(150);
    connect(m_engineComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConsiderationTabManager::onEngineComboBoxChanged);

    // エンジン設定ボタン
    m_btnEngineSettings = new QPushButton(tr("エンジン設定"), m_considerationToolbar);
    m_btnEngineSettings->setToolTip(tr("選択したエンジンの設定を変更します"));
    m_btnEngineSettings->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_btnEngineSettings, &QPushButton::clicked,
            this, &ConsiderationTabManager::onEngineSettingsClicked);

    // 時間無制限ラジオボタン
    m_unlimitedTimeRadioButton = new QRadioButton(tr("時間無制限"), m_considerationToolbar);
    m_unlimitedTimeRadioButton->setToolTip(tr("時間制限なしで検討します"));
    connect(m_unlimitedTimeRadioButton, &QRadioButton::toggled,
            this, &ConsiderationTabManager::onTimeSettingChanged);

    // 検討時間ラジオボタン
    m_considerationTimeRadioButton = new QRadioButton(tr("検討時間"), m_considerationToolbar);
    m_considerationTimeRadioButton->setToolTip(tr("指定した秒数まで検討します"));
    connect(m_considerationTimeRadioButton, &QRadioButton::toggled,
            this, &ConsiderationTabManager::onTimeSettingChanged);

    // 検討時間スピンボックス
    m_byoyomiSecSpinBox = new QSpinBox(m_considerationToolbar);
    m_byoyomiSecSpinBox->setRange(1, 3600);
    m_byoyomiSecSpinBox->setValue(20);
    m_byoyomiSecSpinBox->setToolTip(tr("検討時間（秒）を指定します"));
    connect(m_byoyomiSecSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConsiderationTabManager::onTimeSettingChanged);

    // 「秒まで」ラベル
    m_byoyomiSecUnitLabel = new QLabel(tr("秒まで"), m_considerationToolbar);

    // 経過時間ラベル
    m_elapsedTimeLabel = new QLabel(tr("経過: 0:00"), m_considerationToolbar);
    m_elapsedTimeLabel->setToolTip(tr("検討開始からの経過時間"));
    {
        QPalette palette = m_elapsedTimeLabel->palette();
        palette.setColor(QPalette::WindowText, Qt::red);
        m_elapsedTimeLabel->setPalette(palette);
    }

    // 経過時間更新タイマー
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &ConsiderationTabManager::onElapsedTimerTick);

    // 候補手の数
    m_multiPVLabel = new QLabel(tr("候補手の数"), m_considerationToolbar);
    m_multiPVComboBox = new QComboBox(m_considerationToolbar);
    for (int i = 1; i <= 10; ++i) {
        m_multiPVComboBox->addItem(tr("%1手").arg(i), i);
    }
    m_multiPVComboBox->setCurrentIndex(0);
    m_multiPVComboBox->setToolTip(tr("評価値が大きい順に表示する候補手の数を指定します"));

    // 矢印表示チェックボックス
    m_showArrowsCheckBox = new QCheckBox(tr("矢印表示"), m_considerationToolbar);
    m_showArrowsCheckBox->setToolTip(tr("最善手の矢印を盤面に表示します"));
    m_showArrowsCheckBox->setChecked(true);
    connect(m_showArrowsCheckBox, &QCheckBox::toggled,
            this, &ConsiderationTabManager::showArrowsChanged);

    // 検討開始/中止ボタン（初期状態は「検討開始」）
    m_btnStopConsideration = new QToolButton(m_considerationToolbar);
    m_btnStopConsideration->setText(tr("検討開始"));
    m_btnStopConsideration->setToolTip(tr("検討を開始します"));
    m_btnStopConsideration->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_btnStopConsideration, &QToolButton::clicked,
            this, &ConsiderationTabManager::startConsiderationRequested);

    // ツールバーにウィジェットを追加（FlowLayoutで自動折り返し）
    toolbarLayout->addWidget(m_btnConsiderationFontDecrease);
    toolbarLayout->addWidget(m_btnConsiderationFontIncrease);
    toolbarLayout->addWidget(m_engineComboBox);
    toolbarLayout->addWidget(m_btnEngineSettings);
    toolbarLayout->addWidget(m_unlimitedTimeRadioButton);
    toolbarLayout->addWidget(m_considerationTimeRadioButton);
    toolbarLayout->addWidget(m_byoyomiSecSpinBox);
    toolbarLayout->addWidget(m_byoyomiSecUnitLabel);
    toolbarLayout->addWidget(m_elapsedTimeLabel);
    toolbarLayout->addWidget(m_multiPVLabel);
    toolbarLayout->addWidget(m_multiPVComboBox);
    toolbarLayout->addWidget(m_showArrowsCheckBox);
    toolbarLayout->addWidget(m_btnStopConsideration);

    // コンボボックスの値変更シグナルを接続
    connect(m_multiPVComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConsiderationTabManager::onMultiPVComboBoxChanged);

    // エンジンリストを読み込み、設定を復元
    loadEngineList();
    loadConsiderationTabSettings();

    // EngineInfoWidget（検討タブ用）
    m_considerationInfo = new EngineInfoWidget(parentWidget, false, false);
    m_considerationInfo->setWidgetIndex(2);
    m_considerationInfo->setFontSize(m_considerationFontSize);

    // 読み筋テーブルビュー
    m_considerationView = new QTableView(parentWidget);

    // ヘッダ設定
    {
        auto* h = m_considerationView->horizontalHeader();
        if (h) {
            m_considerationView->setStyleSheet(QStringLiteral(
                "QHeaderView::section {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                "    stop:0 #40acff, stop:1 #209cee);"
                "  color: white;"
                "  font-weight: normal;"
                "  padding: 2px 6px;"
                "  border: none;"
                "  border-bottom: 1px solid #209cee;"
                "}"));
            h->setDefaultSectionSize(100);
            h->setMinimumSectionSize(24);
            h->setStretchLastSection(true);
        }
        auto* vh = m_considerationView->verticalHeader();
        if (vh) {
            vh->setVisible(false);
            const int rowHeight = m_considerationView->fontMetrics().height() + 4;
            vh->setDefaultSectionSize(rowHeight);
            vh->setSectionResizeMode(QHeaderView::Fixed);
        }
        m_considerationView->setWordWrap(false);
    }

    // 読み筋テーブルのクリックシグナルを接続
    connect(m_considerationView, &QTableView::clicked,
            this, &ConsiderationTabManager::onConsiderationViewClicked);

    // buildConsiderationUi() より前に setConsiderationThinkingModel() が呼ばれた場合、
    // 保存済みモデルをビューに適用する
    if (m_considerationModel) {
        setConsiderationThinkingModel(m_considerationModel);
    }

    // フォントマネージャーの初期化・適用
    initFontManager();

    // parentWidget のレイアウトに追加
    auto* layout = qobject_cast<QVBoxLayout*>(parentWidget->layout());
    if (layout) {
        layout->addWidget(m_considerationToolbar);
        layout->addWidget(m_considerationInfo);
        layout->addWidget(m_considerationView, 1);
    }
}

// ===================== モデル設定 =====================

void ConsiderationTabManager::setConsiderationThinkingModel(ShogiEngineThinkingModel* m)
{
    m_considerationModel = m;
    if (m_considerationView && m) {
        m_considerationView->setModel(m);
        // 数値列の右寄せ＆3桁カンマ
        auto* delegate = new NumericRightAlignCommaDelegate(m_considerationView);
        const QStringList targets = {
            "Time", "Depth", "Nodes", "Score",
            "時間", "深さ", "ノード数", "評価値"
        };
        for (const QString& t : targets) {
            for (int c = 0; c < m->columnCount(); ++c) {
                const QString h = m->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().trimmed();
                if (QString::compare(h, t, Qt::CaseInsensitive) == 0) {
                    m_considerationView->setItemDelegateForColumn(c, delegate);
                }
            }
        }
    }
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

void ConsiderationTabManager::onConsiderationViewClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    if (index.column() == 4) {
        qCDebug(lcUi) << "[ConsiderationTabManager] onConsiderationViewClicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(2, index.row());
    }
}

void ConsiderationTabManager::onConsiderationFontIncrease()
{
    m_considerationFontManager->increase();
}

void ConsiderationTabManager::onConsiderationFontDecrease()
{
    m_considerationFontManager->decrease();
}

void ConsiderationTabManager::initFontManager()
{
    m_considerationFontManager = std::make_unique<LogViewFontManager>(m_considerationFontSize, nullptr);
    m_considerationFontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);

        // ツールバー要素のフォントサイズ更新
        // A+/A- ボタンはフォントサイズに合わせてボタンサイズも更新
        {
            const QFontMetrics fm(font);
            const int btnW = fm.horizontalAdvance(QStringLiteral("A+")) + 12;
            const int btnH = fm.height() + 8;
            if (m_btnConsiderationFontDecrease) {
                m_btnConsiderationFontDecrease->setFont(font);
                m_btnConsiderationFontDecrease->setFixedSize(btnW, btnH);
            }
            if (m_btnConsiderationFontIncrease) {
                m_btnConsiderationFontIncrease->setFont(font);
                m_btnConsiderationFontIncrease->setFixedSize(btnW, btnH);
            }
        }
        if (m_engineComboBox) m_engineComboBox->setFont(font);
        if (m_btnEngineSettings) m_btnEngineSettings->setFont(font);
        if (m_unlimitedTimeRadioButton) m_unlimitedTimeRadioButton->setFont(font);
        if (m_considerationTimeRadioButton) m_considerationTimeRadioButton->setFont(font);
        if (m_byoyomiSecSpinBox) m_byoyomiSecSpinBox->setFont(font);
        if (m_byoyomiSecUnitLabel) m_byoyomiSecUnitLabel->setFont(font);
        if (m_elapsedTimeLabel) m_elapsedTimeLabel->setFont(font);
        if (m_multiPVLabel) m_multiPVLabel->setFont(font);
        if (m_multiPVComboBox) m_multiPVComboBox->setFont(font);
        if (m_showArrowsCheckBox) m_showArrowsCheckBox->setFont(font);
        if (m_btnStopConsideration) m_btnStopConsideration->setFont(font);

        if (m_considerationInfo) m_considerationInfo->setFontSize(size);

        QString headerStyle = QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #209cee;"
            "  font-size: %1pt;"
            "}")
            .arg(size);

        if (m_considerationView) {
            m_considerationView->setFont(font);
            m_considerationView->setStyleSheet(headerStyle);
            const int rowHeight = m_considerationView->fontMetrics().height() + 4;
            m_considerationView->verticalHeader()->setDefaultSectionSize(rowHeight);
        }

        AnalysisSettings::setConsiderationFontSize(size);
    });
    m_considerationFontManager->apply();
}

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
