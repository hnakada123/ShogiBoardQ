/// @file tsumecollectionsession.cpp
/// @brief 開いている詰将棋の局面集の実装

#include "tsumecollectionsession.h"
#include "boundedfile.h"

#include "tsumepositionanalyzer.h"

#include <algorithm>

TsumeCollectionSession::TsumeCollectionSession(QObject* parent)
    : QObject(parent)
    , m_store(std::make_unique<TsumeProgressStore>())
    , m_analyzer(new TsumePositionAnalyzer(this))
{
    m_store->open();
    m_evaluationTimer.setSingleShot(true);
    connect(&m_evaluationTimer, &QTimer::timeout, this, &TsumeCollectionSession::evaluateNext);
    connect(m_analyzer, &TsumePositionAnalyzer::finished, this, &TsumeCollectionSession::onEvaluated);
}

TsumeCollectionSession::~TsumeCollectionSession() = default;

// ============================================================
// 局面集
// ============================================================

bool TsumeCollectionSession::load(const QString& path, QString* errorMessage, QList<int>* invalidLines)
{
    QByteArray contents;
    const BoundedFile::Result read = BoundedFile::read(path, BoundedFile::kMaxCollectionBytes, contents);
    if (read != BoundedFile::Result::Ok) {
        if (errorMessage) *errorMessage = BoundedFile::describe(read, path, BoundedFile::kMaxCollectionBytes);
        return false;
    }
    const TsumeCollection::Result parsed = TsumeCollection::parse(QString::fromUtf8(contents));
    if (parsed.problems.isEmpty()) {
        if (errorMessage) errorMessage->clear();
        return false;
    }
    cancelEvaluation();
    m_problems = parsed.problems;
    m_ids = parsed.positionIds;
    m_file = path;
    m_results.clear();
    m_store->setVerifiedCollection(contents, parsed);
    refreshProgress();
    if (invalidLines) *invalidLines = parsed.invalidLines;
    return true;
}

// ============================================================
// 挑戦状況
// ============================================================

TsumeProgressStore::Progress TsumeCollectionSession::progress(int index) const
{
    return m_progress.value(m_ids.value(index));
}

void TsumeCollectionSession::refreshProgress()
{
    m_progress = m_store->progress(m_ids);
}

bool TsumeCollectionSession::resetProgress(bool allCollections)
{
    const bool reset = allCollections ? m_store->resetAllProgress() : m_store->resetProgress(m_ids);
    if (reset) refreshProgress();
    return reset;
}

QList<int> TsumeCollectionSession::filtered(Filter filter) const
{
    QList<int> indexes;
    for (int i = 0; i < problemCount(); ++i) {
        const auto progress = this->progress(i);
        const bool match = filter == Filter::All || (filter == Filter::Unattempted && progress.attempts == 0)
            || (filter == Filter::Unsolved && progress.attempts > 0 && progress.solves == 0)
            || (filter == Filter::Solved && progress.solves > 0);
        if (match) indexes.append(i);
    }
    return indexes;
}

TsumeCollectionSession::Page TsumeCollectionSession::page(int count, int pageSize, int pageNumber)
{
    Page page;
    page.count = count;
    page.pages = std::max(1, (count + pageSize - 1) / pageSize);
    page.begin = (std::clamp(pageNumber, 1, page.pages) - 1) * pageSize;
    page.end = std::min(count, page.begin + pageSize);
    return page;
}

// ============================================================
// 手数の判定
// ============================================================

void TsumeCollectionSession::setEngine(const QString& enginePath)
{
    cancelEvaluation();
    m_enginePath = enginePath;
    m_analyzer->configure(enginePath, m_store.get());
    m_results.clear();
    emit evaluationChanged();
}

void TsumeCollectionSession::evaluate(const QList<int>& indexes)
{
    for (int index : indexes) {
        const QString& id = m_ids.at(index);
        if (m_results.contains(id)) continue;
        if (const auto cached = m_store->validatedCached(id, m_analyzer->engineKey(), usesEngine())) {
            m_results.insert(id, *cached);
            continue;
        }
        if (!m_queue.contains(index)) m_queue.append(index);
    }
    if (!m_paused) m_evaluationTimer.start(0);
}

void TsumeCollectionSession::reevaluate(const QList<int>& indexes)
{
    cancelEvaluation();
    for (int index : indexes) {
        m_results.remove(m_ids.at(index));
        m_store->removeCached(m_ids.at(index), m_analyzer->engineKey());
        m_queue.append(index);
    }
    emit evaluationChanged();
    if (!m_paused) m_evaluationTimer.start(0);
}

void TsumeCollectionSession::cancelEvaluation()
{
    m_evaluationTimer.stop();
    m_analyzer->cancel();
    m_pending = -1;
    m_queue.clear();
}

void TsumeCollectionSession::setPaused(bool paused)
{
    m_paused = paused;
    if (paused) cancelEvaluation();
    else if (!m_queue.isEmpty()) m_evaluationTimer.start(0);
}

std::optional<TsumeEvaluation> TsumeCollectionSession::result(int index) const
{
    const auto found = m_results.constFind(m_ids.value(index));
    if (found == m_results.cend()) return std::nullopt;
    return *found;
}

void TsumeCollectionSession::forgetResults(const QList<int>& indexes)
{
    for (int index : indexes) m_results.remove(m_ids.value(index));
}

void TsumeCollectionSession::evaluateNext()
{
    if (m_paused || m_pending >= 0) return;
    while (!m_queue.isEmpty()) {
        const int index = m_queue.takeFirst();
        if (m_results.contains(m_ids.at(index))) continue;
        m_pending = index;
        emit evaluationChanged();
        m_analyzer->evaluate(m_problems.at(index).sfen, m_timeoutMs);
        return;
    }
}

void TsumeCollectionSession::onEvaluated(const TsumeEvaluation& result)
{
    if (m_pending < 0 || m_paused) return;
    m_results.insert(m_ids.at(m_pending), result);
    m_pending = -1;
    emit evaluationChanged();
    m_evaluationTimer.start(0);
}
