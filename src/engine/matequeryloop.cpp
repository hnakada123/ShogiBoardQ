/// @file matequeryloop.cpp
/// @brief 引き出し型の問い合わせ元を MateOracle で回す処理の実装

#include "matequeryloop.h"

MateQueryLoop::MateQueryLoop(MateOracle* oracle, QObject* parent)
    : QObject(parent)
    , m_oracle(oracle)
{
    Q_ASSERT(m_oracle);
    m_stepTimer.setSingleShot(true);
    connect(&m_stepTimer, &QTimer::timeout, this, &MateQueryLoop::step);
    connect(m_oracle, &MateOracle::answered, this, &MateQueryLoop::onAnswered);
}

void MateQueryLoop::run(MateQuerySource* source)
{
    m_source = source;
    m_awaiting = false;
    m_stepTimer.start(0);
}

void MateQueryLoop::cancel()
{
    m_source = nullptr;
    m_stepTimer.stop();
    if (m_awaiting) {
        m_awaiting = false;
        m_oracle->stop();
    }
}

void MateQueryLoop::step()
{
    if (!m_source || m_awaiting) return;
    const std::optional<MateQuery> query = m_source->nextQuery();
    if (m_source->isDone()) {
        m_source = nullptr;
        emit finished();
        return;
    }
    if (!query) {
        m_stepTimer.start(0);
        return;
    }
    m_awaiting = true;
    m_queryTimer.start();
    const std::optional<UsiPosition> position = UsiPosition::parse(query->sfen);
    const bool asked = position && m_oracle->ask(*position, query->limitMs);
    emit queryStarted(*query);
    // 読めない局面はエンジンに聞かず、判定できなかったとする
    if (!asked) onAnswered({MateAnswer::Kind::Unknown, {}});
}

void MateQueryLoop::onAnswered(const MateAnswer& answer)
{
    if (!m_source || !m_awaiting) return;
    m_awaiting = false;
    if (answer.kind == MateAnswer::Kind::Unsupported) {
        m_source = nullptr;
        emit unsupported();
        return;
    }
    m_source->submit(answer, m_queryTimer.elapsed());
    m_stepTimer.start(0);
}
