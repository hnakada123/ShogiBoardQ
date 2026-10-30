/// @file considerationpanel.cpp
/// @brief 検討タブ管理クラスの実装（状態管理・設定・イベント処理）

#include "considerationpanel.h"
#include "fontsizecontrol.h"
#include "engineselector.h"
#include "engineinfowidget.h"
#include "logcategories.h"
#include "analysissettings.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include "buttonstyles.h"
#include "flowlayout.h"
#include "shogienginethinkingmodel.h"
#include "thinkingtableview.h"
#include <QFont>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <initializer_list>

ConsiderationPanel::ConsiderationPanel(QWidget* parent)
    : ConsiderationPanel(nullptr, parent)
{
}

ConsiderationPanel::ConsiderationPanel(QAction* arrows, QWidget* parent)
    : QWidget(parent)
    , m_arrowsAction(arrows)
    , m_considerationFontSize(AnalysisSettings::considerationFontSize, 8, 24)
{
    if (!m_arrowsAction) {
        // 単独で使うとき
        m_arrowsAction = new QAction(this);
        m_arrowsAction->setCheckable(true);
        m_arrowsAction->setChecked(true);
    }
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    buildToolbarControls();
    layoutToolbar();
    loadEnginesAndSettings();
    buildConsiderationView();

    // 保存されている文字の大きさを適用
    applyFontSize();

    layout->addWidget(m_considerationToolbar);
    layout->addWidget(m_considerationInfo);
    layout->addWidget(m_considerationView, 1);
}

ConsiderationPanel::~ConsiderationPanel()
{
}

// ===================== 検討の条件 =====================

void ConsiderationPanel::onControlChanged()
{
    // 画面の値を読み直し、変わった条件を保存して知らせる
    ConsiderationSettings next = m_settings;
    const std::optional<EngineSelector::Engine> engine = m_engineSelector->selected();
    next.engineName = engine ? engine->name : QString();
    next.unlimitedTime = m_unlimitedTimeRadioButton->isChecked();
    next.byoyomiSec = m_byoyomiSecSpinBox->value();
    next.multiPV = m_multiPVComboBox->currentData().toInt();
    updateTimeControls();
    if (next == m_settings) return;
    m_settings = next;
    saveSettings();
    emit settingsChanged(m_settings);
}

void ConsiderationPanel::showSettings()
{
    const QSignalBlocker engineBlocker(m_engineComboBox);
    const QSignalBlocker unlimitedBlocker(m_unlimitedTimeRadioButton);
    const QSignalBlocker timedBlocker(m_considerationTimeRadioButton);
    const QSignalBlocker secondsBlocker(m_byoyomiSecSpinBox);
    const QSignalBlocker multiPVBlocker(m_multiPVComboBox);
    m_engineSelector->select(m_settings.engineName);
    (m_settings.unlimitedTime ? m_unlimitedTimeRadioButton : m_considerationTimeRadioButton)->setChecked(true);
    m_byoyomiSecSpinBox->setValue(m_settings.byoyomiSec);
    m_multiPVComboBox->setCurrentIndex(m_settings.multiPV - 1);
    updateTimeControls();
}

void ConsiderationPanel::saveSettings() const
{
    m_engineSelector->saveSelection();
    AnalysisSettings::consideration.set(m_settings);
}

void ConsiderationPanel::reloadEngines()
{
    m_engineSelector->reload();
}

