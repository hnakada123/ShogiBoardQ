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

KifuLoadCoordinator::KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
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
// デバッグ出力ヘルパー関数（ファイルスコープ内でのみ使用）
// ============================================================

static void dumpMainline(const KifParseResult& res, const QString& parseWarn)
{
    qCDebug(lcKifu).noquote() << "KifParseResult dump:";
    if (!parseWarn.isEmpty()) {
        qCDebug(lcKifu).noquote() << "  [parseWarn]" << parseWarn;
    }

    qCDebug(lcKifu).noquote() << "  Mainline:";
    qCDebug(lcKifu).noquote() << "    baseSfen: " << res.mainline.baseSfen;
    qCDebug(lcKifu).noquote() << "    usiMoves: " << res.mainline.usiMoves;
    qCDebug(lcKifu).noquote() << "    disp:";
    int mainIdx = 0;
    for (const auto& d : std::as_const(res.mainline.disp)) {
        qCDebug(lcKifu).noquote() << "      [" << mainIdx << "] prettyMove: " << d.prettyMove;
        qCDebug(lcKifu).noquote() << "           comment: " << (d.comment.isEmpty() ? "<none>" : d.comment);
        qCDebug(lcKifu).noquote() << "           timeText: " << d.timeText;
        ++mainIdx;
    }
}

static void dumpVariationsDebug(const KifParseResult& res)
{
    qCDebug(lcKifu).noquote() << "  Variations:";
    int varNo = 0;
    for (const KifVariation& var : std::as_const(res.variations)) {
        qCDebug(lcKifu).noquote() << "  [Var " << varNo << "]";
        qCDebug(lcKifu).noquote() << "    startPly: " << var.startPly;
        qCDebug(lcKifu).noquote() << "    baseSfen: " << var.line.baseSfen;
        qCDebug(lcKifu).noquote() << "    usiMoves: " << var.line.usiMoves;
        qCDebug(lcKifu).noquote() << "    disp:";
        int dispIdx = 0;
        for (const auto& d : std::as_const(var.line.disp)) {
            qCDebug(lcKifu).noquote() << "      [" << dispIdx << "] prettyMove: " << d.prettyMove;
            qCDebug(lcKifu).noquote() << "           comment: " << (d.comment.isEmpty() ? "<none>" : d.comment);
            qCDebug(lcKifu).noquote() << "           timeText: " << d.timeText;
            ++dispIdx;
        }
        ++varNo;
    }
}

// ============================================================
// 棋譜読み込み共通処理
// ============================================================

