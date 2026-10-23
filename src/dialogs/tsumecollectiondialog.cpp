#include "tsumecollectiondialog.h"
#include "engineprofilestore.h"
#include "tsumemateengine.h"
#include "dialogfontscale.h"
#include "dialogutils.h"
#include "engineselector.h"
#include "tsumecollectionsession.h"
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
#include <QLayout>
#include "tsumeplaydialog.h"
#include "tsumepositionpreview.h"
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>

namespace {
/// 長手数の判定に使える登録エンジン（登録時に報告したオプションで最短手数を確認できるもの。KomoringHeights）
bool isMateEngine(const EngineEntry& engine)
{
    return TsumeMateEngine::canVerifyShortestMate(EngineProfileStore::loadOptions(engine.name));
}
} // namespace

TsumeCollectionDialog::TsumeCollectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("tsumeCollectionDialog"));
    setWindowTitle(tr("詰将棋問題一覧"));
    m_session = new TsumeCollectionSession(this);
    connect(m_session, &TsumeCollectionSession::evaluationChanged, this, &TsumeCollectionDialog::updateCards);
    buildUi();
    const auto preferences = TsumeshogiSettings::collectionPreferences();
    // 判定エンジン: 内蔵の Hayanagi か、登録した KomoringHeights（未保存なら登録したものを優先する）
    TsumeshogiSettings::migrateCollectionEngineSelection();
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineListSettings::tsumeCollectionSelection();
    engineOptions.builtinLabel = tr("Hayanagi（内蔵・31手まで）");
    engineOptions.filter = &isMateEngine;
    m_engineSelector = new EngineSelector({m_engine, nullptr, nullptr}, engineOptions, this);
    const int pageSizeIndex = m_pageSize->findData(preferences.pageSize);
    m_pageSize->setCurrentIndex(std::max(0, pageSizeIndex));
    m_filter->setCurrentIndex(std::clamp(preferences.filter, 0, m_filter->count() - 1));
    m_timeout->setValue(preferences.timeoutSec);
    m_session->setTimeoutMs(m_timeout->value() * 1000);
    connect(m_timeout, &QSpinBox::valueChanged, this, &TsumeCollectionDialog::timeoutChanged);
    connect(m_engineSelector, &EngineSelector::selectionChanged, this, &TsumeCollectionDialog::engineChanged);
    connect(m_pageSize, &QComboBox::currentIndexChanged, this, &TsumeCollectionDialog::filterChanged);
    connect(m_filter, &QComboBox::currentIndexChanged, this, &TsumeCollectionDialog::filterChanged);
    connect(m_page, &QSpinBox::valueChanged, this, &TsumeCollectionDialog::pageChanged);
    engineChanged();
    QTimer::singleShot(0, this, &TsumeCollectionDialog::restoreLastFile);
}

TsumeCollectionDialog::~TsumeCollectionDialog()
{
    m_session->cancelEvaluation();
    savePreferences();
}

