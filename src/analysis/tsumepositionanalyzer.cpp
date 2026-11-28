#include "tsumepositionanalyzer.h"
#include "tsumecollection.h"
#include "tsumemateengine.h"
#include "tsumeprogressstore.h"
#include "tsumesolutionsearch.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include "version.h"  // Hayanagi の HAYANAGI_VERSION

TsumePositionAnalyzer::TsumePositionAnalyzer(QObject* parent) : QObject(parent)
{
    m_engine = new TsumeMateEngine(this);
    connect(m_engine, &TsumeMateEngine::finished, this, &TsumePositionAnalyzer::engineFinished);
    m_search = new TsumeSolutionSearch(this);
    connect(m_search, &TsumeSolutionSearch::finished, this, &TsumePositionAnalyzer::internalFinished);
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
    // 内蔵判定の結果は Hayanagi の版と手数上限ごとに保存する。サブモジュールを更新すると版が変わり、
    // 以前の判定結果は使わない。ShogiBoardQ 側の判定方式を変えたときは末尾の版（v3）を上げる
    // （v3: 反復深化の探索と証明数探索を同時に動かす）。
    if (enginePath.isEmpty()) {
        m_engineKey = QStringLiteral("hayanagi-%1-depth%2-v3")
                          .arg(QLatin1String(HAYANAGI_VERSION))
                          .arg(kBuiltInMaxPlies);
    } else {
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
    m_search->cancel();
}

void TsumePositionAnalyzer::evaluate(const QString& sfen, int milliseconds, bool requireLine)
{
    // 呼出元は直前の問い合わせ完了後に次を送る。アイドルなエンジンは再利用する。
    if (m_active) cancel();
    m_sfen = sfen;
    m_id = TsumeCollection::positionId(sfen);
    m_active = true;
    if (m_store) {
        const auto result = m_store->validatedCached(m_id, m_engineKey, requireLine || !m_enginePath.isEmpty());
        if (result) {
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
    m_search->start(position, kBuiltInMaxPlies, milliseconds, requireLine);
}

void TsumePositionAnalyzer::internalFinished(const TsumeEvaluation& evaluation)
{
    if (!m_active) return;
    TsumeEvaluation result = evaluation;
    if (result.status == TsumeEvaluation::Status::Unknown)
        result.detail = tr("内蔵判定の時間または%1手の上限に達しました。KomoringHeightsで再判定できます。").arg(kBuiltInMaxPlies);
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