void KifuLoadCoordinator::loadKifuCommon(
    const QString& filePath,
    const char* funcName,
    const KifuParseFunc& parseFunc,
    const KifuDetectSfenFunc& detectSfenFunc,
    const KifuExtractGameInfoFunc& extractGameInfoFunc,
    bool dumpVariations)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer stepTimer;
    auto logStep = [&](const char* stepName) {
        qCDebug(lcKifu).noquote() << QStringLiteral("%1: %2 ms").arg(stepName).arg(stepTimer.elapsed());
        stepTimer.restart();
    };
    stepTimer.start();

    qCDebug(lcKifu).noquote() << funcName << "IN file=" << filePath;

    m_loadingKifu = true;

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    QString initialSfen;
    if (detectSfenFunc) {
        initialSfen = detectSfenFunc(filePath, &teaiLabel);
        if (initialSfen.isEmpty()) {
            initialSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
            teaiLabel = QStringLiteral("平手(既定)");
        }
    } else {
        initialSfen = prepareInitialSfen(filePath, teaiLabel);
    }
    logStep("detectInitialSfen");

    // 2) 解析（本譜＋分岐＋コメント）を一括取得
    KifParseResult res;
    QString parseWarn;
    if (!parseFunc(filePath, res, &parseWarn)) {
        qCWarning(lcKifu).noquote() << "parse failed:" << filePath << parseWarn;
        QString detail = parseWarn.isEmpty() ? QString() : QStringLiteral("\n") + parseWarn;
        emit errorOccurred(tr("棋譜ファイルの読み込みに失敗しました: %1%2")
                               .arg(QFileInfo(filePath).fileName(), detail));
        m_loadingKifu = false;
        return;
    }
    if (!parseWarn.isEmpty()) {
        qCWarning(lcKifu).noquote() << "parse warn:" << parseWarn;
        emit errorOccurred(tr("棋譜の読み込みで警告があります:\n%1").arg(parseWarn));
    }
    logStep("parseFunc");

    // 2.5) sfenList が未生成の場合は baseSfen + usiMoves から補完
    if (res.mainline.sfenList.isEmpty() && !res.mainline.usiMoves.isEmpty()) {
        res.mainline.sfenList = SfenPositionTracer::buildSfenRecord(
            res.mainline.baseSfen, res.mainline.usiMoves, false);
    }
    for (KifVariation& var : res.variations) {
        if (!var.line.sfenList.isEmpty() || var.line.usiMoves.isEmpty()) {
            continue;
        }
        if (var.line.baseSfen.isEmpty() && !res.mainline.sfenList.isEmpty()) {
            const int branchPly = var.startPly - 1;
            if (branchPly >= 0 && branchPly < res.mainline.sfenList.size()) {
                var.line.baseSfen = res.mainline.sfenList.at(branchPly);
            }
        }
        if (!var.line.baseSfen.isEmpty()) {
            var.line.sfenList = SfenPositionTracer::buildSfenRecord(
                var.line.baseSfen, var.line.usiMoves, var.line.endsWithTerminal);
        }
    }

    // 3) デバッグ出力
    dumpMainline(res, parseWarn);
    if (dumpVariations) {
        dumpVariationsDebug(res);
    }
    logStep("dumpMainline/Variations");

    // 4) 先手/後手名などヘッダ反映
    if (extractGameInfoFunc) {
        const QList<KifGameInfoItem> infoItems = extractGameInfoFunc(filePath);
        m_applyService->populateGameInfo(infoItems);
        m_applyService->applyPlayersFromGameInfo(infoItems);
    }
    logStep("extractGameInfo");

    // 5) 共通の後処理（KifuApplyService に委譲）
    m_applyService->applyParsedResult(filePath, initialSfen, teaiLabel, res, parseWarn, funcName);
    qCDebug(lcKifu).noquote() << QStringLiteral("loadKifuCommon TOTAL: %1 ms").arg(totalTimer.elapsed());
}

// ============================================================
// 各フォーマット用の公開関数
// ============================================================

void KifuLoadCoordinator::loadKi2FromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadKi2FromFile",
        [](const QString& path, KifParseResult& res, QString* warn) {
            return Ki2ToSfenConverter::parseWithVariations(path, res, warn);
        },
        KifuDetectSfenFunc(),
        [](const QString& path) {
            return CsaToSfenConverter::extractGameInfo(path);
        },
        false
    );
}

void KifuLoadCoordinator::loadCsaFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadCsaFromFile",
        [](const QString& path, KifParseResult& res, QString* warn) {
            return CsaToSfenConverter::parse(path, res, warn);
        },
        KifuDetectSfenFunc(),
        [](const QString& path) {
            return CsaToSfenConverter::extractGameInfo(path);
        },
        false
    );
}

void KifuLoadCoordinator::loadJkfFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadJkfFromFile",
        [](const QString& path, KifParseResult& res, QString* warn) {
            return JkfToSfenConverter::parseWithVariations(path, res, warn);
        },
        [](const QString& path, QString* label) {
            return JkfToSfenConverter::detectInitialSfenFromFile(path, label);
        },
        [](const QString& path) {
            return JkfToSfenConverter::extractGameInfo(path);
        },
        true
    );
}

void KifuLoadCoordinator::loadKifuFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadKifuFromFile",
        [](const QString& path, KifParseResult& res, QString* warn) {
            return KifToSfenConverter::parseWithVariations(path, res, warn);
        },
        KifuDetectSfenFunc(),
        [](const QString& path) {
            return KifToSfenConverter::extractGameInfo(path);
        },
        true
    );
}

void KifuLoadCoordinator::loadUsenFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadUsenFromFile",
        [](const QString& path, KifParseResult& res, QString* warn) {
            return UsenToSfenConverter::parseWithVariations(path, res, warn);
        },
        [](const QString& path, QString* label) {
            return UsenToSfenConverter::detectInitialSfenFromFile(path, label);
        },
        [](const QString& path) {
            return UsenToSfenConverter::extractGameInfo(path);
        },
        true
    );
}

