/// @file tsumemateengine.cpp
/// @brief 最短手数を確認できる詰み探索エンジンで詰みを判定するクラスの実装

#include "tsumemateengine.h"
#include "enginelistsettings.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"
#include <algorithm>

namespace {
/// 起動から readyok まで。大きなハッシュの確保に時間がかかるエンジンがある
constexpr int kInitializationTimeoutMs = 15000;
} // namespace

TsumeMateEngine::TsumeMateEngine(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
{
    m_session->setInitializationTimeoutMs(kInitializationTimeoutMs);
    m_deadline.setSingleShot(true);
    m_deadline.setTimerType(Qt::PreciseTimer);
    connect(&m_deadline, &QTimer::timeout, this, &TsumeMateEngine::timedOut);
    connect(m_session, &UsiEngineSession::ready, this, &TsumeMateEngine::onReady);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &TsumeMateEngine::onSessionError);
    UsiProtocolHandler* handler = m_session->handler();
    connect(handler, &UsiProtocolHandler::checkmateSolved, this, &TsumeMateEngine::onCheckmateSolved);
    connect(handler, &UsiProtocolHandler::checkmateNoMate, this, &TsumeMateEngine::onCheckmateNoMate);
    connect(handler, &UsiProtocolHandler::checkmateNotImplemented, this, &TsumeMateEngine::onCheckmateUndetermined);
    connect(handler, &UsiProtocolHandler::checkmateUnknown, this, &TsumeMateEngine::onCheckmateUndetermined);
}

TsumeMateEngine::~TsumeMateEngine() { cancel(); }

QList<QPair<QString, QString>> TsumeMateEngine::requiredOptions()
{
    return {{QStringLiteral("PostSearchLevel"), QStringLiteral("MinLength")},
            {QStringLiteral("GenerateAllLegalMoves"), QStringLiteral("true")},
            {QStringLiteral("RootIsAndNodeIfChecked"), QStringLiteral("false")},
            {QStringLiteral("NodesLimit"), QStringLiteral("0")},
            {QStringLiteral("MultiPV"), QStringLiteral("1")}};
}

void TsumeMateEngine::setExecutable(const QString& path)
{
    if (path == m_executable) return;
    cancel();
    m_executable = path;
}

void TsumeMateEngine::cancel()
{
    m_deadline.stop();
    m_phase = Phase::Off;
    m_session->quit();
}

void TsumeMateEngine::search(const QString& sfen, int milliseconds)
{
    m_sfen = sfen;
    m_milliseconds = std::clamp(milliseconds, 1, 600000);
    if (m_phase == Phase::Idle) {
        beginSearch();
        return;
    }
    cancel();
    m_phase = Phase::Starting;
    // 登録済みなら保存済みの設定を送る（名前で設定を引く）。最短手数の確認に必要な値だけ上書きする
    m_session->handler()->setSessionOptions(requiredOptions());
    QString error;
    if (!m_session->startAsync(m_executable, EngineListSettings::nameForPath(m_executable), &error)) {
        fail(tr("判定エンジンを実行できません: %1").arg(error.isEmpty() ? m_executable : error));
    }
}

void TsumeMateEngine::onReady()
{
    if (m_phase != Phase::Starting) return;
    const UsiProtocolHandler* handler = m_session->handler();
    const bool supported =
        handler->reportedEngineName().contains(QStringLiteral("KomoringHeights"), Qt::CaseInsensitive)
        && handler->reportedOptionLine(QStringLiteral("PostSearchLevel")).contains(QStringLiteral("var MinLength"))
        && !handler->reportedOptionLine(QStringLiteral("GenerateAllLegalMoves")).isEmpty()
        && !handler->reportedOptionLine(QStringLiteral("RootIsAndNodeIfChecked")).isEmpty();
    if (!supported) {
        fail(tr("最短手数を確認できるKomoringHeightsを選択してください。"));
        return;
    }
    m_phase = Phase::Idle;
    beginSearch();
}

void TsumeMateEngine::beginSearch()
{
    m_phase = Phase::Searching;
    UsiProtocolHandler* handler = m_session->handler();
    handler->sendUsiNewGame();
    handler->sendPosition(QStringLiteral("position sfen ") + m_sfen);
    // Komoring 1.1.0 は時間制限で最短化を打ち切っても既発見の PV を返す場合がある。
    // infinite + GUI 側の打切りで、自然終了した MinLength の結果だけを確定扱いする。
    handler->sendGoMate(0, true);
    m_deadline.start(m_milliseconds);
}

void TsumeMateEngine::finishSearch(const TsumeEvaluation& result)
{
    m_deadline.stop();
    m_phase = Phase::Idle;
    emit finished(result);
}

void TsumeMateEngine::onCheckmateSolved(const QStringList& pv)
{
    if (m_phase != Phase::Searching) return;
    TsumeEvaluation result;
    result.status = TsumeEvaluation::Status::Mate;
    result.pv = pv;
    result.plies = static_cast<int>(pv.size());
    finishSearch(result);
}

void TsumeMateEngine::onCheckmateNoMate()
{
    if (m_phase != Phase::Searching) return;
    TsumeEvaluation result;
    result.status = TsumeEvaluation::Status::NoMate;
    finishSearch(result);
}

void TsumeMateEngine::onCheckmateUndetermined()
{
    if (m_phase != Phase::Searching) return;
    TsumeEvaluation result;
    result.detail = tr("エンジンが判定を完了できませんでした。時間を増やして再判定できます。");
    finishSearch(result);
}

void TsumeMateEngine::fail(const QString& message)
{
    cancel();
    emit finished({TsumeEvaluation::Status::Unknown, 0, {}, message});
}

void TsumeMateEngine::onSessionError(const QString&)
{
    if (m_phase == Phase::Off) return;
    fail(tr("判定エンジンが終了しました。実行ファイルを確認してください。"));
}

void TsumeMateEngine::timedOut()
{
    fail(tr("制限時間内に判定が完了しませんでした。不詰とは判定していません。"));
}
