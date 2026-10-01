/// @file considerationtabmanager_ui.cpp
/// @brief ConsiderationTabManager のUI構築・フォント管理

#include "considerationtabmanager.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"
#include "engineinfowidget.h"
#include "flowlayout.h"
#include "logcategories.h"
#include "numericrightaligncommadelegate.h"
#include "pvboardbuttondelegate.h"
#include "tablestyles.h"
#include "analysissettings.h"
#include "shogienginethinkingmodel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <initializer_list>

void ConsiderationTabManager::buildConsiderationUi(QWidget* parentWidget)
{
    // SettingsServiceからフォントサイズを読み込み
    m_considerationFontSize = AnalysisSettings::considerationFontSize();

    buildToolbarControls(parentWidget);
    layoutToolbar();
    buildConsiderationView(parentWidget);

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
    m_considerationInfo = new EngineInfoWidget(parentWidget, false, false);
    m_considerationInfo->setObjectName(QStringLiteral("considerationInfo"));
    m_considerationInfo->setWidgetIndex(2);
    m_considerationInfo->setFontSize(m_considerationFontSize);
    m_considerationInfo->setColumnWidths(AnalysisSettings::engineInfoColumnWidths(2));
    connect(m_considerationInfo, &EngineInfoWidget::columnWidthChanged,
            this, &ConsiderationTabManager::saveInfoColumnWidths);

    // 読み筋テーブルビュー
    m_considerationView = new QTableView(parentWidget);
    m_considerationView->setObjectName(QStringLiteral("considerationView"));

    m_considerationView->setShowGrid(false);
    m_considerationView->setMouseTracking(true);
    m_considerationView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_considerationView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_considerationView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_considerationView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_considerationView->setItemDelegateForColumn(4, new PvBoardButtonDelegate(m_considerationView));

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
}

// ===================== モデル設定 =====================

void ConsiderationTabManager::setConsiderationThinkingModel(ShogiEngineThinkingModel* m)
{
    m_considerationModel = m;
    if (m_considerationView && m) {
        const QSignalBlocker blocker(m_considerationView->horizontalHeader());
        m_considerationView->setModel(m);
        // 数値列の右寄せ＆3桁カンマ
        // delegate は m_considerationView を Qt parent として生成されるため、自動削除される
        auto* delegate = new NumericRightAlignCommaDelegate(m_considerationView);
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
        m_considerationFontSize = size;
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
        if (m_btnStopConsideration) m_btnStopConsideration->setFont(font);

        if (m_considerationInfo) m_considerationInfo->setFontSize(size);

        const QString headerStyle = TableStyles::thinking(size);

        if (m_considerationView) {
            const QSignalBlocker blocker(m_considerationView->horizontalHeader());
            m_considerationView->setFont(font);
            m_considerationView->setStyleSheet(headerStyle);
            const int rowHeight = m_considerationView->fontMetrics().height() + 4;
            m_considerationView->verticalHeader()->setDefaultSectionSize(rowHeight);
            applyViewColumnWidths();
        }

        AnalysisSettings::setConsiderationFontSize(size);
    });
    m_considerationFontManager->apply();
}
