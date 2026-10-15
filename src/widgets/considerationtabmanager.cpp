/// @file considerationtabmanager.cpp
/// @brief 検討タブ管理クラスの実装（状態管理・設定・イベント処理）

#include "considerationtabmanager.h"
#include "enginelistsettings.h"
#include "engineinfowidget.h"
#include "logcategories.h"
#include "analysissettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include "tablemodelutils.h"
#include "kifumovedelegate.h"
#include "buttonstyles.h"
#include "flowlayout.h"
#include "numericrightaligncommadelegate.h"
#include "pvboardbuttondelegate.h"
#include "pvtextdelegate.h"
#include "tablestyles.h"
#include "shogienginethinkingmodel.h"
#include "thinkinginfopresenter.h"
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <initializer_list>

ConsiderationTabManager::ConsiderationTabManager(QObject* parent)
    : QObject(parent)
    , m_considerationFontSize(AnalysisSettings::considerationFontSize, 8, 24)
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
    updateTimeControls();
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

    const QSignalBlocker blocker(m_engineComboBox);
    const QString selectedName = m_engineComboBox->currentText();
    m_engineComboBox->clear();

    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    for (qsizetype i = 0; i < engines.size(); ++i) {
        m_engineComboBox->addItem(engines.at(i).name);
        m_engineComboBox->setItemData(static_cast<int>(i), engines.at(i).name, Qt::ToolTipRole);
    }
    // 選び直した一覧でも、前に選んでいたエンジン（無ければ保存した選択）を選ぶ
    const int previousIndex = m_engineComboBox->findText(selectedName);
    if (m_engineComboBox->count() > 0) {
        const int saved = EngineListSettings::restoreSelection(EngineListSettings::considerationSelection(), engines);
        m_engineComboBox->setCurrentIndex(previousIndex >= 0 ? previousIndex : qMax(0, saved));
    }
}

// ===================== 設定の保存・復元 =====================

void ConsiderationTabManager::loadConsiderationTabSettings()
{
    // 復元中の変更通知で、未復元の設定を初期値へ上書きしない。
    const QSignalBlocker engineBlocker(m_engineComboBox);
    const QSignalBlocker unlimitedBlocker(m_unlimitedTimeRadioButton);
    const QSignalBlocker timedBlocker(m_considerationTimeRadioButton);
    const QSignalBlocker secondsBlocker(m_byoyomiSecSpinBox);
    const QSignalBlocker multiPVBlocker(m_multiPVComboBox);
    const int engineIndex = EngineListSettings::restoreSelection(EngineListSettings::considerationSelection(),
                                                                 EngineListSettings::loadEngines());
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

    m_considerationTimeLimitSec = unlimitedTime ? 0 : byoyomiSec();
    updateTimeControls();

    const int multiPV = AnalysisSettings::considerationMultiPV();
    if (m_multiPVComboBox && multiPV >= 1 && multiPV <= 10) {
        m_multiPVComboBox->setCurrentIndex(multiPV - 1);
    }
}

void ConsiderationTabManager::saveConsiderationTabSettings()
{
    if (m_engineComboBox) {
        EngineListSettings::saveSelection(EngineListSettings::considerationSelection(), m_engineComboBox->currentText());
    }

    if (m_unlimitedTimeRadioButton && m_byoyomiSecSpinBox) {
        AnalysisSettings::considerationUnlimitedTime.set(m_unlimitedTimeRadioButton->isChecked());
        AnalysisSettings::considerationByoyomiSec.set(m_byoyomiSecSpinBox->value());
    }

    if (m_multiPVComboBox) {
        AnalysisSettings::considerationMultiPV.set(m_multiPVComboBox->currentData().toInt());
    }
}

// ===================== 経過時間タイマー =====================

