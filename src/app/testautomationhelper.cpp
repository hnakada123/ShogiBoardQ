#include "testautomationhelper.h"

#include <QDebug>
#include <QModelIndex>
#include <QPushButton>
#include <QTableView>
#include <QRegularExpression>

#include "recordpane.h"
#include "engineanalysistab.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifudisplaycoordinator.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"

TestAutomationHelper::TestAutomationHelper(QObject* parent)
    : QObject(parent)
{
}

void TestAutomationHelper::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void TestAutomationHelper::setTestMode(bool enabled)
{
    m_testMode = enabled;
    qDebug() << "[TEST] Test mode set to:" << enabled;
}

void TestAutomationHelper::navigateToPly(int ply)
{
    qDebug() << "[TEST] navigateToPly:" << ply;

    if (m_deps.recordPane != nullptr) {
        QTableView* kifuView = m_deps.recordPane->kifuView();
        if (kifuView != nullptr) {
            kifuView->selectRow(ply);
            qDebug() << "[TEST] navigateToPly: selected row" << ply;
        } else {
            qWarning() << "[TEST] navigateToPly: kifuView is null";
        }
    } else {
        qWarning() << "[TEST] navigateToPly: m_recordPane is null";
    }
}

void TestAutomationHelper::clickBranchCandidate(int index)
{
    qDebug() << "[TEST] clickBranchCandidate:" << index;

    if (m_deps.displayCoordinator != nullptr) {
        if (m_deps.kifuBranchModel != nullptr) {
            QModelIndex modelIndex = m_deps.kifuBranchModel->index(index, 0);
            m_deps.displayCoordinator->onBranchCandidateActivated(modelIndex);
            qDebug() << "[TEST] clickBranchCandidate: dispatched to coordinator";
        }
    } else {
        qWarning() << "[TEST] clickBranchCandidate: m_displayCoordinator is null";
    }
}

void TestAutomationHelper::clickNextButton()
{
    qDebug() << "[TEST] clickNextButton";

    if (m_deps.recordPane != nullptr) {
        QPushButton* nextBtn = m_deps.recordPane->nextButton();
        if (nextBtn != nullptr) {
            nextBtn->click();
            qDebug() << "[TEST] clickNextButton: button clicked";
        } else {
            qWarning() << "[TEST] clickNextButton: nextButton is null";
        }
    } else {
        qWarning() << "[TEST] clickNextButton: m_recordPane is null";
    }
}

void TestAutomationHelper::clickPrevButton()
{
    qDebug() << "[TEST] clickPrevButton";

    if (m_deps.recordPane != nullptr) {
        QPushButton* prevBtn = m_deps.recordPane->prevButton();
        if (prevBtn != nullptr) {
            prevBtn->click();
            qDebug() << "[TEST] clickPrevButton: button clicked";
        } else {
            qWarning() << "[TEST] clickPrevButton: prevButton is null";
        }
    } else {
        qWarning() << "[TEST] clickPrevButton: m_recordPane is null";
    }
}

void TestAutomationHelper::clickFirstButton()
{
    qDebug() << "[TEST] clickFirstButton";

    if (m_deps.recordPane != nullptr) {
        QPushButton* firstBtn = m_deps.recordPane->firstButton();
        if (firstBtn != nullptr) {
            firstBtn->click();
            qDebug() << "[TEST] clickFirstButton: button clicked";
        } else {
            qWarning() << "[TEST] clickFirstButton: firstButton is null";
        }
    } else {
        qWarning() << "[TEST] clickFirstButton: m_recordPane is null";
    }
}

void TestAutomationHelper::clickLastButton()
{
    qDebug() << "[TEST] clickLastButton";

    if (m_deps.recordPane != nullptr) {
        QPushButton* lastBtn = m_deps.recordPane->lastButton();
        if (lastBtn != nullptr) {
            lastBtn->click();
            qDebug() << "[TEST] clickLastButton: button clicked";
        } else {
            qWarning() << "[TEST] clickLastButton: lastButton is null";
        }
    } else {
        qWarning() << "[TEST] clickLastButton: m_recordPane is null";
    }
}

void TestAutomationHelper::clickKifuRow(int row)
{
    qDebug() << "[TEST] clickKifuRow:" << row;

    // ★ ユーザー操作（テスト）による棋譜欄クリックの前にフラグをリセット
    if (m_deps.skipBoardSyncForBranchNav != nullptr) {
        *m_deps.skipBoardSyncForBranchNav = false;
    }

    if (m_deps.recordPane == nullptr) {
        qWarning() << "[TEST] clickKifuRow: m_recordPane is null";
        return;
    }

    QTableView* kifuView = m_deps.recordPane->kifuView();
    if (kifuView == nullptr) {
        qWarning() << "[TEST] clickKifuRow: kifuView is null";
        return;
    }

    // 指定行を選択してアクティベート
    QModelIndex index = kifuView->model()->index(row, 0);
    if (!index.isValid()) {
        qWarning() << "[TEST] clickKifuRow: invalid index for row" << row;
        return;
    }

    // currentIndex を設定してからクリックシグナルをエミュレート
    kifuView->setCurrentIndex(index);
    kifuView->scrollTo(index);

    // activated シグナルを発火（ダブルクリック相当）
    emit kifuView->activated(index);

    qDebug() << "[TEST] clickKifuRow: row" << row << "clicked";
}

