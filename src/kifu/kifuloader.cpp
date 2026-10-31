/// @file kifuloader.cpp
/// @brief 棋譜を読み、分岐ツリーを作り直す

#include "kifuloader.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "logcategories.h"
#include "shogiposition.h"
#include "usiposition.h"

#include <QtConcurrentRun>
#include <utility>

KifuLoader::KifuLoader(KifuDocument* document, QObject* parent)
    : QObject(parent)
    , m_document(document)
{
    Q_ASSERT(m_document);
}

KifuLoader::~KifuLoader()
{
    if (m_loadCancel) m_loadCancel->store(true);
}

// ============================================================
// 非同期の読み込み
// ============================================================

void KifuLoader::cancelLoad()
{
    if (!m_loadWatcher) return;
    m_loadCancel->store(true);
    if (auto* watcher = std::exchange(m_loadWatcher, nullptr)) {
        disconnect(watcher, nullptr, this, nullptr);
        watcher->deleteLater();
    }
    emit loadFinished(false);
}

void KifuLoader::startLoad(const QString& input, bool text)
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
            this, &KifuLoader::onLoadFinished);
    m_loadWatcher->setFuture(QtConcurrent::run([input, text, cancel]() {
        return text ? KifuLoadParser::parseText(input, cancel)
                    : KifuLoadParser::parseFile(input, std::nullopt, cancel);
    }));
}

void KifuLoader::loadFileAsync(const QString& filePath) { startLoad(filePath, false); }
void KifuLoader::loadTextAsync(const QString& content) { startLoad(content, true); }

void KifuLoader::onLoadFinished()
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

bool KifuLoader::loadFile(const QString& filePath)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(filePath));
}

bool KifuLoader::loadText(const QString& content)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseText(content));
}

bool KifuLoader::loadPositionFromSfen(const QString& sfen)
{
    cancelLoad();
    // 読み方は position コマンドと同じ（UsiPosition::parse）。局面として読めなければ置き換えない
    const std::optional<UsiPosition> position = UsiPosition::parse(sfen);
    if (!position) {
        emit errorOccurred(tr("無効なSFEN形式です。"));
        return false;
    }
    // 局面だけの棋譜にする（表示中の手は開始局面へ戻る）
    m_document->replaceWithPosition(position->sfen());
    return true;
}

// ============================================================
// 読み込み結果をツリーに載せる
// ============================================================

bool KifuLoader::applyLoadResult(const KifuLoadResult& result)
{
    if (!result.success) {
        if (!result.error.isEmpty()) emit errorOccurred(result.error);
        return false;
    }
    if (result.positionOnly) return loadPositionFromSfen(result.startSfen());
    if (!result.warning.isEmpty()) emit warningOccurred(tr("棋譜の読み込みで警告があります:\n%1").arg(result.warning));
    return applyRecord(result);
}

bool KifuLoader::applyRecord(const KifuLoadResult& result)
{
    const KifParseResult& res = result.record;
    // 開始局面が明示された棋譜は0手でも開く。棋譜でも局面でもない入力は引き続き拒否する。
    const bool hasMoves = !res.mainline.usiMoves().isEmpty() || res.mainline.endsWithTerminal();
    if (result.startKind == StartKind::Default && !hasMoves
        && result.format != KifuFormat::Jkf) {
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