void ConsiderationPanel::loadEnginesAndSettings()
{
    // エンジンの一覧（登録順）。保存した選択を復元し、無ければ先頭のエンジン
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineListSettings::considerationSelection();
    m_engineSelector = new EngineSelector({m_engineComboBox, m_btnEngineSettings, nullptr}, engineOptions, this);
    connect(m_engineSelector, &EngineSelector::settingsRequested,
            this, &ConsiderationPanel::engineSettingsRequested);
    m_settings = AnalysisSettings::consideration();
    if (const std::optional<EngineSelector::Engine> engine = m_engineSelector->selected())
        m_settings.engineName = engine->name;
    showSettings();

    // 利用者がどれを変えても、画面の値から条件を読み直す
    connect(m_engineSelector, &EngineSelector::selectionChanged, this, &ConsiderationPanel::onControlChanged);
    connect(m_unlimitedTimeRadioButton, &QRadioButton::toggled, this, &ConsiderationPanel::onControlChanged);
    connect(m_considerationTimeRadioButton, &QRadioButton::toggled, this, &ConsiderationPanel::onControlChanged);
    connect(m_byoyomiSecSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConsiderationPanel::onControlChanged);
    connect(m_multiPVComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConsiderationPanel::onControlChanged);
    // 矢印のチェックは検討の条件ではなく、盤の矢印の切り替えに結ぶ
    m_showArrowsCheckBox->setChecked(m_arrowsAction->isChecked());
    connect(m_showArrowsCheckBox, &QCheckBox::toggled, m_arrowsAction, &QAction::setChecked);
    connect(m_arrowsAction, &QAction::toggled, m_showArrowsCheckBox, &QCheckBox::setChecked);
}

// ===================== エンジン名 =====================

void ConsiderationPanel::setConsiderationEngineName(const QString& name)
{
    m_considerationInfo->setDisplayNameFallback(name);
}

// ===================== 実行の状態 =====================

void ConsiderationPanel::setRunState(RunState state)
{
    qCDebug(lcUi).noquote() << "[ConsiderationPanel::setRunState]" << static_cast<int>(state);
    m_runState = state;
    switch (state) {
    case RunState::Idle:
        resetElapsedTime();
        break;
    case RunState::Running:
        // 局面ごと・候補手の数を変えるたびに数え直す
        resetElapsedTime();
        m_elapsedTimer->start();
        break;
    case RunState::Waiting:
        m_elapsedTimer->stop();
        break;
    }
    // 時間設定は開始時にエンジンへ渡す。実行中に表示だけ変更されないよう固定する。
    updateTimeControls();
    updateRunButton();
}

void ConsiderationPanel::updateRunButton()
{
    const bool running = m_runState != RunState::Idle;
    m_btnStopConsideration->setText(running ? tr("検討中止") : tr("検討開始"));
    m_btnStopConsideration->setToolTip(running ? tr("検討を中止してエンジンを停止します") : tr("検討を開始します"));
    // 検討中は中止操作を優先して無効化しない
    m_btnStopConsideration->setEnabled(running || m_startConsiderationEnabled);
}

void ConsiderationPanel::onRunButtonClicked()
{
    if (m_runState == RunState::Idle) emit startConsiderationRequested();
    else emit stopConsiderationRequested();
}

void ConsiderationPanel::setStartConsiderationEnabled(bool enabled)
{
    m_startConsiderationEnabled = enabled;
    updateRunButton();
}

// ===================== 経過時間 =====================

void ConsiderationPanel::resetElapsedTime()
{
    m_elapsedTimer->stop();
    m_elapsedSeconds = 0;
    m_elapsedTimeLabel->setText(tr("経過: 0:00"));
}

void ConsiderationPanel::onElapsedTimerTick()
{
    m_elapsedSeconds++;
    // 時間を決めた検討は、その秒数で止める
    const int limit = m_settings.unlimitedTime ? 0 : m_settings.byoyomiSec;
    if (limit > 0 && m_elapsedSeconds >= limit) {
        m_elapsedTimer->stop();
        m_elapsedSeconds = limit;
    }
    m_elapsedTimeLabel->setText(tr("経過: %1:%2")
        .arg(m_elapsedSeconds / 60)
        .arg(m_elapsedSeconds % 60, 2, 10, QLatin1Char('0')));
}

// ===================== 設定の操作 =====================

