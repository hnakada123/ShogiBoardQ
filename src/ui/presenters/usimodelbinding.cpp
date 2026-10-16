/// @file usimodelbinding.cpp
/// @brief エンジン1台の思考情報・通信ログを表示のモデルへ反映する処理の実装

#include "usimodelbinding.h"

#include "engineprocessmanager.h"
#include "kifupresentation.h"
#include "sfenreplay.h"
#include "shogienginethinkingmodel.h"
#include "thinkinginfoformatter.h"
#include "usi.h"
#include "usicommlogmodel.h"

namespace {
/// 思考の表に残す行数
constexpr int kMaxThinkingRows = 500;
} // namespace

UsiModelBinding::UsiModelBinding(Usi* engine, const Models& models)
    : QObject(engine)
    , m_engineTag(engine->engineTag())
    , m_commLog(models.commLog)
    , m_thinking(models.thinking)
{
    const ThinkingInfoFormatter* thinking = engine->thinking();
    connect(thinking, &ThinkingInfoFormatter::thinkingInfoUpdated, this, &UsiModelBinding::onThinkingRow);
    connect(thinking, &ThinkingInfoFormatter::clearThinkingInfoRequested, this, &UsiModelBinding::onThinkingCleared);
    connect(thinking, &ThinkingInfoFormatter::searchStatusUpdated, this, &UsiModelBinding::onSearchStatus);
    const EngineProcessManager* process = engine->process();
    connect(process, &EngineProcessManager::commandSent, this, &UsiModelBinding::onCommandSent);
    connect(process, &EngineProcessManager::dataReceived, this, &UsiModelBinding::onLineReceived);
    connect(process, &EngineProcessManager::stderrReceived, this, &UsiModelBinding::onStderrReceived);
    connect(engine->protocol(), &UsiProtocolHandler::bestMoveReceived, this, &UsiModelBinding::onBestMove);
    if (m_commLog) m_commLog->setEngineName(engine->engineName());
}

void UsiModelBinding::setConsideration(ShogiEngineThinkingModel* model, int maxMultiPV)
{
    m_consideration = model;
    m_considerationMaxMultiPV = qBound(1, maxMultiPV, 10);
}

void UsiModelBinding::onThinkingRow(const ThinkingInfoRow& row)
{
    // 読み筋のない行（詰み探索の途中や、停止時の time・nodes だけの行）は表に出さない。
    // 検討の表では multipv 1 扱いになり、最善手の行を空の読み筋で上書きしてしまう
    if (row.pvKanji.isEmpty()) return;
    if (m_thinking) m_thinking->prependRow(row, kMaxThinkingRows);
    if (m_consideration) m_consideration->updateByMultipv(row, m_considerationMaxMultiPV);
}

void UsiModelBinding::onThinkingCleared()
{
    if (m_thinking) m_thinking->clear();
}

void UsiModelBinding::onSearchStatus(const EngineSearchStatus& status)
{
    if (m_commLog) m_commLog->updateStatus(status);
}

void UsiModelBinding::appendLog(const QString& mark, const QString& text)
{
    if (m_commLog) m_commLog->appendLogLine(mark + QLatin1Char(' ') + m_engineTag + QStringLiteral(": ") + text);
}

void UsiModelBinding::onCommandSent(const QString& command)
{
    appendLog(QStringLiteral("▶"), command);
}

void UsiModelBinding::onLineReceived(const QString& line)
{
    appendLog(QStringLiteral("◀"), line);
}

void UsiModelBinding::onStderrReceived(const QString& line)
{
    appendLog(QStringLiteral("⚠"), line);
}

void UsiModelBinding::onBestMove(const UsiBestMove& result)
{
    if (!m_commLog || result.pondering) return;
    // 予想手（相手の手の予想）を、指した後の局面の表記で出す
    QString prediction;
    if (result.isMove() && !result.ponder.isEmpty()) {
        const QString afterBest = SfenReplay::positionAfter(result.baseSfen, result.move);
        if (!afterBest.isEmpty())
            prediction = KifuPresentation::move(afterBest, result.ponder, KifuPresentation::options());
    }
    m_commLog->setPredictedMove(prediction);
}
