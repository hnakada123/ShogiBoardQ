/// @file kifuloadcoordinator.cpp
/// @brief 棋譜読み込みコーディネータクラスの実装

#include "kifuloadcoordinator.h"
#include "kifuapplyservice.h"
#include "kifuapplylogger.h"
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

#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QFile>
#include <QTextStream>
#include <QTableWidget>
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QtConcurrentRun>
#include <utility>

namespace {
const QColor kBranchHighlightColor(255, 220, 160);
} // namespace

KifuLoadCoordinator::KifuLoadCoordinator(QList<ShogiMove>& gameMoves,
                                         QStringList& positionStrList,
                                         int& activePly,
                                         int& currentSelectedPly,
                                         int& currentMoveIndex,
                                         QStringList* sfenRecord,
                                         QTableWidget* gameInfoTable,
                                         QDockWidget* gameInfoDock,
                                         QTabWidget* tab,
                                         RecordPane* recordPane,
                                         KifuRecordListModel* kifuRecordModel,
                                         KifuBranchListModel* kifuBranchModel,
                                         QObject* parent)
    : QObject(parent)
    , m_gameInfoTable(gameInfoTable)
    , m_gameInfoDock(gameInfoDock)
    , m_tab(tab)
    , m_sfenHistory(sfenRecord)
    , m_gameMoves(gameMoves)
    , m_positionStrList(positionStrList)
    , m_recordPane(recordPane)
    , m_activePly(activePly)
    , m_currentSelectedPly(currentSelectedPly)
    , m_currentMoveIndex(currentMoveIndex)
    , m_kifuRecordModel(kifuRecordModel)
    , m_kifuBranchModel(kifuBranchModel)
{
    initApplyService();
}

void KifuLoadCoordinator::initApplyService()
{
    m_applyService = new KifuApplyService(this);

    KifuApplyService::Refs refs;
    refs.kifuUsiMoves = &m_kifuUsiMoves;
    refs.gameMoves = &m_gameMoves;
    refs.positionStrList = &m_positionStrList;
    refs.dispMain = &m_dispMain;
    refs.sfenMain = &m_sfenMain;
    refs.gmMain = &m_gmMain;
    refs.variationsByPly = &m_variationsByPly;
    refs.variationsSeq = &m_variationsSeq;
    refs.activePly = &m_activePly;
    refs.currentSelectedPly = &m_currentSelectedPly;
    refs.currentMoveIndex = &m_currentMoveIndex;
    refs.loadingKifu = &m_loadingKifu;
    refs.gameInfoTable = m_gameInfoTable;
    refs.tab = m_tab;
    refs.recordPane = m_recordPane;
    refs.kifuRecordModel = m_kifuRecordModel;
    refs.kifuBranchModel = m_kifuBranchModel;
    refs.sfenHistory = &m_sfenHistory;
    refs.gameInfoDock = &m_gameInfoDock;
    refs.shogiView = &m_shogiView;
    refs.branchTreeManager = &m_branchTreeManager;
    refs.branchTree = &m_branchTree;
    refs.navState = &m_navState;
    refs.treeParent = this;
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
    if (m_branchRowDelegate) m_branchRowDelegate->setMarkers(nullptr);
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
                                            result.record, result.warning, "loadKifu", &result);
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
// 分岐マーカー描画
// ============================================================

void KifuLoadCoordinator::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);

    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QList<BranchLine> lines = m_branchTree->allLines();
        const int nLines = static_cast<int>(lines.size());
        const int currentLineIdx = (m_navState != nullptr) ? m_navState->currentLineIndex() : 0;
        const int active = (nLines == 0) ? 0 : qBound(0, currentLineIdx, nLines - 1);

        if (active >= 0 && active < nLines) {
            const BranchLine& line = lines.at(active);
            m_branchablePlySet = m_branchTree->branchablePlysOnLine(line);
        }
    }

    ensureBranchRowDelegateInstalled();

    if (view && view->viewport()) view->viewport()->update();
}

void KifuLoadCoordinator::ensureBranchRowDelegateInstalled()
{
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view) return;

    if (!m_branchRowDelegate) {
        m_branchRowDelegate = new BranchRowDelegate(view);
        view->setItemDelegate(m_branchRowDelegate);
    } else {
        if (m_branchRowDelegate->parent() != view) {
            m_branchRowDelegate->setParent(view);
            view->setItemDelegate(m_branchRowDelegate);
        }
    }

    m_branchRowDelegate->setMarkers(&m_branchablePlySet);
}

KifuLoadCoordinator::BranchRowDelegate::BranchRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

KifuLoadCoordinator::BranchRowDelegate::~BranchRowDelegate() = default;

void KifuLoadCoordinator::BranchRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    QStyledItemDelegate::initStyleOption(&opt, index);

    const bool isBranchable = (m_marks && m_marks->contains(index.row()));

    if (isBranchable && !(opt.state & QStyle::State_Selected)) {
        painter->save();
        painter->fillRect(opt.rect, kBranchHighlightColor);
        painter->restore();
    }

    QStyledItemDelegate::paint(painter, opt, index);
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

void KifuLoadCoordinator::resetBranchContext()
{
    m_branchPlyContext = -1;
}

void KifuLoadCoordinator::resetBranchTreeForNewGame()
{
    cancelLoad();
    qCDebug(lcKifu).noquote() << "resetBranchTreeForNewGame: clearing all branch data";

    m_branchPlyContext = -1;

    if (m_navState != nullptr) {
        m_navState->setCurrentNode(nullptr);
        m_navState->resetPreferredLineIndex();
    }

    if (m_branchTree == nullptr) {
        m_branchTree = new KifuBranchTree(this);
    } else {
        m_branchTree->clear();
    }
    m_branchTree->setRootSfen(SfenUtils::hirateSfen());

    if (m_navState != nullptr) {
        m_navState->goToRoot();
    }

    m_branchablePlySet.clear();

    if (m_branchTreeManager) {
        QList<BranchTreeManager::ResolvedRowLite> emptyRows;
        m_branchTreeManager->setBranchTreeRows(emptyRows);
    }

    qCDebug(lcKifu).noquote() << "resetBranchTreeForNewGame: done";
}