void TsumeCollectionDialog::done(int result)
{
    m_session->cancelEvaluation();
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
    m_resetCollection = new QPushButton(tr("この局面集の履歴を初期化…"), this);
    m_resetCollection->setObjectName(QStringLiteral("tsumeResetCollectionProgress"));
    m_resetCollection->setAutoDefault(false);
    m_resetCollection->setEnabled(false);
    connect(m_resetCollection, &QPushButton::clicked, this, &TsumeCollectionDialog::resetCollectionProgress);
    fileRow->addWidget(m_resetCollection);
    auto* resetAll = new QPushButton(tr("全局面集の履歴を初期化…"), this);
    resetAll->setObjectName(QStringLiteral("tsumeResetAllProgress"));
    resetAll->setAutoDefault(false);
    connect(resetAll, &QPushButton::clicked, this, &TsumeCollectionDialog::resetAllProgress);
    fileRow->addWidget(resetAll);
    layout->addLayout(fileRow);

    auto* engineRow = new QHBoxLayout;
    engineRow->addWidget(new QLabel(tr("判定エンジン:"), this));
    m_engine = new QComboBox(this);
    m_engine->setObjectName(QStringLiteral("tsumeMateEngine"));
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
    auto* filterLabel = new QLabel(tr("挑戦状況:"), this);
    filters->addWidget(filterLabel);
    m_filter = new QComboBox(this);
    m_filter->setObjectName(QStringLiteral("tsumeProgressFilter"));
    m_filter->addItems({tr("すべて"), tr("未挑戦"), tr("挑戦済み・未正答"), tr("正答済み")});
    filterLabel->setBuddy(m_filter);
    m_filter->setAccessibleName(filterLabel->text());
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
    auto* fontDecrease = new QToolButton(this);
    fontDecrease->setObjectName(QStringLiteral("tsumeCollectionFontDecrease"));
    auto* fontIncrease = new QToolButton(this);
    fontIncrease->setObjectName(QStringLiteral("tsumeCollectionFontIncrease"));
    navigation->addWidget(fontDecrease);
    navigation->addWidget(fontIncrease);
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

    // 文字の大きさ（ページを替えて作り直す札はダイアログの文字を受け継ぐ）
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = fontDecrease;
    fontOptions.increase = fontIncrease;
    fontOptions.size = &TsumeshogiSettings::collectionDialogSize;
    DialogFontScale::install(this, TsumeshogiSettings::tsumeCollectionFontSize, fontOptions);
}

void TsumeCollectionDialog::savePreferences()
{
    TsumeshogiSettings::CollectionPreferences preferences;
    preferences.lastFile = m_session->filePath();
    preferences.pageSize = m_pageSize->currentData().toInt();
    preferences.page = m_page->value();
    preferences.filter = m_filter->currentIndex();
    preferences.timeoutSec = m_timeout->value();
    TsumeshogiSettings::collectionPreferences.set(preferences);
    m_engineSelector->saveSelection();
}

// ============================================================
// 詰み手順の解析
// ============================================================

void TsumeCollectionDialog::engineChanged()
{
    const std::optional<EngineSelector::Engine> engine = m_engineSelector->selected();
    m_session->setEngine(engine ? engine->path : QString());
    m_notice->setText(m_session->usesEngine()
        ? tr("玉方はHayanagi、詰み判定はKomoringHeightsです。参考手順の手数と、確認済みの詰み手数を区別して表示します。")
        : tr("内蔵判定は31手までです。長手数を解く場合はエンジン登録でKomoringHeightsを登録し、ここで選択してください。"));
    showStoreError();
    rebuildPage();
}

void TsumeCollectionDialog::timeoutChanged(int seconds)
{
    m_session->setTimeoutMs(seconds * 1000);
}

void TsumeCollectionDialog::showStoreError()
{
    if (!m_session->store()->error().isEmpty())
        m_notice->setText(tr("履歴を保存できません: %1").arg(m_session->store()->error()));
}

QString TsumeCollectionDialog::lengthText(int index) const
{
    if (m_session->isPending(index)) return tr("判定中…");
    if (const auto found = m_session->result(index)) {
        if (found->status == TsumeEvaluation::Status::Mate) return tr("%1手詰").arg(found->plies);
        if (found->status == TsumeEvaluation::Status::NoMate) return tr("不詰（確認済み）");
        return tr("判定不能（再判定できます）");
    }
    const auto& reference = m_session->problem(index).referenceMoves;
    return reference.isEmpty() ? tr("手数未判定") : tr("参考手順: %1手・未検証").arg(reference.size());
}

void TsumeCollectionDialog::updateCards()
{
    for (auto* card : std::as_const(m_cards)) updateCard(card);
}