void ConsiderationTabManager::startElapsedTimer()
{
    if (m_elapsedTimer) {
        m_elapsedSeconds = 0;
        if (m_elapsedTimeLabel) {
            m_elapsedTimeLabel->setText(tr("経過: 0:00"));
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
    // 時間設定は開始時にエンジンへ渡す。実行中に表示だけ変更されないよう固定する。
    updateTimeControls();

    if (!m_btnStopConsideration) {
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] button is null, returning";
        return;
    }

    m_btnStopConsideration->setEnabled(false);
    qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] reconnecting button handler";
    if (m_stopButtonConnection) {
        QObject::disconnect(m_stopButtonConnection);
        m_stopButtonConnection = {};
    }

    if (running) {
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] setting button to '検討中止'";
        m_btnStopConsideration->setText(tr("検討中止"));
        m_btnStopConsideration->setToolTip(tr("検討を中止してエンジンを停止します"));
        m_stopButtonConnection = connect(
            m_btnStopConsideration, &QToolButton::clicked,
            this, &ConsiderationTabManager::stopConsiderationRequested);
        m_btnStopConsideration->setEnabled(true);
    } else {
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] setting button to '検討開始'";
        m_btnStopConsideration->setText(tr("検討開始"));
        m_btnStopConsideration->setToolTip(tr("検討を開始します"));
        m_stopButtonConnection = connect(
            m_btnStopConsideration, &QToolButton::clicked,
            this, &ConsiderationTabManager::startConsiderationRequested);
        m_btnStopConsideration->setEnabled(m_startConsiderationEnabled);
        qCDebug(lcUi).noquote() << "[ConsiderationTabManager] button state updated immediately"
                                << "enabled=" << m_startConsiderationEnabled;
    }

    qCDebug(lcUi).noquote() << "[ConsiderationTabManager::setConsiderationRunning] EXIT";
}

void ConsiderationTabManager::setStartConsiderationEnabled(bool enabled)
{
    m_startConsiderationEnabled = enabled;

    // 検討中は「検討中止」操作を優先して無効化しない
    if (!m_considerationRunning && m_btnStopConsideration) {
        m_btnStopConsideration->setEnabled(enabled);
    }
}

// ===================== スロット =====================

