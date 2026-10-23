/// @file kifuloadcoordinator.cpp
/// @brief 棋譜を読み、分岐ツリーを作り直す

#include "kifuloadcoordinator.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "logcategories.h"
#include "shogiposition.h"

#include <QtConcurrentRun>
#include <utility>

KifuLoadCoordinator::KifuLoadCoordinator(KifuDocument* document, QObject* parent)
    : QObject(parent)
    , m_document(document)
{
    Q_ASSERT(m_document);
}

KifuLoadCoordinator::~KifuLoadCoordinator()
{
    if (m_loadCancel) m_loadCancel->store(true);
}

// ============================================================
// 非同期の読み込み
// ============================================================

void KifuLoadCoordinator::cancelLoad()
{
    if (!m_loadWatcher) return;
    m_loadCancel->store(true);
    if (auto* watcher = std::exchange(m_loadWatcher, nullptr)) {
        disconnect(watcher, nullptr, this, nullptr);
        watcher->deleteLater();
    }
    emit loadFinished(false);
}

void KifuLoadCoordinator::startLoad(const QString& input, bool text)
{
    // 以前のジョブは値だけを所有するため、完了を待たずに破棄できる。
    if (m_loadCancel) m_loadCancel->store(true);
    if (auto* watcher = std::exchange(m_loadWatcher, nullptr)) {
        disconnect(watcher, nullptr, this, nullptr);
        watcher->deleteLater();
    }
    m_loadCancel = makeCancelFlag();
    const auto cancel = m_loadCancel;
    m_loadWatcher = new QFutureWatcher<KifuLoadResult>(this);
    connect(m_loadWatcher, &QFutureWatcher<KifuLoadResult>::finished,
            this, &KifuLoadCoordinator::onLoadFinished);
    m_loadWatcher->setFuture(QtConcurrent::run([input, text, cancel]() {
        return text ? KifuLoadParser::parseText(input, cancel)
                    : KifuLoadParser::parseFile(input, std::nullopt, cancel);
    }));
}

void KifuLoadCoordinator::loadFileAsync(const QString& filePath) { startLoad(filePath, false); }
void KifuLoadCoordinator::loadTextAsync(const QString& content) { startLoad(content, true); }

void KifuLoadCoordinator::onLoadFinished()
{
    if (sender() != m_loadWatcher) return;
    auto* watcher = std::exchange(m_loadWatcher, nullptr);
    const auto result = watcher->result();
    watcher->deleteLater();
    const bool success = !m_loadCancel->load() && applyLoadResult(result);
    emit loadFinished(success);
}

// ============================================================
// 同期の読み込み
// ============================================================

bool KifuLoadCoordinator::loadFile(const QString& filePath)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(filePath));
}

bool KifuLoadCoordinator::loadText(const QString& content)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseText(content));
}

bool KifuLoadCoordinator::loadPositionFromSfen(const QString& sfenStr)
{
    cancelLoad();
    QString sfen = sfenStr.trimmed();
    if (sfen.startsWith(QStringLiteral("sfen "), Qt::CaseInsensitive)) sfen = sfen.mid(5).trimmed();
    if (sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() < 4) {
        emit errorOccurred(tr("無効なSFEN形式です。"));
        return false;
    }
    // 局面だけの棋譜にする（表示中の手は開始局面へ戻る）
    m_document->replaceWithPosition(sfen);
    return true;
}

// ============================================================
// 読み込み結果をツリーに載せる
// ============================================================

bool KifuLoadCoordinator::applyLoadResult(const KifuLoadResult& result)
{
    if (!result.success) {
        if (!result.error.isEmpty()) emit errorOccurred(result.error);
        return false;
    }
    if (result.positionOnly) return loadPositionFromSfen(result.startSfen());
    if (!result.warning.isEmpty()) emit errorOccurred(tr("棋譜の読み込みで警告があります:\n%1").arg(result.warning));
    return applyRecord(result);
}

bool KifuLoadCoordinator::applyRecord(const KifuLoadResult& result)
{
    const KifParseResult& res = result.record;
    // 指し手が0手でも、平手以外の開始局面が書かれていれば棋譜として読む（局面編集から保存した棋譜・
    // 駒落ちの開始局面など。形式によらない）。平手の開始局面だけの棋譜は開くものが無いので読まない
    const bool hasMoves = !res.mainline.usiMoves().isEmpty() || res.mainline.endsWithTerminal();
    const bool namedHirate = result.startKind == StartKind::Default
        || (result.startKind == StartKind::Handicap && Position::fromSfen(result.startSfen()) == Position::hirate());
    if (namedHirate && !hasMoves) {
        emit errorOccurred(tr("読み込み失敗 %1 から指し手を取得できませんでした。").arg(result.filePath));
        return false;
    }

    // 文書を読んだ棋譜で置き換える（ツリーの変更の通知は作り終えてから1回）。表示中の手は開始局面へ戻る
    if (!m_document->replaceWithRecord(res, result.gameInfo)) {
        emit errorOccurred(tr("棋譜の分岐元または開始手数が不正です。"));
        return false;
    }
    qCDebug(lcKifu).noquote() << "kifu loaded:" << result.filePath << "start=" << static_cast<int>(result.startKind)
                              << "nodes=" << m_document->tree()->nodeCount() << "lines=" << m_document->tree()->lineCount();
    return true;
}
