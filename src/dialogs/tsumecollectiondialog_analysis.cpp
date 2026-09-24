#include "tsumecollectiondialog.h"
#include "tsumepositionanalyzer.h"
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSpinBox>

void TsumeCollectionDialog::cancelAnalysis()
{
    m_analysisTimer.stop();
    m_analyzer->cancel();
    m_pending = -1;
    m_queue.clear();
}

void TsumeCollectionDialog::engineChanged()
{
    cancelAnalysis();
    m_analyzer->configure(m_engine->currentData().toString(), m_store.get());
    m_results.clear();
    m_notice->setText(m_engine->currentData().toString().isEmpty()
        ? tr("内蔵判定は31手までです。長手数を解く場合はエンジン登録でKomoringHeightsを登録し、ここで選択してください。")
        : tr("玉方はHayanagi、詰み判定はKomoringHeightsです。参考手順の手数と、確認済みの詰み手数を区別して表示します。"));
    if (!m_store->error().isEmpty()) m_notice->setText(tr("履歴を保存できません: %1").arg(m_store->error()));
    rebuildPage();
}

QString TsumeCollectionDialog::lengthText(int index) const
{
    if (m_pending >= 0 && m_ids[m_pending] == m_ids[index]) return tr("判定中…");
    const auto found = m_results.constFind(m_ids[index]);
    if (found != m_results.cend()) {
        if (found->status == TsumeEvaluation::Status::Mate) return tr("%1手詰").arg(found->plies);
        if (found->status == TsumeEvaluation::Status::NoMate) return tr("不詰（確認済み）");
        return tr("判定不能（再判定できます）");
    }
    const auto& reference = m_problems[index].referenceMoves;
    return reference.isEmpty() ? tr("手数未判定") : tr("参考手順: %1手・未検証").arg(reference.size());
}

void TsumeCollectionDialog::updateCard(QPushButton* card)
{
    const int index = card->property("problemIndex").toInt();
    const auto progress = m_progress.value(m_ids[index]);
    auto* length = card->findChild<QLabel*>(QStringLiteral("cardLength"));
    auto* history = card->findChild<QLabel*>(QStringLiteral("cardProgress"));
    length->setText(lengthText(index));
    const auto result = m_results.value(m_ids[index]);
    length->setToolTip(result.detail);
    history->setText(progress.solves > 0 ? tr("正答済み ／ 挑戦%1回・正答%2回").arg(progress.attempts).arg(progress.solves)
                     : progress.attempts > 0 ? tr("挑戦済み・未正答 ／ 挑戦%1回").arg(progress.attempts) : tr("未挑戦"));
    history->setToolTip(tr("最終挑戦: %1\n最終正答: %2").arg(progress.lastAttempt, progress.lastSolved));
    // QPushButton の標準 sizeHint は内部レイアウトの高さを含まない。
    card->setMinimumHeight(card->layout()->minimumSize().height());
    card->setAccessibleName(tr("第%1問 / %2 / %3").arg(index + 1).arg(length->text(), history->text()));
    card->setEnabled(result.status != TsumeEvaluation::Status::NoMate);
}

void TsumeCollectionDialog::analyzeNext()
{
    if (m_playing || m_pending >= 0) return;
    while (!m_queue.isEmpty()) {
        const int index = m_queue.takeFirst();
        if (m_results.contains(m_ids[index])) continue;
        m_pending = index;
        for (auto* card : std::as_const(m_cards)) updateCard(card);
        m_analyzer->evaluate(m_problems[index].sfen, m_timeout->value() * 1000);
        return;
    }
}

void TsumeCollectionDialog::analysisFinished(const TsumeEvaluation& result)
{
    if (m_pending < 0 || m_playing) return;
    m_results.insert(m_ids[m_pending], result);
    m_pending = -1;
    for (auto* card : std::as_const(m_cards)) updateCard(card);
    m_analysisTimer.start(0);
}

void TsumeCollectionDialog::reanalyzePage()
{
    cancelAnalysis();
    for (auto* card : std::as_const(m_cards)) {
        const int index = card->property("problemIndex").toInt();
        m_results.remove(m_ids[index]);
        m_store->removeCached(m_ids[index], m_analyzer->engineKey());
        updateCard(card);
        m_queue.append(index);
    }
    m_analysisTimer.start(0);
}