void TsumeCollectionDialog::updateCard(QPushButton* card)
{
    const int index = card->property("problemIndex").toInt();
    const auto progress = m_session->progress(index);
    auto* length = card->findChild<QLabel*>(QStringLiteral("cardLength"));
    auto* history = card->findChild<QLabel*>(QStringLiteral("cardProgress"));
    length->setText(lengthText(index));
    const TsumeEvaluation result = m_session->result(index).value_or(TsumeEvaluation{});
    length->setToolTip(result.detail);
    history->setText(progress.solves > 0 ? tr("正答済み ／ 挑戦%1回・正答%2回").arg(progress.attempts).arg(progress.solves)
                     : progress.attempts > 0 ? tr("挑戦済み・未正答 ／ 挑戦%1回").arg(progress.attempts) : tr("未挑戦"));
    history->setToolTip(tr("最終挑戦: %1\n最終正答: %2").arg(progress.lastAttempt, progress.lastSolved));
    // QPushButton の標準 sizeHint は内部レイアウトの高さを含まない。
    card->setMinimumHeight(card->layout()->minimumSize().height());
    card->setAccessibleName(tr("第%1問 / %2 / %3").arg(index + 1).arg(length->text(), history->text()));
    card->setEnabled(result.status != TsumeEvaluation::Status::NoMate);
}

void TsumeCollectionDialog::reanalyzePage()
{
    QList<int> indexes;
    for (auto* card : std::as_const(m_cards)) indexes.append(card->property("problemIndex").toInt());
    m_session->reevaluate(indexes);
}

// ============================================================
// ファイルの読み込みと問題の移動
// ============================================================

void TsumeCollectionDialog::restoreLastFile()
{
    if (!m_session->filePath().isEmpty()) return;
    const auto preferences = TsumeshogiSettings::collectionPreferences();
    const QString file = preferences.lastFile.isEmpty() ? TsumeshogiSettings::playPreferences().lastFile : preferences.lastFile;
    if (QFileInfo::exists(file) && loadFile(file)) m_page->setValue(preferences.page);
}

void TsumeCollectionDialog::openFile()
{
    const auto path = QFileDialog::getOpenFileName(this, tr("詰将棋の局面集を開く"), m_session->filePath(),
                                                 tr("局面集 (*.txt *.sfen *.usi);;すべてのファイル (*)"));
    if (!path.isEmpty()) loadFile(path);
}

bool TsumeCollectionDialog::loadFile(const QString& path)
{
    QString error;
    QList<int> invalidLines;
    if (!m_session->load(path, &error, &invalidLines)) {
        QMessageBox::warning(this, windowTitle(), error.isEmpty()
            ? tr("有効な詰将棋局面がありません。SFEN形式と玉方の玉を確認してください。")
            : tr("ファイルを開けませんでした。\n%1").arg(error));
        return false;
    }
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_fileLabel->setToolTip(path);
    m_resetCollection->setEnabled(true);
    showStoreError();
    filterChanged();
    if (!invalidLines.isEmpty()) {
        QStringList lines;
        for (int line : std::as_const(invalidLines)) lines.append(QString::number(line));
        QMessageBox::warning(this, windowTitle(), tr("次の行は形式が不正なため読み込めませんでした: %1").arg(lines.join(QStringLiteral(", "))));
    }
    return true;
}

void TsumeCollectionDialog::resetCollectionProgress() { resetProgress(false); }
void TsumeCollectionDialog::resetAllProgress() { resetProgress(true); }

