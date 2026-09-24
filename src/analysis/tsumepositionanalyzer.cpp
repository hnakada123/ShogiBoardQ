#include "tsumepositionanalyzer.h"
#include "tsumecollection.h"
#include "tsumemateengine.h"
#include "tsumeprogressstore.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QtConcurrentRun>

namespace {
TsumeEvaluation evaluateInternal(shogi::Position position, int milliseconds, bool requireLine,
                                const std::atomic_bool& stop)
{
    QElapsedTimer elapsed;
    elapsed.start();
    shogi::TsumeSearch solver;
    const auto attacker = position.side_to_move();
    auto search = solver.solve(position, attacker, 31, milliseconds, stop);
    if (search.status == shogi::TsumeStatus::NoMate) return {TsumeEvaluation::Status::NoMate, 0, {}, {}};
    if (search.status != shogi::TsumeStatus::Mate) return {};
    TsumeEvaluation result{TsumeEvaluation::Status::Mate, search.plies, {}, {}};
    if (!requireLine) return result;
    // 内蔵コアは最善手1手を返すため、攻方の最短手・玉方の最長応手を順に取得する。
    // 各局面で同じ時間を使い直さず、手順全体で制限時間と中止フラグを共有する。
    for (int remaining = result.plies; remaining > 0; --remaining) {
        if (stop.load() || search.status != shogi::TsumeStatus::Mate
            || search.plies != remaining || !search.move.is_valid()) return {};
        result.pv.append(QString::fromStdString(position.move_to_usi(search.move)));
        position.do_move(search.move);
        if (remaining > 1) {
            const qint64 timeLeft = milliseconds - elapsed.elapsed();
            if (timeLeft <= 0) return {};
            search = solver.solve(position, attacker, remaining - 1, static_cast<int>(timeLeft), stop);
        }
    }
    return result;
}
}

TsumePositionAnalyzer::TsumePositionAnalyzer(QObject* parent) : QObject(parent)
{
    m_engine = new TsumeMateEngine(this);
    connect(m_engine, &TsumeMateEngine::finished, this, &TsumePositionAnalyzer::engineFinished);
    m_cacheTimer.setSingleShot(true);
    connect(&m_cacheTimer, &QTimer::timeout, this, &TsumePositionAnalyzer::deliverCached);
    configure({}, nullptr);
}

TsumePositionAnalyzer::~TsumePositionAnalyzer() { cancel(); }

void TsumePositionAnalyzer::configure(const QString& enginePath, TsumeProgressStore* store)
{
    cancel();
    m_enginePath = enginePath;
    m_store = store;
    m_engine->setExecutable(enginePath);
    // 内蔵コアを更新するときは、その参照コミットと判定方式の版も更新する。
    if (enginePath.isEmpty()) m_engineKey = QStringLiteral("hayanagi-30cfce58-depth31-v1");
    else {
        QFile file(enginePath);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (file.open(QIODevice::ReadOnly)) hash.addData(&file);
        m_engineKey = QStringLiteral("komoring-minlength-infinite-alllegal-rootor-v1:")
                    + QString::fromLatin1(hash.result().toHex()) + QLatin1Char(':') + QFileInfo(file).absoluteFilePath();
    }
}

void TsumePositionAnalyzer::cancel()
{
    m_active = false;
    m_cacheTimer.stop();
    m_engine->cancel();
    if (!m_watcher) return;
    disconnect(m_watcher.get(), nullptr, this, nullptr);
    m_stop->store(true);
    m_watcher->waitForFinished();
    m_watcher.reset();
}

void TsumePositionAnalyzer::evaluate(const QString& sfen, int milliseconds, bool requireLine)
{
    // 呼出元は直前の問い合わせ完了後に次を送る。アイドルなエンジンは再利用する。
    if (m_active) cancel();
    m_sfen = sfen;
    m_id = TsumeCollection::positionId(sfen);
    m_active = true;
    if (m_store) {
        const auto result = m_store->cached(m_id, m_engineKey);
        if (result && ((!requireLine && m_enginePath.isEmpty()) || result->status != TsumeEvaluation::Status::Mate
                       || (result->pv.size() == result->plies && TsumeCollection::validMateLine(sfen, result->pv)))) {
            m_cached = *result;
            m_cacheTimer.start(0);
            return;
        }
    }
    if (!m_enginePath.isEmpty()) { m_engine->search(sfen, milliseconds); return; }
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) {
        m_cached = {};
        m_cacheTimer.start(0);
        return;
    }
    m_stop = std::make_shared<std::atomic_bool>(false);
    m_watcher = std::make_unique<QFutureWatcher<TsumeEvaluation>>(this);
    connect(m_watcher.get(), &QFutureWatcher<TsumeEvaluation>::finished, this, &TsumePositionAnalyzer::internalFinished);
    const auto stop = m_stop;
    m_watcher->setFuture(QtConcurrent::run([position, milliseconds, requireLine, stop]() {
        return evaluateInternal(position, milliseconds, requireLine, *stop);
    }));
}

void TsumePositionAnalyzer::internalFinished()
{
    if (sender() != m_watcher.get()) return;
    auto result = m_watcher->result();
    m_watcher.release()->deleteLater();
    if (result.status == TsumeEvaluation::Status::Unknown)
        result.detail = tr("内蔵判定の時間または31手の上限に達しました。KomoringHeightsで再判定できます。");
    complete(result);
}

void TsumePositionAnalyzer::engineFinished(const TsumeEvaluation& result)
{
    if (!m_active) return;
    if (result.status == TsumeEvaluation::Status::Mate && !TsumeCollection::validMateLine(m_sfen, result.pv)) {
        complete({TsumeEvaluation::Status::Unknown, 0, {}, tr("エンジンが返した手順を合法な詰み手順として確認できませんでした。")});
    } else complete(result);
}

void TsumePositionAnalyzer::deliverCached()
{
    if (!m_active) return;
    m_active = false;
    emit finished(m_cached);
}

void TsumePositionAnalyzer::complete(const TsumeEvaluation& result)
{
    if (!m_active) return;
    if (m_store) m_store->cache(m_id, m_engineKey, result);
    m_active = false;
    emit finished(result);
}