void ConsiderationPanel::updateTimeControls()
{
    const bool idle = m_runState == RunState::Idle;
    const bool unlimited = !m_unlimitedTimeRadioButton || m_unlimitedTimeRadioButton->isChecked();
    m_unlimitedTimeRadioButton->setEnabled(idle);
    m_considerationTimeRadioButton->setEnabled(idle);
    const bool enabled = idle && !unlimited;
    m_byoyomiSecSpinBox->setEnabled(enabled);
    m_byoyomiSecUnitLabel->setEnabled(enabled);
}

// ============================================================
// ConsiderationPanel のUI構築・フォント管理
// ============================================================

void ConsiderationPanel::clearViewSelection(const ShogiEngineThinkingModel* model)
{
    if (m_considerationView->thinkingModel() == model) m_considerationView->clearRowSelection();
}

void ConsiderationPanel::buildToolbarControls()
{
    // ツールバー（FlowLayout使用）
    m_considerationToolbar = new QWidget(this);
    m_considerationToolbar->setObjectName(QStringLiteral("considerationToolbar"));

    // 文字の大きさ（A-/A+。ボタンの寸法は applyFontSize が文字に合わせる）
    const auto* fontButtons = FontSizeControl::createToolButtons(m_considerationToolbar, &m_considerationFontSize,
        QStringLiteral("considerationFontDecrease"), QStringLiteral("considerationFontIncrease"));
    m_btnConsiderationFontDecrease = qobject_cast<QToolButton*>(fontButtons->decreaseButton());
    m_btnConsiderationFontIncrease = qobject_cast<QToolButton*>(fontButtons->increaseButton());
    connect(&m_considerationFontSize, &FontSizeHelper::fontSizeChanged, this, &ConsiderationPanel::applyFontSize);

    // エンジン選択コンボボックス
    m_engineComboBox = new QComboBox(m_considerationToolbar);
    m_engineComboBox->setObjectName(QStringLiteral("considerationEngine"));
    m_engineComboBox->setToolTip(tr("検討に使用するエンジンを選択します"));
    m_engineComboBox->setMinimumWidth(150);
    m_engineComboBox->setMinimumContentsLength(24);
    m_engineComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

    // エンジン設定ボタン
    m_btnEngineSettings = new QPushButton(tr("エンジン設定"), m_considerationToolbar);
    m_btnEngineSettings->setObjectName(QStringLiteral("considerationEngineSettings"));
    m_btnEngineSettings->setToolTip(tr("選択したエンジンの設定を変更します"));
    m_btnEngineSettings->setStyleSheet(ButtonStyles::panelToolButton()
        + QStringLiteral("QPushButton { padding: 2px 8px; }"));

    // 時間無制限ラジオボタン
    m_unlimitedTimeRadioButton = new QRadioButton(tr("時間無制限"), m_considerationToolbar);
    m_unlimitedTimeRadioButton->setObjectName(QStringLiteral("considerationUnlimited"));
    m_unlimitedTimeRadioButton->setToolTip(tr("時間制限なしで検討します"));

    // 検討時間ラジオボタン
    m_considerationTimeRadioButton = new QRadioButton(tr("検討時間"), m_considerationToolbar);
    m_considerationTimeRadioButton->setObjectName(QStringLiteral("considerationTimed"));
    m_considerationTimeRadioButton->setToolTip(tr("指定した秒数まで検討します"));

    // 検討時間スピンボックス
    m_byoyomiSecSpinBox = new QSpinBox(m_considerationToolbar);
    m_byoyomiSecSpinBox->setObjectName(QStringLiteral("considerationSeconds"));
    m_byoyomiSecSpinBox->setRange(ConsiderationSettings::kMinByoyomiSec, ConsiderationSettings::kMaxByoyomiSec);
    m_byoyomiSecSpinBox->setToolTip(tr("検討時間（秒）を指定します"));

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
    connect(m_elapsedTimer, &QTimer::timeout, this, &ConsiderationPanel::onElapsedTimerTick);

    // 候補手の数
    m_multiPVLabel = new QLabel(tr("候補手の数"), m_considerationToolbar);
    m_multiPVComboBox = new QComboBox(m_considerationToolbar);
    m_multiPVComboBox->setObjectName(QStringLiteral("considerationMultiPV"));
    for (int i = 1; i <= ConsiderationSettings::kMaxMultiPV; ++i) {
        m_multiPVComboBox->addItem(tr("%1手").arg(i), i);
    }
    m_multiPVComboBox->setToolTip(tr("評価値が大きい順に表示する候補手の数を指定します"));

    // 矢印表示チェックボックス
    m_showArrowsCheckBox = new QCheckBox(tr("矢印表示"), m_considerationToolbar);
    m_showArrowsCheckBox->setObjectName(QStringLiteral("considerationArrows"));
    m_showArrowsCheckBox->setToolTip(tr("最善手の矢印を盤面に表示します"));

    // 検討開始/中止ボタン（初期状態は「検討開始」）
    m_btnStopConsideration = new QToolButton(m_considerationToolbar);
    m_btnStopConsideration->setObjectName(QStringLiteral("considerationStartStop"));
    m_btnStopConsideration->setText(tr("検討開始"));
    m_btnStopConsideration->setToolTip(tr("検討を開始します"));
    m_btnStopConsideration->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_btnStopConsideration, &QToolButton::clicked, this, &ConsiderationPanel::onRunButtonClicked);
}

