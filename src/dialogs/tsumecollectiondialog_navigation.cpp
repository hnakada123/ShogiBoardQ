#include "tsumecollectiondialog.h"
#include "tsumeplaydialog.h"
#include "tsumepositionanalyzer.h"
#include "tsumepositionpreview.h"
#include "tsumeshogisettings.h"
#include "dialogutils.h"
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

void TsumeCollectionDialog::restoreLastFile()
{
    if (!m_file.isEmpty()) return;
    const auto preferences = TsumeshogiSettings::collectionPreferences();
    const QString file = preferences.lastFile.isEmpty() ? TsumeshogiSettings::playPreferences().lastFile : preferences.lastFile;
    if (QFileInfo::exists(file) && loadFile(file)) m_page->setValue(preferences.page);
}

void TsumeCollectionDialog::openFile()
{
    const auto path = QFileDialog::getOpenFileName(this, tr("詰将棋の局面集を開く"), m_file,
                                                 tr("局面集 (*.txt *.sfen *.usi);;すべてのファイル (*)"));
    if (!path.isEmpty()) loadFile(path);
}

bool TsumeCollectionDialog::loadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, windowTitle(), tr("ファイルを開けませんでした。\n%1").arg(file.errorString()));
        return false;
    }
    const auto parsed = TsumeCollection::parse(QString::fromUtf8(file.readAll()));
    if (parsed.problems.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("有効な詰将棋局面がありません。SFEN形式と玉方の玉を確認してください。"));
        return false;
    }
    cancelAnalysis();
    m_problems = parsed.problems;
    m_file = path;
    m_ids.clear();
    m_results.clear();
    for (const auto& problem : std::as_const(m_problems)) m_ids.append(TsumeCollection::positionId(problem.sfen));
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_fileLabel->setToolTip(path);
    refreshProgress();
    filterChanged();
    if (!parsed.invalidLines.isEmpty()) {
        QStringList lines;
        for (int line : parsed.invalidLines) lines.append(QString::number(line));
        QMessageBox::warning(this, windowTitle(), tr("次の行は形式が不正なため読み込めませんでした: %1").arg(lines.join(QStringLiteral(", "))));
    }
    return true;
}

void TsumeCollectionDialog::refreshProgress()
{
    m_progress.clear();
    for (const auto& id : std::as_const(m_ids)) {
        if (!m_progress.contains(id)) m_progress.insert(id, m_store->progress(id));
    }
    if (!m_store->error().isEmpty()) m_notice->setText(tr("履歴を保存できません: %1").arg(m_store->error()));
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
    cancelAnalysis();
    m_filtered.clear();
    const int filter = m_filter->currentIndex();
    for (qsizetype i = 0; i < m_problems.size(); ++i) {
        const auto progress = m_progress.value(m_ids[i]);
        if (filter == 0 || (filter == 1 && progress.attempts == 0) ||
            (filter == 2 && progress.attempts > 0 && progress.solves == 0) || (filter == 3 && progress.solves > 0))
            m_filtered.append(static_cast<int>(i));
    }
    const int count = static_cast<int>(m_filtered.size());
    const int pageSize = m_pageSize->currentData().toInt();
    const int pages = std::max(1, (count + pageSize - 1) / pageSize);
    { const QSignalBlocker blocker(m_page); m_page->setMaximum(pages); }
    m_page->setEnabled(count > 0);
    const int begin = (m_page->value() - 1) * pageSize;
    const int end = std::min(count, begin + pageSize);
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
    for (int row = begin; row < end; ++row) {
        const int index = m_filtered[row];
        auto* card = new QPushButton(m_scroll->widget());
        card->setObjectName(QStringLiteral("tsumeProblemCard"));
        card->setProperty("problemIndex", index);
        card->setMinimumWidth(350);
        card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        card->setToolTip(m_problems[index].sfen);
        card->setAutoDefault(false);
        auto* contents = new QVBoxLayout(card);
        auto* title = new QLabel(tr("第%1問（%2行目）").arg(index + 1).arg(m_problems[index].lineNumber), card);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        contents->addWidget(title);
        contents->addWidget(new TsumePositionPreview(m_problems[index].sfen, card));
        for (const auto& name : {QStringLiteral("cardLength"), QStringLiteral("cardProgress")}) {
            auto* label = new QLabel(card);
            label->setObjectName(name);
            label->setWordWrap(true);
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            contents->addWidget(label);
        }
        if (!m_results.contains(m_ids[index])) {
            const auto cache = m_store->cached(m_ids[index], m_analyzer->engineKey());
            if (cache && (m_engine->currentData().toString().isEmpty()
                          || cache->status != TsumeEvaluation::Status::Mate
                          || (cache->pv.size() == cache->plies && TsumeCollection::validMateLine(m_problems[index].sfen, cache->pv))))
                m_results.insert(m_ids[index], *cache);
        }
        DialogUtils::applyFontToAllChildren(card, font());
        updateCard(card);
        connect(card, &QPushButton::clicked, this, &TsumeCollectionDialog::startProblem);
        m_cards.append(card);
        if (!m_results.contains(m_ids[index]) && !m_queue.contains(index)) m_queue.append(index);
    }
    arrangeCards();
    m_scroll->verticalScrollBar()->setValue(0);
    if (!m_playing) m_analysisTimer.start(0);
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
    const int index = card->property("problemIndex").toInt();
    cancelAnalysis();
    m_playing = true;
    const int scroll = m_scroll->verticalScrollBar()->value();
    savePreferences();
    // exec() 中の一覧を hide() すると、一覧自身のイベントループも終了する。
    // 一覧は表示したままにし、対局中の入力は子ダイアログのモーダル制御で止める。
    {
        TsumePlayDialog play(this);
        play.setProblem(m_problems[index], index + 1, m_engine->currentData().toString(), m_store.get(), m_timeout->value());
        play.exec();
    }
    m_playing = false;
    // 対局開始時の再判定で確定した手数も一覧へ反映する。
    m_results.remove(m_ids[index]);
    refreshProgress();
    rebuildPage();
    m_scroll->verticalScrollBar()->setValue(scroll);
    raise();
    activateWindow();
}