void TsumeCollectionDialog::resetProgress(bool allCollections)
{
    if (m_playing || (!allCollections && m_session->problemCount() == 0)) return;
    const QString message = allCollections
        ? tr("過去に開いたすべての局面集の挑戦・正答履歴を初期化し、すべての問題を未挑戦に戻します。\nこの操作は取り消せません。初期化しますか？")
        : tr("「%1」の全問題の挑戦・正答履歴を初期化し、未挑戦に戻します。\n表示中のページや絞り込みに関係なく、この局面集の全問題が対象です。\n同じ局面が他の局面集にもある場合、その共有履歴も初期化されます。\nこの操作は取り消せません。初期化しますか？").arg(QFileInfo(m_session->filePath()).fileName());
    QMessageBox confirmation(QMessageBox::Warning, tr("履歴の初期化"), message,
                             QMessageBox::Yes | QMessageBox::Cancel, this);
    confirmation.setOption(QMessageBox::Option::DontUseNativeDialog);
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.button(QMessageBox::Yes)->setText(tr("初期化する"));
    confirmation.setDefaultButton(QMessageBox::Cancel);
    confirmation.setEscapeButton(QMessageBox::Cancel);
    if (confirmation.exec() != QMessageBox::Yes) return;
    if (!m_session->resetProgress(allCollections)) {
        QMessageBox::warning(this, tr("履歴の初期化"), tr("履歴を初期化できませんでした。\n%1").arg(m_session->store()->error()));
        return;
    }
    rebuildPage();
}

void TsumeCollectionDialog::filterChanged()
{
    { const QSignalBlocker blocker(m_page); m_page->setValue(1); }
    rebuildPage();
}

void TsumeCollectionDialog::pageChanged() { rebuildPage(); }
void TsumeCollectionDialog::firstPage() { m_page->setValue(1); }
void TsumeCollectionDialog::previousPage() { m_page->setValue(m_page->value() - 1); }
void TsumeCollectionDialog::nextPage() { m_page->setValue(m_page->value() + 1); }
void TsumeCollectionDialog::lastPage() { m_page->setValue(m_page->maximum()); }

void TsumeCollectionDialog::rebuildPage()
{
    m_session->cancelEvaluation();
    m_filtered = m_session->filtered(static_cast<TsumeCollectionSession::Filter>(m_filter->currentIndex()));
    const auto page = TsumeCollectionSession::page(static_cast<int>(m_filtered.size()),
                                                   m_pageSize->currentData().toInt(), m_page->value());
    const int count = page.count;
    const int pages = page.pages;
    { const QSignalBlocker blocker(m_page); m_page->setMaximum(pages); }
    m_page->setEnabled(count > 0);
    const int begin = page.begin;
    const int end = page.end;
    m_summary->setText(tr("%1問中 %2〜%3問 ／ %4/%5ページ").arg(count).arg(count ? begin + 1 : 0).arg(end).arg(m_page->value()).arg(pages));
    m_first->setEnabled(m_page->value() > 1);
    m_previous->setEnabled(m_page->value() > 1);
    m_next->setEnabled(m_page->value() < pages);
    m_last->setEnabled(m_page->value() < pages);
    while (m_grid->count() > 0) {
        const std::unique_ptr<QLayoutItem> item(m_grid->takeAt(0));
        const std::unique_ptr<QWidget> widget(item->widget());
    }
    m_cards.clear();
    m_grid->setAlignment(count == 0 ? Qt::AlignTop : Qt::AlignTop | Qt::AlignLeft);
    if (count == 0) {
        auto* empty = new QLabel(m_session->problemCount() == 0
            ? tr("問題がありません。「局面集を開く…」からSFEN形式のファイルを選んでください。")
            : tr("この挑戦状況に一致する問題はありません。絞り込みを「すべて」にすると全問を表示できます。"), m_scroll->widget());
        empty->setObjectName(QStringLiteral("tsumeEmptyState"));
        empty->setWordWrap(true);
        empty->setMargin(16);
        m_grid->addWidget(empty, 0, 0);
    }
    QList<int> shown;
    for (int row = begin; row < end; ++row) {
        const int index = m_filtered[row];
        const TsumeProblem& problem = m_session->problem(index);
        auto* card = new QPushButton(m_scroll->widget());
        card->setObjectName(QStringLiteral("tsumeProblemCard"));
        card->setProperty("problemIndex", index);
        card->setMinimumWidth(350);
        card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        card->setToolTip(problem.sfen);
        card->setAutoDefault(false);
        auto* contents = new QVBoxLayout(card);
        auto* title = new QLabel(tr("第%1問（%2行目）").arg(index + 1).arg(problem.lineNumber), card);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        contents->addWidget(title);
        contents->addWidget(new TsumePositionPreview(problem.sfen, card));
        for (const auto& name : {QStringLiteral("cardLength"), QStringLiteral("cardProgress")}) {
            auto* label = new QLabel(card);
            label->setObjectName(name);
            label->setWordWrap(true);
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            contents->addWidget(label);
        }
        DialogUtils::applyFontToAllChildren(card, font());
        connect(card, &QPushButton::clicked, this, &TsumeCollectionDialog::startProblem);
        m_cards.append(card);
        shown.append(index);
    }
    // 表示した問題の手数を判定する（判定済みのものは結果を使う）
    m_session->evaluate(shown);
    updateCards();
    arrangeCards();
    m_scroll->verticalScrollBar()->setValue(0);
}

