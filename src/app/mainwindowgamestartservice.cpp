/// @file mainwindowgamestartservice.cpp
/// @brief MainWindow の対局開始前処理を分離したサービス実装

#include "mainwindowgamestartservice.h"

#include "logcategories.h"

#include <QDebug>

void MainWindowGameStartService::prepareForInitializeGame(const PrepareDeps& deps) const
{
    rebuildMatchHistoryFromLine(deps);

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

void MainWindowGameStartService::rebuildMatchHistoryFromLine(const PrepareDeps& deps) const
{
    // 対局の局面列・指し手列を、棋譜の正本（分岐ツリーの表示中の手順）の開始局面から
    // 開始する手数までで作り直す。本譜か分岐かは区別しない。局面列の先頭が棋譜の開始局面になる
    const int ply = deps.line.currentPly;
    if (deps.sfenRecord == nullptr || deps.line.isEmpty()) {
        return;
    }

    *deps.sfenRecord = deps.line.sfens.mid(0, ply + 1);
    if (deps.gameMoves != nullptr) {
        *deps.gameMoves = deps.line.moves.mid(0, ply);
    }

    qCDebug(lcApp).noquote()
        << "initializeGame: rebuilt match history from the record line, entries=" << deps.sfenRecord->size();
}

QString MainWindowGameStartService::resolveCurrentSfenForGameStart(const PrepareDeps& deps) const
{
    qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: currentPly="
                             << deps.line.currentPly
                             << " sfenRecord()=" << (deps.sfenRecord ? "exists" : "null")
                             << " size=" << (deps.sfenRecord ? deps.sfenRecord->size() : -1);

    if (deps.sfenRecord != nullptr) {
        const qsizetype size = deps.sfenRecord->size();
        int idx = deps.line.currentPly;
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
