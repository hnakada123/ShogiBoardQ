/// @file mainwindowgamestartservice.cpp
/// @brief MainWindow の対局開始前処理を分離したサービス実装

#include "mainwindowgamestartservice.h"

#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "logcategories.h"

#include <QDebug>

void MainWindowGameStartService::prepareForInitializeGame(const PrepareDeps& deps) const
{
    rebuildSfenRecordForSelectedBranch(deps);

    if (!deps.currentSfenStr) {
        return;
    }

    qCDebug(lcApp).noquote() << "initializeGame: BEFORE resolve, m_state.currentSfenStr="
                             << deps.currentSfenStr->left(50);
    const QString sfen = resolveCurrentSfenForGameStart(deps).trimmed();
    qCDebug(lcApp).noquote() << "initializeGame: resolved sfen=" << sfen.left(50);

    if (!sfen.isEmpty()) {
        *deps.currentSfenStr = sfen;
        qCDebug(lcApp).noquote() << "initializeGame: SET m_state.currentSfenStr="
                                 << deps.currentSfenStr->left(50);
    } else {
        qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart_: empty. delegate to coordinator.";
    }
}

void MainWindowGameStartService::rebuildSfenRecordForSelectedBranch(const PrepareDeps& deps) const
{
    if (deps.branchTree == nullptr || deps.navState == nullptr || deps.sfenRecord == nullptr
        || deps.currentSelectedPly <= 0) {
        return;
    }

    const int lineIndex = deps.navState->currentLineIndex();
    const QStringList branchSfens = deps.branchTree->getSfenListForLine(lineIndex);
    if (branchSfens.isEmpty() || deps.currentSelectedPly >= branchSfens.size()) {
        return;
    }

    deps.sfenRecord->clear();
    for (int i = 0; i <= deps.currentSelectedPly; ++i) {
        deps.sfenRecord->append(branchSfens.at(i));
    }

    qCDebug(lcApp).noquote()
        << "initializeGame: rebuilt sfenRecord from branchTree line=" << lineIndex
        << " entries=" << deps.sfenRecord->size();
}

QString MainWindowGameStartService::resolveCurrentSfenForGameStart(const PrepareDeps& deps) const
{
    qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: m_kifu.currentSelectedPly="
                             << deps.currentSelectedPly
                             << " sfenRecord()=" << (deps.sfenRecord ? "exists" : "null")
                             << " size=" << (deps.sfenRecord ? deps.sfenRecord->size() : -1);

    if (deps.sfenRecord != nullptr) {
        const qsizetype size = deps.sfenRecord->size();
        int idx = deps.currentSelectedPly;
        qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: initial idx=" << idx;

        if (idx < 0) {
            idx = 0;
        }
        if (idx >= size && size > 0) {
            qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: idx(" << idx << ") >= size("
                                     << size << "), clamping to " << (size - 1);
            idx = static_cast<int>(size - 1);
        }
        if (idx >= 0 && idx < size) {
            const QString s = deps.sfenRecord->at(idx).trimmed();
            qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: at(" << idx << ")="
                                     << s.left(50);
            if (!s.isEmpty()) {
                return s;
            }
        }
    }

    qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: returning EMPTY";
    return QString();
}

GameStartCoordinator::Ctx MainWindowGameStartService::buildContext(const ContextDeps& deps) const
{
    GameStartCoordinator::Ctx c;
    c.gc = deps.gc;
    c.clock = deps.clock;
    c.sfenRecord = deps.sfenRecord;
    c.kifuModel = deps.kifuModel;
    c.kifuLoadCoordinator = deps.kifuLoadCoordinator;
    c.currentSfenStr = deps.currentSfenStr;
    c.startSfenStr = deps.startSfenStr;
    c.selectedPly = deps.selectedPly;
    c.isReplayMode = deps.isReplayMode;
    c.bottomIsP1 = deps.bottomIsP1;
    return c;
}