void KifuLoadCoordinator::loadUsiFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadUsiFromFile",
        [](const QString& path, KifParseResult& res, QString* warn) {
            return UsiToSfenConverter::parseWithVariations(path, res, warn);
        },
        [](const QString& path, QString* label) {
            return UsiToSfenConverter::detectInitialSfenFromFile(path, label);
        },
        KifuExtractGameInfoFunc(),
        false
    );
}

// ============================================================
// 文字列からの棋譜読み込み（フォーマット自動判定）
// ============================================================

bool KifuLoadCoordinator::loadKifuFromString(const QString& content)
{
    if (content.trimmed().isEmpty()) {
        emit errorOccurred(tr("貼り付けるテキストが空です。"));
        return false;
    }

    qCDebug(lcKifu).noquote() << "loadKifuFromString: content length =" << content.size();

    // I/O層でフォーマット判定
    const auto fmt = KifuFileReader::detectFormat(content);

    // SFEN/BOD は適用層で直接処理
    if (fmt == KifuFileReader::KifuFormat::SFEN) {
        return m_applyService->loadPositionFromSfen(content.trimmed());
    }
    if (fmt == KifuFileReader::KifuFormat::BOD) {
        return m_applyService->loadPositionFromBod(content);
    }

    // 一時ファイルを作成して読み込み
    const QString tempFilePath = KifuFileReader::tempFilePath(fmt);
    if (!KifuFileReader::writeTempFile(tempFilePath, content)) {
        emit errorOccurred(tr("一時ファイルの作成に失敗しました。"));
        return false;
    }
    qCDebug(lcKifu).noquote() << "created temp file:" << tempFilePath;

    // 形式に応じた読み込み関数を呼び出し
    switch (fmt) {
    case KifuFileReader::KifuFormat::KIF:  loadKifuFromFile(tempFilePath); break;
    case KifuFileReader::KifuFormat::KI2:  loadKi2FromFile(tempFilePath); break;
    case KifuFileReader::KifuFormat::CSA:  loadCsaFromFile(tempFilePath); break;
    case KifuFileReader::KifuFormat::USI:  loadUsiFromFile(tempFilePath); break;
    case KifuFileReader::KifuFormat::JKF:  loadJkfFromFile(tempFilePath); break;
    case KifuFileReader::KifuFormat::USEN: loadUsenFromFile(tempFilePath); break;
    default:                               loadKifuFromFile(tempFilePath); break;
    }

    QFile::remove(tempFilePath);
    qCDebug(lcKifu).noquote() << "removed temp file:" << tempFilePath;

    return true;
}

// ============================================================
// SFEN/BOD 局面読み込み（KifuApplyService に委譲）
// ============================================================

bool KifuLoadCoordinator::loadPositionFromSfen(const QString& sfenStr)
{
    return m_applyService->loadPositionFromSfen(sfenStr);
}

bool KifuLoadCoordinator::loadPositionFromBod(const QString& bodStr)
{
    return m_applyService->loadPositionFromBod(bodStr);
}

// ============================================================
// 初期SFEN検出
// ============================================================

QString KifuLoadCoordinator::prepareInitialSfen(const QString& filePath, QString& teaiLabel) const
{
    const QString sfen = KifToSfenConverter::detectInitialSfenFromFile(filePath, &teaiLabel);
    return sfen.isEmpty()
               ? QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
               : sfen;
}

// ============================================================
// 分岐マーカー描画
// ============================================================

void KifuLoadCoordinator::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);

    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
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
        painter->fillRect(opt.rect, QColor(255, 220, 160));
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
    static const QString kHirateStartSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    m_branchTree->setRootSfen(kHirateStartSfen);

    if (m_navState != nullptr) {
        m_navState->goToRoot();
    }

    m_branchablePlySet.clear();

    if (m_branchTreeManager) {
        QVector<BranchTreeManager::ResolvedRowLite> emptyRows;
        m_branchTreeManager->setBranchTreeRows(emptyRows);
    }

    qCDebug(lcKifu).noquote() << "resetBranchTreeForNewGame: done";
}