void ConsiderationTabManager::onMultiPVComboBoxChanged(int index)
{
    Q_UNUSED(index);
    if (m_multiPVComboBox) {
        saveConsiderationTabSettings();
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
    updateTimeControls();
    saveConsiderationTabSettings();

    const bool unlimited = isUnlimitedTime();
    const int sec = byoyomiSec();
    m_considerationTimeLimitSec = unlimited ? 0 : sec;

    emit considerationTimeSettingsChanged(unlimited, sec);
}

void ConsiderationTabManager::updateTimeControls()
{
    if (m_unlimitedTimeRadioButton) m_unlimitedTimeRadioButton->setEnabled(!m_considerationRunning);
    if (m_considerationTimeRadioButton) m_considerationTimeRadioButton->setEnabled(!m_considerationRunning);
    const bool enabled = !m_considerationRunning && !isUnlimitedTime();
    if (m_byoyomiSecSpinBox) m_byoyomiSecSpinBox->setEnabled(enabled);
    if (m_byoyomiSecUnitLabel) m_byoyomiSecUnitLabel->setEnabled(enabled);
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

// ============================================================
// ConsiderationTabManager のUI構築・フォント管理
// ============================================================

void ConsiderationTabManager::buildConsiderationUi(QWidget* parentWidget)
{
    buildToolbarControls(parentWidget);
    layoutToolbar();
    buildConsiderationView(parentWidget);

    // 保存されている文字の大きさを適用
    applyFontSize();

    // parentWidget のレイアウトに追加
    auto* layout = qobject_cast<QVBoxLayout*>(parentWidget->layout());
    if (layout) {
        layout->addWidget(m_considerationToolbar);
        layout->addWidget(m_considerationInfo);
        layout->addWidget(m_considerationView, 1);
    }
}

void ConsiderationTabManager::buildToolbarControls(QWidget* parentWidget)
{
    // ツールバー（FlowLayout使用）
    m_considerationToolbar = new QWidget(parentWidget);
    m_considerationToolbar->setObjectName(QStringLiteral("considerationToolbar"));

    // フォントサイズ減少ボタン（A-）
    m_btnConsiderationFontDecrease = new QToolButton(m_considerationToolbar);
    m_btnConsiderationFontDecrease->setObjectName(QStringLiteral("considerationFontDecrease"));
    m_btnConsiderationFontDecrease->setText(QStringLiteral("A-"));
    m_btnConsiderationFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnConsiderationFontDecrease->setStyleSheet(ButtonStyles::panelToolButton());
    connect(m_btnConsiderationFontDecrease, &QToolButton::clicked,
            this, &ConsiderationTabManager::onConsiderationFontDecrease);

    // フォントサイズ増加ボタン（A+）
    m_btnConsiderationFontIncrease = new QToolButton(m_considerationToolbar);
    m_btnConsiderationFontIncrease->setObjectName(QStringLiteral("considerationFontIncrease"));
    m_btnConsiderationFontIncrease->setText(QStringLiteral("A+"));
    m_btnConsiderationFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnConsiderationFontIncrease->setStyleSheet(ButtonStyles::panelToolButton());
    connect(m_btnConsiderationFontIncrease, &QToolButton::clicked,
            this, &ConsiderationTabManager::onConsiderationFontIncrease);

    // エンジン選択コンボボックス
    m_engineComboBox = new QComboBox(m_considerationToolbar);
    m_engineComboBox->setObjectName(QStringLiteral("considerationEngine"));
    m_engineComboBox->setToolTip(tr("検討に使用するエンジンを選択します"));
    m_engineComboBox->setMinimumWidth(150);
    m_engineComboBox->setMinimumContentsLength(24);
    m_engineComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    connect(m_engineComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConsiderationTabManager::onEngineComboBoxChanged);

    // エンジン設定ボタン
    m_btnEngineSettings = new QPushButton(tr("エンジン設定"), m_considerationToolbar);
    m_btnEngineSettings->setObjectName(QStringLiteral("considerationEngineSettings"));
    m_btnEngineSettings->setToolTip(tr("選択したエンジンの設定を変更します"));
    m_btnEngineSettings->setStyleSheet(ButtonStyles::panelToolButton()
        + QStringLiteral("QPushButton { padding: 2px 8px; }"));
    connect(m_btnEngineSettings, &QPushButton::clicked,
            this, &ConsiderationTabManager::onEngineSettingsClicked);

    // 時間無制限ラジオボタン
    m_unlimitedTimeRadioButton = new QRadioButton(tr("時間無制限"), m_considerationToolbar);
    m_unlimitedTimeRadioButton->setObjectName(QStringLiteral("considerationUnlimited"));
    m_unlimitedTimeRadioButton->setToolTip(tr("時間制限なしで検討します"));
    connect(m_unlimitedTimeRadioButton, &QRadioButton::toggled,
            this, &ConsiderationTabManager::onTimeSettingChanged);

    // 検討時間ラジオボタン
    m_considerationTimeRadioButton = new QRadioButton(tr("検討時間"), m_considerationToolbar);
    m_considerationTimeRadioButton->setObjectName(QStringLiteral("considerationTimed"));
    m_considerationTimeRadioButton->setToolTip(tr("指定した秒数まで検討します"));
    connect(m_considerationTimeRadioButton, &QRadioButton::toggled,
            this, &ConsiderationTabManager::onTimeSettingChanged);

    // 検討時間スピンボックス
    m_byoyomiSecSpinBox = new QSpinBox(m_considerationToolbar);
    m_byoyomiSecSpinBox->setObjectName(QStringLiteral("considerationSeconds"));
    m_byoyomiSecSpinBox->setRange(1, 3600);
    m_byoyomiSecSpinBox->setValue(20);
    m_byoyomiSecSpinBox->setToolTip(tr("検討時間（秒）を指定します"));
    connect(m_byoyomiSecSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConsiderationTabManager::onTimeSettingChanged);

    // 「秒まで」ラベル
    m_byoyomiSecUnitLabel = new QLabel(tr("秒まで"), m_considerationToolbar);

    // 経過時間ラベル
    m_elapsedTimeLabel = new QLabel(tr("経過: 0:00"), m_considerationToolbar);
    m_elapsedTimeLabel->setObjectName(QStringLiteral("considerationElapsed"));
    m_elapsedTimeLabel->setToolTip(tr("検討開始からの経過時間"));
    m_elapsedTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 経過時間更新タイマー
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &ConsiderationTabManager::onElapsedTimerTick);

    // 候補手の数
    m_multiPVLabel = new QLabel(tr("候補手の数"), m_considerationToolbar);
    m_multiPVComboBox = new QComboBox(m_considerationToolbar);
    m_multiPVComboBox->setObjectName(QStringLiteral("considerationMultiPV"));
    for (int i = 1; i <= 10; ++i) {
        m_multiPVComboBox->addItem(tr("%1手").arg(i), i);
    }
    m_multiPVComboBox->setCurrentIndex(0);
    m_multiPVComboBox->setToolTip(tr("評価値が大きい順に表示する候補手の数を指定します"));

    // 矢印表示チェックボックス
    m_showArrowsCheckBox = new QCheckBox(tr("矢印表示"), m_considerationToolbar);
    m_showArrowsCheckBox->setObjectName(QStringLiteral("considerationArrows"));
    m_showArrowsCheckBox->setToolTip(tr("最善手の矢印を盤面に表示します"));
    m_showArrowsCheckBox->setChecked(true);
    connect(m_showArrowsCheckBox, &QCheckBox::toggled,
            this, &ConsiderationTabManager::showArrowsChanged);

    // 検討開始/中止ボタン（初期状態は「検討開始」）
    m_btnStopConsideration = new QToolButton(m_considerationToolbar);
    m_btnStopConsideration->setObjectName(QStringLiteral("considerationStartStop"));
    m_btnStopConsideration->setText(tr("検討開始"));
    m_btnStopConsideration->setToolTip(tr("検討を開始します"));
    m_btnStopConsideration->setStyleSheet(ButtonStyles::primaryAction());
    m_stopButtonConnection = connect(m_btnStopConsideration, &QToolButton::clicked,
                                    this, &ConsiderationTabManager::startConsiderationRequested);
}

void ConsiderationTabManager::layoutToolbar()
{
    auto* toolbarLayout = new FlowLayout(m_considerationToolbar, 2, 16, 6);
    // 関連する操作を同じ行に保ち、グループ単位で折り返す。
    const auto addGroup = [this, toolbarLayout](std::initializer_list<QWidget*> widgets) {
        auto* group = new QWidget(m_considerationToolbar);
        auto* row = new QHBoxLayout(group);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        for (auto* widget : widgets) row->addWidget(widget);
        toolbarLayout->addWidget(group);
    };
    addGroup({m_btnConsiderationFontDecrease, m_btnConsiderationFontIncrease});
    addGroup({m_engineComboBox, m_btnEngineSettings});
    // ラジオボタンは同じ親に置き、相互排他を維持する。
    addGroup({m_unlimitedTimeRadioButton, m_considerationTimeRadioButton,
              m_byoyomiSecSpinBox, m_byoyomiSecUnitLabel});
    addGroup({m_multiPVLabel, m_multiPVComboBox, m_showArrowsCheckBox});
    addGroup({m_elapsedTimeLabel, m_btnStopConsideration});

    // コンボボックスの値変更シグナルを接続
    connect(m_multiPVComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConsiderationTabManager::onMultiPVComboBoxChanged);

    // エンジンリストを読み込み、設定を復元
    loadEngineList();
    loadConsiderationTabSettings();
}

void ConsiderationTabManager::buildConsiderationView(QWidget* parentWidget)
{
    // EngineInfoWidget（検討タブ用）
    m_considerationInfo = new EngineInfoWidget(parentWidget, false);
    m_considerationInfo->setObjectName(QStringLiteral("considerationInfo"));
    m_considerationInfo->setWidgetIndex(2);
    m_considerationInfo->setFontSize(m_considerationFontSize.fontSize());
    m_considerationInfo->setColumnWidths(AnalysisSettings::engineInfoColumnWidths(2));
    connect(m_considerationInfo, &EngineInfoWidget::columnWidthChanged,
            this, &ConsiderationTabManager::saveInfoColumnWidths);

    // 読み筋テーブルビュー
    m_considerationView = new QTableView(parentWidget);
    m_considerationView->setObjectName(QStringLiteral("considerationView"));

    m_considerationView->setShowGrid(false);
    m_considerationView->setMouseTracking(true);
    // 思考表（EngineAnalysisPresenter::setupThinkingViewHeader）と見た目をそろえ、
    // 縦のスクロールバーを常に出しておく
    m_considerationView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_considerationView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_considerationView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_considerationView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_considerationView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_considerationView->setItemDelegateForColumn(4, new PvBoardButtonDelegate(m_considerationView));
    m_considerationView->setItemDelegateForColumn(5, new PvTextDelegate(m_considerationView));

    // ヘッダ設定
    {
        auto* h = m_considerationView->horizontalHeader();
        if (h) {
            m_considerationView->setStyleSheet(TableStyles::thinking());
            h->setDefaultSectionSize(100);
            h->setMinimumSectionSize(24);
            h->setStretchLastSection(true);
            connect(h, &QHeaderView::sectionResized,
                    this, &ConsiderationTabManager::saveViewColumnWidths);
        }
        auto* vh = m_considerationView->verticalHeader();
        if (vh) {
            vh->setVisible(false);
            const int rowHeight = KifuMoveDelegate::textHeight(m_considerationView->font()) + 4;
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
}

// ===================== モデル設定 =====================

void ConsiderationTabManager::setConsiderationThinkingModel(ShogiEngineThinkingModel* m)
{
    m_considerationModel = m;
    if (m_considerationView && m) {
        const QSignalBlocker blocker(m_considerationView->horizontalHeader());
        TableModelUtils::setModel(m_considerationView, m);
        // 数値列の右寄せ＆3桁カンマ
        // 検討の開始・終了でモデルを再設定しても同じ delegate を使う。
        auto* delegate = qobject_cast<NumericRightAlignCommaDelegate*>(m_considerationView->itemDelegateForColumn(0));
        if (!delegate) delegate = new NumericRightAlignCommaDelegate(m_considerationView);
        for (int c = 0; c < 4; ++c) {
            m_considerationView->setItemDelegateForColumn(c, delegate);
        }
        applyViewColumnWidths();
    }
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ConsiderationTabManager::applyViewColumnWidths()
{
    if (!m_considerationView || !m_considerationModel) return;
    auto* header = m_considerationView->horizontalHeader();
    const QSignalBlocker blocker(header);
    const QFontMetrics metrics(m_considerationView->font());
    const auto saved = AnalysisSettings::thinkingViewColumnWidths(2);
    const QString samples[] = {QStringLiteral("99,999"), QStringLiteral("99"),
        QStringLiteral("99,999,999"), QStringLiteral("-9,999"), tr("表示")};
    for (int col = 0; col < 5; ++col) {
        const QString title = m_considerationModel->headerData(col, Qt::Horizontal).toString();
        const int minimum = qMax(metrics.horizontalAdvance(title), metrics.horizontalAdvance(samples[col])) + 20;
        const int width = saved.size() == 6 ? qMax(minimum, saved.at(col)) : minimum;
        m_considerationView->setColumnWidth(col, width);
    }
    // 評価値列は、訳語の長い詰みの表示（英語では "Mate in 99 plies"）も省略せずに表示する。
    const int mateWidth = metrics.horizontalAdvance(ThinkingInfoPresenter::mateScoreText(QStringLiteral("-99"))) + 20;
    m_considerationView->setColumnWidth(3, qMax(m_considerationView->columnWidth(3), mateWidth));
    // 読み筋は残り幅を使う。狭いドックでも横スクロールで閲覧できる。
    m_considerationView->setColumnWidth(5, metrics.horizontalAdvance(QStringLiteral("M")) * 24);
}

void ConsiderationTabManager::saveViewColumnWidths(int logicalIndex)
{
    // 最終列はドックのリサイズでも伸縮するため保存対象から除く。
    if (logicalIndex >= 5 || !m_considerationModel) return;
    QList<int> widths;
    for (int col = 0; col < 5; ++col) widths.append(m_considerationView->columnWidth(col));
    widths.append(0);
    AnalysisSettings::setThinkingViewColumnWidths(2, widths);
}

void ConsiderationTabManager::saveInfoColumnWidths()
{
    AnalysisSettings::setEngineInfoColumnWidths(2, m_considerationInfo->columnWidths());
}

// ===================== スロット（UI関連） =====================

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
    if (m_considerationFontSize.increase()) applyFontSize();
}

void ConsiderationTabManager::onConsiderationFontDecrease()
{
    if (m_considerationFontSize.decrease()) applyFontSize();
}

void ConsiderationTabManager::applyFontSize()
{
    const int size = m_considerationFontSize.fontSize();
    QFont font;
    font.setPointSize(size);

    // ツールバー要素のフォントサイズ更新
    // A+/A- ボタンはフォントサイズに合わせてボタンサイズも更新
    {
        const QFontMetrics fm(font);
        const int btnW = qMax(36, fm.horizontalAdvance(QStringLiteral("A+")) + 8);
        const int btnH = qMax(24, fm.height() + 4);
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
    if (m_elapsedTimeLabel) {
        m_elapsedTimeLabel->setFont(font);
        m_elapsedTimeLabel->setMinimumWidth(QFontMetrics(font).horizontalAdvance(tr("経過: 000:00")));
    }
    if (m_multiPVLabel) m_multiPVLabel->setFont(font);
    if (m_multiPVComboBox) m_multiPVComboBox->setFont(font);
    if (m_showArrowsCheckBox) m_showArrowsCheckBox->setFont(font);
    if (m_btnStopConsideration) {
        m_btnStopConsideration->setFont(font);
        // 開始・中止で幅が変わるとツールバーの折り返しが変わり、下の表が上下に動くため、
        // 長い方の表示に合わせる
        const QString currentText = m_btnStopConsideration->text();
        int width = 0;
        for (const QString& text : {tr("検討開始"), tr("検討中止")}) {
            m_btnStopConsideration->setText(text);
            width = qMax(width, m_btnStopConsideration->sizeHint().width());
        }
        m_btnStopConsideration->setText(currentText);
        m_btnStopConsideration->setMinimumWidth(width);
    }

    if (m_considerationInfo) m_considerationInfo->setFontSize(size);

    const QString headerStyle = TableStyles::thinking(size);

    if (m_considerationView) {
        const QSignalBlocker blocker(m_considerationView->horizontalHeader());
        m_considerationView->setFont(font);
        m_considerationView->setStyleSheet(headerStyle);
        const int rowHeight = KifuMoveDelegate::textHeight(m_considerationView->font()) + 4;
        m_considerationView->verticalHeader()->setDefaultSectionSize(rowHeight);
        applyViewColumnWidths();
    }
}