void TestAutomationHelper::clickBranchTreeNode(int row, int ply)
{
    qDebug() << "[TEST] clickBranchTreeNode: row=" << row << "ply=" << ply;

    if (m_deps.analysisTab) {
        // まずハイライトを更新（GUIクリック時の即時フィードバックをシミュレート）
        m_deps.analysisTab->highlightBranchTreeAt(row, ply, false);
        // シグナルを発火
        emit m_deps.analysisTab->branchNodeActivated(row, ply);
    } else {
        qWarning() << "[TEST] clickBranchTreeNode: m_analysisTab is null";
    }

    qDebug() << "[TEST] clickBranchTreeNode: completed";
}

void TestAutomationHelper::dumpTestState()
{
    qDebug() << "========== [TEST STATE DUMP] ==========";

    // 1. 盤面情報
    qDebug() << "[TEST] === BOARD STATE ===";
    if (m_deps.gameController && m_deps.gameController->board()) {
        qDebug() << "[TEST] board currentPlayer:" << m_deps.gameController->board()->currentPlayer();
        qDebug() << "[TEST] board actualSfen:" << m_deps.gameController->board()->convertBoardToSfen();
    }

    // 2. 棋譜欄の状態
    qDebug() << "[TEST] === KIFU LIST STATE ===";
    if (m_deps.kifuRecordModel) {
        const int rowCount = m_deps.kifuRecordModel->rowCount();
        qDebug() << "[TEST] kifuRecordModel rowCount:" << rowCount;
        qDebug() << "[TEST] kifuRecordModel currentHighlightRow:" << m_deps.kifuRecordModel->currentHighlightRow();
        for (int i = 0; i < qMin(rowCount, 10); ++i) {
            KifuDisplay* item = m_deps.kifuRecordModel->item(i);
            if (item) {
                qDebug().noquote() << "[TEST]   kifu[" << i << "]:" << item->currentMove();
            }
        }
        if (rowCount > 10) {
            qDebug() << "[TEST]   ... and" << (rowCount - 10) << "more";
        }
    }

    // 3. 分岐候補欄の状態
    qDebug() << "[TEST] === BRANCH CANDIDATES STATE ===";
    if (m_deps.kifuBranchModel) {
        qDebug() << "[TEST] kifuBranchModel rowCount:" << m_deps.kifuBranchModel->rowCount();
        qDebug() << "[TEST] kifuBranchModel hasBackToMainRow:" << m_deps.kifuBranchModel->hasBackToMainRow();
        for (int i = 0; i < m_deps.kifuBranchModel->rowCount(); ++i) {
            QModelIndex idx = m_deps.kifuBranchModel->index(i, 0);
            qDebug() << "[TEST]   branch[" << i << "]:"
                     << m_deps.kifuBranchModel->data(idx, Qt::DisplayRole).toString();
        }
    }
    if (m_deps.recordPane && m_deps.recordPane->branchView()) {
        qDebug() << "[TEST] branchView enabled:" << m_deps.recordPane->branchView()->isEnabled();
    }

    // 4. ナビゲーション状態
    qDebug() << "[TEST] === NAVIGATION STATE ===";
    if (m_deps.navState) {
        qDebug() << "[TEST] currentPly:" << m_deps.navState->currentPly();
        qDebug() << "[TEST] currentLineIndex:" << m_deps.navState->currentLineIndex();
        qDebug() << "[TEST] isOnMainLine:" << m_deps.navState->isOnMainLine();
        if (m_deps.navState->currentNode()) {
            qDebug() << "[TEST] currentNode displayText:" << m_deps.navState->currentNode()->displayText();
        }
    }

    // 5. 分岐ツリーの状態
    qDebug() << "[TEST] === BRANCH TREE STATE ===";
    if (m_deps.branchTree) {
        auto lines = m_deps.branchTree->allLines();
        qDebug() << "[TEST] branchTree lineCount:" << lines.size();
        for (int i = 0; i < lines.size() && i < 5; ++i) {
            const auto& line = lines[i];
            qDebug() << "[TEST]   line[" << i << "] nodeCount:" << line.nodes.size()
                     << "branchPly:" << line.branchPly
                     << "branchPoint:" << (line.branchPoint ? line.branchPoint->displayText() : "null");
        }
    }

    // 6. 一致性チェック
    qDebug() << "[TEST] === CONSISTENCY CHECK ===";
    if (m_deps.displayCoordinator) {
        const bool displayConsistent = m_deps.displayCoordinator->verifyDisplayConsistency();
        qDebug() << "[TEST] displayConsistency:" << (displayConsistent ? "PASS" : "FAIL");
        if (!displayConsistent) {
            qDebug().noquote() << m_deps.displayCoordinator->getConsistencyReport();
        }
    }

    // 7. 4方向一致検証
    const bool fourWayConsistent = verify4WayConsistency();
    qDebug() << "[TEST] fourWayConsistency:" << (fourWayConsistent ? "PASS" : "FAIL");

    qDebug() << "========== [END STATE DUMP] ==========";
}