void ConsiderationPanel::layoutToolbar()
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
}

void ConsiderationPanel::buildConsiderationView()
{
    // EngineInfoWidget（検討タブ用）
    m_considerationInfo = new EngineInfoWidget(AnalysisSettings::ThinkingTable::Consideration, this, false);
    m_considerationInfo->setObjectName(QStringLiteral("considerationInfo"));
    m_considerationInfo->setFontSize(m_considerationFontSize.fontSize());

    // 読み筋の表
    m_considerationView = new ThinkingTableView(AnalysisSettings::ThinkingTable::Consideration, this);
    m_considerationView->setObjectName(QStringLiteral("considerationView"));
    connect(m_considerationView, &ThinkingTableView::pvRowClicked, this, &ConsiderationPanel::pvRowClicked);
    connect(m_considerationView, &ThinkingTableView::thinkingRowContextMenuAboutToShow,
            this, &ConsiderationPanel::rowContextMenuAboutToShow);

    // buildConsiderationUi() より前に setConsiderationThinkingModel() が呼ばれた場合、
    // 保存済みモデルをビューに適用する
    if (m_considerationModel) m_considerationView->setThinkingModel(m_considerationModel);
}

// ===================== モデル設定 =====================

void ConsiderationPanel::setConsiderationThinkingModel(ShogiEngineThinkingModel* m)
{
    m_considerationModel = m;
    if (m) m_considerationView->setThinkingModel(m);
}

// ===================== スロット（UI関連） =====================

void ConsiderationPanel::applyFontSize()
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
        m_btnConsiderationFontDecrease->setFont(font);
        m_btnConsiderationFontDecrease->setFixedSize(btnW, btnH);
        m_btnConsiderationFontIncrease->setFont(font);
        m_btnConsiderationFontIncrease->setFixedSize(btnW, btnH);
    }
    m_engineComboBox->setFont(font);
    m_btnEngineSettings->setFont(font);
    m_unlimitedTimeRadioButton->setFont(font);
    m_considerationTimeRadioButton->setFont(font);
    m_byoyomiSecSpinBox->setFont(font);
    m_byoyomiSecUnitLabel->setFont(font);
    m_elapsedTimeLabel->setFont(font);
    m_elapsedTimeLabel->setMinimumWidth(QFontMetrics(font).horizontalAdvance(tr("経過: 000:00")));
    m_multiPVLabel->setFont(font);
    m_multiPVComboBox->setFont(font);
    m_showArrowsCheckBox->setFont(font);
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

    m_considerationInfo->setFontSize(size);
    m_considerationView->setFontPointSize(size);
}
