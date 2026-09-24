#include "tsumecollectiondialog.h"
#include "dialogutils.h"
#include "buttonstyles.h"
#include "enginelistsettings.h"
#include "tsumepositionanalyzer.h"
#include "tsumeshogisettings.h"
#include <QComboBox>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

TsumeCollectionDialog::TsumeCollectionDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper({TsumeshogiSettings::tsumeCollectionFontSize(), 8, 24, 1,
                    TsumeshogiSettings::setTsumeCollectionFontSize})
{
    setObjectName(QStringLiteral("tsumeCollectionDialog"));
    setWindowTitle(tr("詰将棋の問題一覧"));
    m_store = std::make_unique<TsumeProgressStore>();
    m_store->open();
    m_analyzer = new TsumePositionAnalyzer(this);
    m_analysisTimer.setSingleShot(true);
    connect(&m_analysisTimer, &QTimer::timeout, this, &TsumeCollectionDialog::analyzeNext);
    connect(m_analyzer, &TsumePositionAnalyzer::finished, this, &TsumeCollectionDialog::analysisFinished);
    buildUi();
    applyFontSize();
    const auto preferences = TsumeshogiSettings::collectionPreferences();
    DialogUtils::restoreDialogSize(this, preferences.size);
    int engineIndex = m_engine->findData(preferences.enginePath);
    if (preferences.enginePath.isEmpty()) engineIndex = m_engine->count() > 1 ? 1 : 0;
    if (preferences.enginePath == QStringLiteral("@hayanagi")) engineIndex = 0;
    if (engineIndex < 0) engineIndex = m_engine->count() > 1 ? 1 : 0;
    m_engine->setCurrentIndex(engineIndex);
    const int pageSizeIndex = m_pageSize->findData(preferences.pageSize);
    m_pageSize->setCurrentIndex(std::max(0, pageSizeIndex));
    m_filter->setCurrentIndex(std::clamp(preferences.filter, 0, 3));
    m_timeout->setValue(std::clamp(preferences.timeoutSec, 1, 600));
    connect(m_engine, &QComboBox::currentIndexChanged, this, &TsumeCollectionDialog::engineChanged);
    connect(m_pageSize, &QComboBox::currentIndexChanged, this, &TsumeCollectionDialog::filterChanged);
    connect(m_filter, &QComboBox::currentIndexChanged, this, &TsumeCollectionDialog::filterChanged);
    connect(m_page, &QSpinBox::valueChanged, this, &TsumeCollectionDialog::pageChanged);
    engineChanged();
    QTimer::singleShot(0, this, &TsumeCollectionDialog::restoreLastFile);
}

TsumeCollectionDialog::~TsumeCollectionDialog()
{
    cancelAnalysis();
    savePreferences();
}

void TsumeCollectionDialog::done(int result)
{
    cancelAnalysis();
    savePreferences();
    QDialog::done(result);
}

void TsumeCollectionDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    auto* fileRow = new QHBoxLayout;
    auto* open = new QPushButton(tr("局面集を開く…"), this);
    open->setObjectName(QStringLiteral("tsumeOpenFile"));
    connect(open, &QPushButton::clicked, this, &TsumeCollectionDialog::openFile);
    m_fileLabel = new QLabel(tr("局面集を選択してください。"), this);
    m_fileLabel->setTextFormat(Qt::PlainText);
    fileRow->addWidget(open);
    fileRow->addWidget(m_fileLabel, 1);
    layout->addLayout(fileRow);

    auto* engineRow = new QHBoxLayout;
    engineRow->addWidget(new QLabel(tr("判定エンジン:"), this));
    m_engine = new QComboBox(this);
    m_engine->setObjectName(QStringLiteral("tsumeMateEngine"));
    m_engine->addItem(tr("Hayanagi（内蔵・31手まで）"), QString());
    for (const auto& engine : EngineListSettings::loadEngines()) {
        if (engine.name.contains(QStringLiteral("KomoringHeights"), Qt::CaseInsensitive)) m_engine->addItem(engine.name, engine.path);
    }
    engineRow->addWidget(m_engine, 1);
    engineRow->addWidget(new QLabel(tr("判定時間:"), this));
    m_timeout = new QSpinBox(this);
    m_timeout->setObjectName(QStringLiteral("tsumeCollectionTimeout"));
    m_timeout->setRange(1, 600);
    m_timeout->setSuffix(tr(" 秒"));
    engineRow->addWidget(m_timeout);
    auto* reanalyze = new QPushButton(tr("このページを再判定"), this);
    reanalyze->setObjectName(QStringLiteral("tsumeReanalyzePage"));
    connect(reanalyze, &QPushButton::clicked, this, &TsumeCollectionDialog::reanalyzePage);
    engineRow->addWidget(reanalyze);
    layout->addLayout(engineRow);
    m_notice = new QLabel(this);
    m_notice->setWordWrap(true);
    layout->addWidget(m_notice);

    auto* filters = new QHBoxLayout;
    filters->addWidget(new QLabel(tr("表示件数:"), this));
    m_pageSize = new QComboBox(this);
    m_pageSize->setObjectName(QStringLiteral("tsumePageSize"));
    for (int count : {10, 20, 50, 100}) m_pageSize->addItem(QString::number(count), count);
    filters->addWidget(m_pageSize);
    m_filter = new QComboBox(this);
    m_filter->setObjectName(QStringLiteral("tsumeProgressFilter"));
    m_filter->addItems({tr("すべて"), tr("未挑戦"), tr("挑戦済み・未正答"), tr("正答済み")});
    filters->addWidget(m_filter);
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("tsumePageSummary"));
    filters->addWidget(m_summary, 1);
    layout->addLayout(filters);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    auto* cards = new QWidget(m_scroll);
    m_grid = new QGridLayout(cards);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scroll->setWidget(cards);
    layout->addWidget(m_scroll, 1);

    auto* navigation = new QHBoxLayout;
    m_fontDecrease = new QToolButton(this);
    m_fontDecrease->setObjectName(QStringLiteral("tsumeCollectionFontDecrease"));
    m_fontDecrease->setText(QStringLiteral("A-"));
    m_fontDecrease->setToolTip(tr("文字サイズを縮小"));
    m_fontDecrease->setStyleSheet(ButtonStyles::fontButton());
    m_fontIncrease = new QToolButton(this);
    m_fontIncrease->setObjectName(QStringLiteral("tsumeCollectionFontIncrease"));
    m_fontIncrease->setText(QStringLiteral("A+"));
    m_fontIncrease->setToolTip(tr("文字サイズを拡大"));
    m_fontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_fontDecrease, &QToolButton::clicked, this, &TsumeCollectionDialog::onFontDecrease);
    connect(m_fontIncrease, &QToolButton::clicked, this, &TsumeCollectionDialog::onFontIncrease);
    navigation->addWidget(m_fontDecrease);
    navigation->addWidget(m_fontIncrease);
    m_first = new QPushButton(tr("最初"), this);
    m_previous = new QPushButton(tr("前ページ"), this);
    m_next = new QPushButton(tr("次ページ"), this);
    m_next->setObjectName(QStringLiteral("tsumeNextPage"));
    m_last = new QPushButton(tr("最後"), this);
    m_page = new QSpinBox(this);
    m_page->setObjectName(QStringLiteral("tsumePageNumber"));
    m_page->setRange(1, 1);
    connect(m_first, &QPushButton::clicked, this, &TsumeCollectionDialog::firstPage);
    connect(m_previous, &QPushButton::clicked, this, &TsumeCollectionDialog::previousPage);
    connect(m_next, &QPushButton::clicked, this, &TsumeCollectionDialog::nextPage);
    connect(m_last, &QPushButton::clicked, this, &TsumeCollectionDialog::lastPage);
    navigation->addWidget(m_first);
    navigation->addWidget(m_previous);
    navigation->addWidget(m_page);
    navigation->addWidget(m_next);
    navigation->addWidget(m_last);
    navigation->addStretch();
    auto* close = new QPushButton(tr("閉じる"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    navigation->addWidget(close);
    layout->addLayout(navigation);
}

void TsumeCollectionDialog::savePreferences()
{
    TsumeshogiSettings::CollectionPreferences preferences;
    preferences.size = size();
    preferences.lastFile = m_file;
    preferences.enginePath = m_engine->currentIndex() == 0 ? QStringLiteral("@hayanagi") : m_engine->currentData().toString();
    preferences.pageSize = m_pageSize->currentData().toInt();
    preferences.page = m_page->value();
    preferences.filter = m_filter->currentIndex();
    preferences.timeoutSec = m_timeout->value();
    TsumeshogiSettings::setCollectionPreferences(preferences);
}

void TsumeCollectionDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void TsumeCollectionDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void TsumeCollectionDialog::applyFontSize()
{
    QFont f = font();
    f.setPointSize(m_fontHelper.fontSize());
    DialogUtils::applyFontToAllChildren(this, f);
    for (auto* combo : {m_engine, m_pageSize, m_filter}) combo->view()->setFont(f);
    m_fontDecrease->setEnabled(m_fontHelper.fontSize() > 8);
    m_fontIncrease->setEnabled(m_fontHelper.fontSize() < 24);
}
