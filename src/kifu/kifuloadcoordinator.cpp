/// @file kifuloadcoordinator.cpp
/// @brief 棋譜読み込みコーディネータクラスの実装

#include "kifuloadcoordinator.h"
#include "kifuapplyservice.h"
#include "kifufilereader.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "csatosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "sfenutils.h"

#include "logcategories.h"

#include <QAbstractItemView>
#include <QFile>
#include <QTextStream>
#include <QTableWidget>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QtConcurrentRun>
#include <utility>

KifuLoadCoordinator::KifuLoadCoordinator(QTableWidget* gameInfoTable,
                                         RecordPane* recordPane,
                                         KifuRecordListModel* kifuRecordModel,
                                         KifuBranchListModel* kifuBranchModel,
                                         QObject* parent)
    : QObject(parent)
    , m_gameInfoTable(gameInfoTable)
    , m_recordPane(recordPane)
    , m_kifuRecordModel(kifuRecordModel)
    , m_kifuBranchModel(kifuBranchModel)
{
    initApplyService();
}

void KifuLoadCoordinator::initApplyService()
{
    m_applyService = new KifuApplyService(this);

    KifuApplyService::Refs refs;
    refs.loadingKifu = &m_loadingKifu;
    refs.gameInfoTable = m_gameInfoTable;
    refs.recordPane = m_recordPane;
    refs.kifuRecordModel = m_kifuRecordModel;
    refs.kifuBranchModel = m_kifuBranchModel;
    refs.shogiView = [this]() { return m_shogiView; };
    refs.branchTreeManager = [this]() { return m_branchTreeManager; };
    refs.branchTree = [this]() { return m_branchTree; };
    refs.navState = [this]() { return m_navState; };
    m_applyService->setRefs(refs);

    KifuApplyService::Hooks hooks;
    hooks.displayGameRecord = [this](const QList<KifDisplayItem>& d) { emit displayGameRecord(d); };
    hooks.syncBoardAndHighlightsAtRow = [this](int p) { emit syncBoardAndHighlightsAtRow(p); };
    hooks.enableArrowButtons = [this]() { emit enableArrowButtons(); };
    hooks.gameInfoPopulated = [this](const QList<KifGameInfoItem>& i) { emit gameInfoPopulated(i); };
    hooks.branchTreeBuilt = [this]() { emit branchTreeBuilt(); };
    hooks.errorOccurred = [this](const QString& e) { emit errorOccurred(e); };
    m_applyService->setHooks(hooks);
}

// ============================================================
// 棋譜読み込み共通処理
// ============================================================

KifuLoadCoordinator::~KifuLoadCoordinator()
{
    if (m_loadCancel) m_loadCancel->store(true);
}

void KifuLoadCoordinator::cancelLoad()
{
    if (!m_loadWatcher) return;
    m_loadCancel->store(true);
    if (auto* watcher = std::exchange(m_loadWatcher, nullptr)) {
        disconnect(watcher, nullptr, this, nullptr);
        watcher->deleteLater();
    }
    m_loadingKifu = false;
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
    m_loadingKifu = true;
    m_loadWatcher->setFuture(QtConcurrent::run([input, text, cancel]() {
        return text ? KifuLoadParser::parseText(input, cancel)
                    : KifuLoadParser::parseFile(input, KifuFileReader::KifuFormat::Unknown, cancel);
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
    m_loadingKifu = false;
    emit loadFinished(success);
}

bool KifuLoadCoordinator::applyLoadResult(const KifuLoadResult& result)
{
    if (!result.success) {
        m_loadingKifu = false;
        if (!result.error.isEmpty()) emit errorOccurred(result.error);
        return false;
    }
    if (!result.positionOnly.isEmpty()) return m_applyService->loadPositionFromSfen(result.positionOnly);
    if (!result.warning.isEmpty()) emit errorOccurred(tr("棋譜の読み込みで警告があります:\n%1").arg(result.warning));
    m_loadingKifu = true;
    m_applyService->populateGameInfo(result.gameInfo);
    m_applyService->applyPlayersFromGameInfo(result.gameInfo);
    return m_applyService->applyParsedResult(result.filePath, result.initialSfen, result.teaiLabel,
                                            result.record, result.warning, "loadKifu");
}

bool KifuLoadCoordinator::loadKifuFromFile(const QString& path)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(path, KifuFileReader::KifuFormat::KIF));
}

bool KifuLoadCoordinator::loadKi2FromFile(const QString& path)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(path, KifuFileReader::KifuFormat::KI2));
}

bool KifuLoadCoordinator::loadCsaFromFile(const QString& path)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(path, KifuFileReader::KifuFormat::CSA));
}

bool KifuLoadCoordinator::loadJkfFromFile(const QString& path)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(path, KifuFileReader::KifuFormat::JKF));
}

bool KifuLoadCoordinator::loadUsenFromFile(const QString& path)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(path, KifuFileReader::KifuFormat::USEN));
}

bool KifuLoadCoordinator::loadUsiFromFile(const QString& path)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseFile(path, KifuFileReader::KifuFormat::USI));
}

bool KifuLoadCoordinator::loadKifuFromString(const QString& content)
{
    cancelLoad();
    return applyLoadResult(KifuLoadParser::parseText(content));
}

// ============================================================
// SFEN/BOD 局面読み込み（KifuApplyService に委譲）
// ============================================================

bool KifuLoadCoordinator::loadPositionFromSfen(const QString& sfenStr)
{
    cancelLoad();
    return m_applyService->loadPositionFromSfen(sfenStr);
}

bool KifuLoadCoordinator::loadPositionFromBod(const QString& bodStr)
{
    cancelLoad();
    return m_applyService->loadPositionFromBod(bodStr);
}

// ============================================================
// 外部オブジェクト設定
// ============================================================

void KifuLoadCoordinator::setBranchTreeManager(BranchTreeManager* manager)
{
    m_branchTreeManager = manager;
}

// ============================================================
// 分岐管理
// ============================================================

