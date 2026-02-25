/// @file livegamesessionupdater.cpp
/// @brief LiveGameSession の更新ロジック実装

#include "livegamesessionupdater.h"

#include "livegamesession.h"
#include "kifubranchtree.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "logcategories.h"

void LiveGameSessionUpdater::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void LiveGameSessionUpdater::ensureSessionStarted()
{
    if (m_deps.liveSession == nullptr || m_deps.branchTree == nullptr) {
        return;
    }

    if (m_deps.liveSession->isActive()) {
        return;  // 既に開始済み
    }

    // ルートノードが必要（performCleanup で作成済みのはず）
    if (m_deps.branchTree->root() == nullptr) {
        qCWarning(lcApp).noquote() << "ensureSessionStarted: root is null, cannot start session";
        return;
    }

    // 現在の局面が途中局面かどうかを判定
    const QString currentSfen = m_deps.currentSfenStr ? m_deps.currentSfenStr->trimmed() : QString();
    bool isFromMidPosition = !currentSfen.isEmpty() &&
                              currentSfen != QStringLiteral("startpos") &&
                              !currentSfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));

    if (isFromMidPosition) {
        // 途中局面から開始: SFENからノードを探す
        KifuBranchNode* branchPoint = m_deps.branchTree->findBySfen(currentSfen);
        if (branchPoint != nullptr) {
            m_deps.liveSession->startFromNode(branchPoint);
            qCDebug(lcApp).noquote() << "ensureSessionStarted: started from node, ply=" << branchPoint->ply();
        } else {
            // ノードが見つからない場合はルートから開始
            qCWarning(lcApp).noquote() << "ensureSessionStarted: branchPoint not found for sfen, starting from root";
            m_deps.liveSession->startFromRoot();
        }
    } else {
        // 新規対局: ルートから開始
        m_deps.liveSession->startFromRoot();
        qCDebug(lcApp).noquote() << "ensureSessionStarted: session started from root";
    }
}

void LiveGameSessionUpdater::appendMove(const ShogiMove& move, const QString& moveText, const QString& elapsedTime)
{
    if (m_deps.liveSession == nullptr) {
        return;
    }

    // セッションが未開始の場合は遅延開始
    if (!m_deps.liveSession->isActive()) {
        ensureSessionStarted();
    }

    if (!m_deps.liveSession->isActive()) {
        return;
    }

    // 実際の盤面から完全な SFEN を構築する
    QString sfen;
    if (m_deps.gameController && m_deps.gameController->board()) {
        ShogiBoard* board = m_deps.gameController->board();
        // 完全な SFEN を構築: <盤面> <手番> <持ち駒> <手数>
        const QString boardPart = board->convertBoardToSfen();
        // 手番は GC を正とする（board->currentPlayer() は setSfen() 以外で更新されない）
        const QString turnPart =
            (m_deps.gameController->currentPlayer() == ShogiGameController::Player2)
                ? QStringLiteral("w")
                : QStringLiteral("b");
        QString standPart = board->convertStandToSfen();
        if (standPart.isEmpty()) {
            standPart = QStringLiteral("-");
        }
        // 手数は anchorPly + 現在のセッション内の手数 + 1
        const int moveCount = m_deps.liveSession->totalPly() + 1;
        sfen = QStringLiteral("%1 %2 %3 %4")
                   .arg(boardPart, turnPart, standPart, QString::number(moveCount));
        qCDebug(lcApp).noquote() << "appendMove: constructed full SFEN for LiveGameSession"
                           << "sfen=" << sfen.left(80);
    } else if (m_deps.sfenRecord != nullptr && !m_deps.sfenRecord->isEmpty()) {
        // フォールバック: 盤面がない場合は sfenRecord を使用
        sfen = m_deps.sfenRecord->last();
        qCWarning(lcApp) << "appendMove: fallback to sfenRecord (no board)";
    }

    m_deps.liveSession->addMove(move, moveText, sfen, elapsedTime);
}
