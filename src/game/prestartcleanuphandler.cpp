/// @file prestartcleanuphandler.cpp
/// @brief 対局開始前クリーンアップハンドラクラスの実装

#include "prestartcleanuphandler.h"
#include "logcategories.h"
#include "matchcoordinator.h"

#include <QDebug>
#include <QLabel>
#include <QTableView>
#include <QItemSelectionModel>

#include "boardinteractioncontroller.h"
#include "shogiview.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "usicommlogmodel.h"
#include "timecontrolcontroller.h"
#include "kifudisplay.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "recordpane.h"
#include "livegamesession.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

// ============================================================
// 初期化
// ============================================================

PreStartCleanupHandler::PreStartCleanupHandler(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_boardController(deps.boardController)
    , m_shogiView(deps.shogiView)
    , m_kifuRecordModel(deps.kifuRecordModel)
    , m_kifuBranchModel(deps.kifuBranchModel)
    , m_lineEditModel1(deps.lineEditModel1)
    , m_lineEditModel2(deps.lineEditModel2)
    , m_timeController(deps.timeController)
    , m_evalChart(deps.evalChart)
    , m_evalGraphController(deps.evalGraphController)
    , m_recordPane(deps.recordPane)
    , m_startSfenStr(deps.startSfenStr)
    , m_currentSfenStr(deps.currentSfenStr)
    , m_activePly(deps.activePly)
    , m_currentSelectedPly(deps.currentSelectedPly)
    , m_currentMoveIndex(deps.currentMoveIndex)
    , m_liveGameSession(deps.liveGameSession)
    , m_branchTree(deps.branchTree)
    , m_navState(deps.navState)
{
}

// ============================================================
// メインクリーンアップ
// ============================================================

void PreStartCleanupHandler::performCleanup()
{
    qCDebug(lcGame).noquote() << "performCleanup: start"
                       << "m_startSfenStr=" << (m_startSfenStr ? m_startSfenStr->left(50) : "(null)")
                       << "m_currentSfenStr=" << (m_currentSfenStr ? m_currentSfenStr->left(50) : "(null)")
                       << "m_currentSelectedPly=" << (m_currentSelectedPly ? *m_currentSelectedPly : -1);

    // m_currentSfenStr を保存（selectStartRow() で変更される前に）
    // この値は startLiveGameSession() で分岐点を探すために使用される
    m_savedCurrentSfen = m_currentSfenStr ? *m_currentSfenStr : QString();
    m_savedSelectedPly = m_currentSelectedPly ? *m_currentSelectedPly : 0;

    // ユーザーが選択しているノードを直接保存
    // SFENで検索すると、同じ局面が複数存在する場合に誤ったノードが返される可能性がある
    m_savedCurrentNode = (m_navState != nullptr) ? m_navState->currentNode() : nullptr;

    qCDebug(lcGame).noquote() << "performCleanup: saved sfen=" << m_savedCurrentSfen.left(50)
                       << "saved ply=" << m_savedSelectedPly
                       << "saved node=" << (m_savedCurrentNode ? m_savedCurrentNode->displayText() : "(null)");

    clearBoardAndHighlights();
    clearClockDisplay();

    const bool startFromCurrentPos = isStartFromCurrentPosition();

    int keepRow = m_currentSelectedPly ? qMax(0, *m_currentSelectedPly) : 0;
    keepRow = cleanupKifuModel(startFromCurrentPos, keepRow);
    updatePlyTracking(startFromCurrentPos, keepRow);
    clearBranchModel();

    resetUsiLogModels();
    resetTimeControl();
    resetEvaluationGraph();
    selectStartRow();

    ensureBranchTreeRoot();

    if (m_branchTree != nullptr && m_branchTree->root() != nullptr) {
        qCDebug(lcGame).noquote() << "performCleanup: branchTree has"
                           << m_branchTree->root()->childCount() << "children"
                           << "lineCount=" << m_branchTree->lineCount();
    }

    // ライブゲームセッションを開始（途中局面からの場合は分岐点を設定）
    startLiveGameSession();

    qCDebug(lcGame).noquote()
        << "performCleanup: startFromCurrentPos=" << startFromCurrentPos
        << " keepRow=" << keepRow
        << " rows(after)=" << (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
}

// ============================================================
// 個別クリーンアップ処理
// ============================================================

bool PreStartCleanupHandler::isStartFromCurrentPosition() const
{
    if (!m_startSfenStr || !m_currentSfenStr) {
        return false;
    }

    const int selectedPly = m_currentSelectedPly ? *m_currentSelectedPly : 0;
    return shouldStartFromCurrentPosition(*m_startSfenStr, *m_currentSfenStr, selectedPly);
}

void PreStartCleanupHandler::clearBoardAndHighlights()
{
    if (m_boardController) {
        m_boardController->clearAllHighlights();
    }
}

void PreStartCleanupHandler::clearClockDisplay()
{
    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText(QStringLiteral("00:00:00"));
        m_shogiView->whiteClockLabel()->setText(QStringLiteral("00:00:00"));
    }
}

int PreStartCleanupHandler::cleanupKifuModel(bool startFromCurrentPos, int /*keepRow*/)
{
    if (!m_kifuRecordModel) return 0;

    if (startFromCurrentPos && m_branchTree) {
        // ユーザーが選択していたノードを直接使用
        KifuBranchNode* branchPoint = m_savedCurrentNode;

        // savedNode が利用できない場合のみ、SFENで検索（フォールバック）
        if (branchPoint == nullptr && !m_savedCurrentSfen.isEmpty()) {
            branchPoint = m_branchTree->findBySfen(m_savedCurrentSfen);
        }

        // 終局手（投了など）のノードが選択されていた場合は、その親ノードから再開
        if (branchPoint != nullptr && branchPoint->isTerminal()) {
            branchPoint = branchPoint->parent();
        }

        if (branchPoint != nullptr) {
            QList<KifuBranchNode*> path = m_branchTree->pathToNode(branchPoint);
            m_kifuRecordModel->clearAllItems();

            for (KifuBranchNode* node : std::as_const(path)) {
                auto* item = new KifuDisplay(node->displayText(), node->timeText(), node->comment());
                m_kifuRecordModel->appendItem(item);
            }
            return branchPoint->ply();
        }
    }

    // 平手/手合割など「新規初期局面から開始」またはフォールバック
    m_kifuRecordModel->clearAllItems();
    m_kifuRecordModel->appendItem(
        new KifuDisplay(tr("=== 開始局面 ==="),
                        tr("（１手 / 合計）")));
    return 0;
}

void PreStartCleanupHandler::updatePlyTracking(bool startFromCurrentPos, int keepRow)
{
    if (startFromCurrentPos) {
        if (m_activePly) *m_activePly = keepRow;
        if (m_currentSelectedPly) *m_currentSelectedPly = keepRow;
        if (m_currentMoveIndex) *m_currentMoveIndex = keepRow;
    } else {
        if (m_activePly) *m_activePly = 0;
        if (m_currentSelectedPly) *m_currentSelectedPly = 0;
        if (m_currentMoveIndex) *m_currentMoveIndex = 0;
    }
}

void PreStartCleanupHandler::clearBranchModel()
{
    if (m_kifuBranchModel) {
        m_kifuBranchModel->clear();
    }
}

void PreStartCleanupHandler::resetUsiLogModels()
{
    qCDebug(lcGame).noquote() << "resetUsiLogModels: RESETTING ENGINE NAMES";

    auto resetInfo = [](UsiCommLogModel* m) {
        if (!m) return;
        m->clear();
        m->setEngineName(QString());
        m->setPredictiveMove(QString());
        m->setSearchedMove(QString());
        m->setSearchDepth(QString());
        m->setNodeCount(QString());
        m->setNodesPerSecond(QString());
        m->setHashUsage(QString());
    };

    resetInfo(m_lineEditModel1);
    resetInfo(m_lineEditModel2);

    qCDebug(lcGame).noquote() << "resetUsiLogModels: ENGINE NAMES RESET DONE";
}

void PreStartCleanupHandler::resetTimeControl()
{
    if (m_timeController) {
        m_timeController->clearGameStartTime();
    }
}

void PreStartCleanupHandler::resetEvaluationGraph()
{
    const bool startFromCurrentPos = isStartFromCurrentPosition();
    if (!startFromCurrentPos) {
        if (m_evalChart) {
            m_evalChart->clearAll();
        }
        if (m_evalGraphController) {
            m_evalGraphController->clearScores();
        }
    } else {
        if (m_evalGraphController && m_currentMoveIndex) {
            const int trimPly = qMax(0, *m_currentMoveIndex);
            m_evalGraphController->trimToPly(trimPly);
        }
    }
}

void PreStartCleanupHandler::selectStartRow()
{
    if (!m_recordPane || !m_kifuRecordModel || m_kifuRecordModel->rowCount() <= 0) return;

    if (auto* view = m_recordPane->kifuView()) {
        if (auto* sel = view->selectionModel()) {
            const QModelIndex idx = m_kifuRecordModel->index(0, 0);
            sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
    m_kifuRecordModel->setCurrentHighlightRow(0);
}

// ============================================================
// ライブゲームセッション
// ============================================================

void PreStartCleanupHandler::startLiveGameSession()
{
    qCDebug(lcGame).noquote() << "startLiveGameSession: ENTER"
                       << "liveGameSession=" << static_cast<void*>(m_liveGameSession)
                       << "branchTree=" << static_cast<void*>(m_branchTree);

    if (m_liveGameSession == nullptr || m_branchTree == nullptr) {
        qCDebug(lcGame).noquote() << "startLiveGameSession: liveGameSession or branchTree is null";
        return;
    }

    qCDebug(lcGame).noquote() << "startLiveGameSession: branchTree->root()="
                       << static_cast<void*>(m_branchTree->root());

    // アクティブなセッションがあれば破棄
    if (m_liveGameSession->isActive()) {
        m_liveGameSession->discard();
    }

    static const QString kHirateStartSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    // 保存した値を使用（selectStartRow() で変更される前の値）
    const QString savedSfen = m_savedCurrentSfen.trimmed();
    const int savedPly = m_savedSelectedPly;
    KifuBranchNode* savedNode = m_savedCurrentNode;

    qCDebug(lcGame).noquote() << "startLiveGameSession: using saved sfen=" << savedSfen.left(50)
                       << "savedPly=" << savedPly
                       << "savedNode=" << (savedNode ? savedNode->displayText() : "(null)");

    // "startpos" または初期局面SFENの場合は新規対局として扱う
    bool isNewGame = savedSfen.isEmpty() ||
                     savedSfen == QStringLiteral("startpos") ||
                     savedSfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));

    qCDebug(lcGame).noquote() << "startLiveGameSession:"
                       << "isNewGame=" << isNewGame
                       << "savedPly=" << savedPly;

    bool hasExistingMoves = m_branchTree->root() != nullptr && m_branchTree->root()->childCount() > 0;

    if (!isNewGame && hasExistingMoves && savedPly > 0) {
        // ユーザーが選択していたノードを直接使用
        // SFENで検索すると、同じ局面が複数存在する場合に誤ったノードが返される
        KifuBranchNode* branchPoint = savedNode;

        // savedNode が利用できない場合のみ、SFENで検索（フォールバック）
        if (branchPoint == nullptr && !savedSfen.isEmpty()) {
            branchPoint = m_branchTree->findBySfen(savedSfen);
            qCDebug(lcGame).noquote() << "startLiveGameSession: fallback to findBySfen, result="
                               << static_cast<void*>(branchPoint)
                               << "ply=" << (branchPoint ? branchPoint->ply() : -1);
        }

        qCDebug(lcGame).noquote() << "startLiveGameSession: branchPoint="
                           << static_cast<void*>(branchPoint)
                           << "ply=" << (branchPoint ? branchPoint->ply() : -1)
                           << "isTerminal=" << (branchPoint ? branchPoint->isTerminal() : false);

        // 終局手の場合は親ノードを分岐点として使用
        if (branchPoint != nullptr && branchPoint->isTerminal()) {
            branchPoint = branchPoint->parent();
            qCDebug(lcGame).noquote() << "startLiveGameSession: terminal node, using parent="
                               << static_cast<void*>(branchPoint)
                               << "ply=" << (branchPoint ? branchPoint->ply() : -1);
        }

        if (branchPoint != nullptr && branchPoint->ply() > 0) {
            m_liveGameSession->startFromNode(branchPoint);

            // m_currentSfenStr を branchPoint の SFEN で更新
            // GameStartCoordinator::prepareDataCurrentPosition() で盤面が正しい位置にリセットされる
            if (m_currentSfenStr != nullptr) {
                *m_currentSfenStr = branchPoint->sfen();
                qCDebug(lcGame).noquote() << "startLiveGameSession: updated m_currentSfenStr to branchPoint sfen="
                                   << branchPoint->sfen().left(60);
            }

            qCDebug(lcGame).noquote() << "startLiveGameSession: started from node, ply=" << branchPoint->ply()
                               << "displayText=" << branchPoint->displayText();
            return;
        }
        qCDebug(lcGame).noquote() << "startLiveGameSession: branchPoint not found or is root, treating as new game";
    }

    // 新規対局または途中局面が見つからない場合
    {
        if (m_branchTree->root() == nullptr) {
            QString rootSfen = kHirateStartSfen;
            if (m_startSfenStr != nullptr && !m_startSfenStr->trimmed().isEmpty()) {
                rootSfen = *m_startSfenStr;
            }
            m_branchTree->setRootSfen(rootSfen);
            qCDebug(lcGame).noquote() << "startLiveGameSession: created root node with sfen=" << rootSfen;
        }

        m_liveGameSession->startFromRoot();
        qCDebug(lcGame).noquote() << "startLiveGameSession: started from root (new game)";
    }
}

void PreStartCleanupHandler::ensureBranchTreeRoot()
{
    if (m_branchTree == nullptr) {
        return;
    }

    static const QString kHirateStartSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    QString rootSfen = kHirateStartSfen;
    if (m_startSfenStr != nullptr && !m_startSfenStr->trimmed().isEmpty()) {
        rootSfen = *m_startSfenStr;
    }

    if (m_branchTree->root() != nullptr) {
        // ルートが既に存在する場合でも、SFENが異なれば更新する
        if (m_branchTree->root()->sfen() != rootSfen) {
            m_branchTree->clear();
            m_branchTree->setRootSfen(rootSfen);
            qCDebug(lcGame).noquote() << "ensureBranchTreeRoot: updated root sfen=" << rootSfen;
        } else {
            qCDebug(lcGame).noquote() << "ensureBranchTreeRoot: root already exists with correct sfen";
        }
        return;
    }

    m_branchTree->setRootSfen(rootSfen);
    qCDebug(lcGame).noquote() << "ensureBranchTreeRoot: created root with sfen=" << rootSfen;
}
