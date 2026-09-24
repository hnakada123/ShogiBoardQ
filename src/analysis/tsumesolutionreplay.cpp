#include "tsumesolutionreplay.h"
#include "tsumecollection.h"
#include "tsumepositionanalyzer.h"
#include <algorithm>

TsumeSolutionReplay::TsumeSolutionReplay(QObject* parent) : QObject(parent)
{
    m_analyzer = new TsumePositionAnalyzer(this);
    connect(m_analyzer, &TsumePositionAnalyzer::finished, this, &TsumeSolutionReplay::evaluationFinished);
}

void TsumeSolutionReplay::configure(const QString& sfen, const QString& enginePath, TsumeProgressStore* store)
{
    m_analyzer->configure(enginePath, store);
    m_sfen = sfen;
    m_moves.clear();
    m_positions.clear();
    m_detail.clear();
    m_ply = m_requested = 0;
    m_loading = false;
}

void TsumeSolutionReplay::seek(int ply, int milliseconds)
{
    if (m_sfen.isEmpty()) return;
    m_requested = std::max(0, ply);
    if (available()) { displayRequested(); return; }
    if (m_loading) return;
    m_loading = true;
    m_detail.clear();
    emit positionChanged(m_sfen, {});
    emit stateChanged();
    m_analyzer->evaluate(m_sfen, milliseconds, true);
}

void TsumeSolutionReplay::cancel()
{
    m_analyzer->cancel();
    if (!m_loading) return;
    m_loading = false;
    m_detail = tr("正解手順の確認を中断しました。「再判定」でやり直せます。");
    emit stateChanged();
}

void TsumeSolutionReplay::evaluationFinished(const TsumeEvaluation& result)
{
    if (!m_loading) return;
    m_loading = false;
    if (result.status != TsumeEvaluation::Status::Mate || result.pv.size() != result.plies
        || !TsumeCollection::validMateLine(m_sfen, result.pv)) {
        m_detail = result.status == TsumeEvaluation::Status::NoMate
            ? tr("この局面には詰み手順がありません。")
            : tr("正解手順を確認できませんでした。判定時間を増やして「再判定」してください。");
        if (!result.detail.isEmpty()) m_detail += QLatin1Char('\n') + result.detail;
        emit stateChanged();
        return;
    }
    shogi::Position position;
    position.set_sfen(m_sfen.toStdString(), true);
    m_positions = {QString::fromStdString(position.to_sfen())};
    m_moves = result.pv;
    for (const auto& move : std::as_const(m_moves)) {
        position.apply_usi_move(move.toStdString()); // 上で手順全体を検証済み。
        m_positions.append(QString::fromStdString(position.to_sfen()));
    }
    displayRequested();
}

void TsumeSolutionReplay::displayRequested()
{
    m_ply = std::clamp(m_requested, 0, totalPlies());
    emit positionChanged(m_positions[m_ply], m_ply ? m_moves[m_ply - 1] : QString());
    emit stateChanged();
}