void TsumeCollectionDialog::arrangeCards()
{
    const int columns = std::max(1, (m_scroll->viewport()->width() - 20) / 360);
    for (qsizetype i = 0; i < m_cards.size(); ++i) {
        m_grid->addWidget(m_cards[i], static_cast<int>(i) / columns, static_cast<int>(i) % columns);
    }
}

void TsumeCollectionDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (m_grid) arrangeCards();
}

void TsumeCollectionDialog::startProblem()
{
    const auto* card = qobject_cast<QPushButton*>(sender());
    if (!card || m_playing) return;
    const int position = static_cast<int>(m_filtered.indexOf(card->property("problemIndex").toInt()));
    if (position < 0) return;
    m_session->setPaused(true);
    m_playing = true;
    const int scroll = m_scroll->verticalScrollBar()->value();
    savePreferences();
    m_played.clear();
    // exec() 中の一覧を hide() すると、一覧自身のイベントループも終了する。
    // 一覧は表示したままにし、対局中の入力は子ダイアログのモーダル制御で止める。
    {
        TsumePlayDialog play(this);
        m_play = &play;
        connect(&play, &TsumePlayDialog::previousProblemRequested, this, &TsumeCollectionDialog::playPreviousProblem);
        connect(&play, &TsumePlayDialog::nextProblemRequested, this, &TsumeCollectionDialog::playNextProblem);
        connect(&play, &TsumePlayDialog::movePlayed, this, &TsumeCollectionDialog::movePlayed);
        playProblemAt(position, m_timeout->value());
        play.exec();
        m_play = nullptr;
    }
    m_playing = false;
    // 対局開始時の再判定で確定した手数も一覧へ反映する。
    m_session->forgetResults(m_played);
    m_session->refreshProgress();
    m_session->setPaused(false);
    rebuildPage();
    m_scroll->verticalScrollBar()->setValue(scroll);
    raise();
    activateWindow();
}

/// 絞り込み後の表示順（m_filtered）で position 番目の問題を対局画面に出題する。
/// 対局中は一覧を組み直さないので、前後移動の間 m_filtered の並びは変わらない。
void TsumeCollectionDialog::playProblemAt(int position, int timeoutSec)
{
    if (!m_play || position < 0 || position >= m_filtered.size()) return;
    const int index = m_filtered[position];
    m_playPosition = position;
    if (!m_played.contains(index)) m_played.append(index);
    m_play->setProblem(m_session->problem(index), index + 1, m_engine->currentData().toString(), m_session->store(),
                       timeoutSec);
    m_play->setProblemNavigation(position > 0, position + 1 < m_filtered.size());
}

// 対局画面で変更した判定時間は、前後の問題へ移っても引き継ぐ。
void TsumeCollectionDialog::playPreviousProblem() { if (m_play) playProblemAt(m_playPosition - 1, m_play->timeoutSec()); }
void TsumeCollectionDialog::playNextProblem() { if (m_play) playProblemAt(m_playPosition + 1, m_play->timeoutSec()); }