QString TestAutomationHelper::extractSfenBase(const QString& sfen) const
{
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.isEmpty()) {
        return sfen;
    }
    return parts.at(0);
}

bool TestAutomationHelper::verify4WayConsistency()
{
    bool consistent = true;

    // 1. 盤面SFENとナビゲーション状態のSFENを比較
    if (m_deps.gameController != nullptr && m_deps.gameController->board() != nullptr && m_deps.navState != nullptr) {
        const QString boardSfen = m_deps.gameController->board()->convertBoardToSfen();
        KifuBranchNode* currentNode = m_deps.navState->currentNode();
        if (currentNode != nullptr) {
            const QString nodeSfen = currentNode->sfen();
            const QString boardBase = extractSfenBase(boardSfen);
            const QString nodeBase = extractSfenBase(nodeSfen);
            if (boardBase != nodeBase) {
                consistent = false;
                qWarning() << "[4WAY] Board SFEN mismatch:"
                           << "board=" << boardBase
                           << "node=" << nodeBase;
            }
        }
    }

    // 2. 棋譜欄の内容が選択ラインの内容と一致するかを検証
    if (m_deps.kifuRecordModel != nullptr && m_deps.navState != nullptr && m_deps.branchTree != nullptr) {
        const int currentLineIndex = m_deps.navState->currentLineIndex();
        QVector<BranchLine> lines = m_deps.branchTree->allLines();
        if (currentLineIndex >= 0 && currentLineIndex < lines.size()) {
            const BranchLine& line = lines.at(currentLineIndex);
            if (line.nodes.size() > 3 && m_deps.kifuRecordModel->rowCount() > 3) {
                KifuDisplay* item = m_deps.kifuRecordModel->item(3);
                if (item != nullptr && line.nodes.size() > 2) {
                    for (KifuBranchNode* node : std::as_const(line.nodes)) {
                        if (node->ply() == 3) {
                            QString itemMove = item->currentMove().trimmed();
                            QString nodeMove = node->displayText();
                            itemMove = itemMove.remove(QRegularExpression(QStringLiteral("^\\s*\\d+\\s+")));
                            itemMove = itemMove.remove(QLatin1Char('+'));
                            if (!itemMove.contains(nodeMove) && !nodeMove.contains(itemMove)) {
                                if (itemMove != nodeMove) {
                                    consistent = false;
                                    qWarning() << "[4WAY] Kifu content mismatch at ply 3:"
                                               << "model=" << itemMove
                                               << "node=" << nodeMove;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // 3. 分岐候補数が期待値と一致するかを検証
    if (m_deps.kifuBranchModel != nullptr && m_deps.navState != nullptr) {
        KifuBranchNode* currentNode = m_deps.navState->currentNode();
        if (currentNode != nullptr) {
            KifuBranchNode* parent = currentNode->parent();
            if (parent != nullptr) {
                const int expectedCandidates = parent->childCount();
                int actualCandidates = m_deps.kifuBranchModel->rowCount();
                if (m_deps.kifuBranchModel->hasBackToMainRow() && actualCandidates > 0) {
                    actualCandidates--;
                }
                if (expectedCandidates > 1 && actualCandidates != expectedCandidates) {
                    qWarning() << "[4WAY] Branch candidate count mismatch:"
                               << "expected=" << expectedCandidates
                               << "actual=" << actualCandidates;
                }
            }
        }
    }

    // 4. KifuDisplayCoordinator の一致性を確認
    if (m_deps.displayCoordinator != nullptr) {
        if (!m_deps.displayCoordinator->verifyDisplayConsistency()) {
            consistent = false;
            qWarning() << "[4WAY] KifuDisplayCoordinator inconsistency detected";
        }
    }

    return consistent;
}

int TestAutomationHelper::getBranchTreeNodeCount()
{
    if (m_deps.branchTree == nullptr) {
        return 0;
    }

    int totalNodes = 0;
    const auto lines = m_deps.branchTree->allLines();
    for (const auto& line : lines) {
        totalNodes += static_cast<int>(line.nodes.size());
    }

    if (!lines.isEmpty()) {
        return static_cast<int>(lines.first().nodes.size());
    }

    return totalNodes;
}

bool TestAutomationHelper::verifyBranchTreeNodeCount(int minExpected)
{
    const int actual = getBranchTreeNodeCount();
    const bool pass = (actual >= minExpected);

    qDebug() << "[TEST] verifyBranchTreeNodeCount:"
             << "expected>=" << minExpected
             << "actual=" << actual
             << (pass ? "PASS" : "FAIL");

    return pass;
}
