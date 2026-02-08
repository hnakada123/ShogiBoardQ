/// @file positioneditcontroller.cpp
/// @brief 局面編集モードの開始・終了・盤操作の実装

#include "positioneditcontroller.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "boardinteractioncontroller.h"

#include <QStringList>
#include <QRegularExpression>
#include <Qt>

// ======================================================================
// 無名名前空間ヘルパ
// ======================================================================

namespace {

/// 将棋の平手初期配置（board部分）
static const QString kInitialBoard =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
static const QString kInitialStand = QStringLiteral("-");

/// 入力文字列（startpos / sfen ... / 最小SFEN / 不明形式）を最小SFENに正規化する
static QString toMinimalSfen(const QString& in)
{
    const QString s = in.trimmed();
    if (s.isEmpty()) {
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    const QStringList parts = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    // "startpos"（moves は編集開始では無視）
    if (parts[0] == QStringLiteral("startpos")) {
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    // "sfen <board> <turn> <stand> <ply> (moves ...)?"
    if (parts[0] == QStringLiteral("sfen")) {
        if (parts.size() >= 5) {
            return QStringLiteral("%1 %2 %3 %4").arg(parts[1], parts[2], parts[3], parts[4]);
        }
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    // 既に最小SFEN（board に / を含む）
    if (parts.size() >= 4 && parts[0].contains(QLatin1Char('/'))) {
        return QStringLiteral("%1 %2 %3 %4").arg(parts[0], parts[1], parts[2], parts[3]);
    }

    // 不明形式は平手へフォールバック
    return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
}

/// 最小SFENの手番と手数を上書きして返す
static QString forceTurnAndPly(const QString& minimalSfen, QChar turnBW, int ply = 1)
{
    QStringList t = minimalSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    while (t.size() < 4) t << QString();
    t[1] = QString(turnBW);
    t[3] = QString::number(ply);
    return QStringLiteral("%1 %2 %3 %4").arg(t[0], t[1], t[2], t[3]);
}

static QChar toBW(ShogiGameController::Player p)
{
    return (p == ShogiGameController::Player2) ? QLatin1Char('w') : QLatin1Char('b');
}

static ShogiGameController::Player fromBW(const QString& bw)
{
    return (bw == QStringLiteral("w"))
    ? ShogiGameController::Player2
    : ShogiGameController::Player1;
}

} // namespace

// ======================================================================
// 編集ライフサイクル
// ======================================================================

void PositionEditController::beginPositionEditing(const BeginEditContext& c)
{
    // 処理フロー:
    // 1. 編集開始SFENを既存の棋譜/局面状態から決定
    // 2. SFENを正規化して盤面に適用、編集モードへ遷移
    // 3. ハイライト初期化、GC手番同期、UIコールバック呼び出し

    if (!c.view || !c.gc) return;
    if (!c.view->board()) return;

    m_view = c.view;
    m_gc   = c.gc;
    m_bic  = c.bic;

    ShogiBoard* board = c.view->board();

    // GCの手番から desiredTurn を決める（未設定なら先手扱い）
    ShogiGameController::Player preSide = c.gc->currentPlayer();
    if (preSide == ShogiGameController::NoPlayer) preSide = ShogiGameController::Player1;
    const QChar desiredTurn = toBW(preSide);

    // 編集開始SFENの決定（record / current / resume / 盤面 から）
    QString baseSfen;
    if (c.sfenRecord && !c.sfenRecord->isEmpty()) {
        const qsizetype lastIdx = c.sfenRecord->size() - 1;
        qsizetype idx = -1;
        if (c.gameOver) {
            idx = lastIdx;
        } else if (c.selectedPly >= 0 && c.selectedPly <= lastIdx) {
            idx = c.selectedPly;
        } else if (c.activePly >= 0 && c.activePly <= lastIdx) {
            idx = c.activePly;
        } else {
            idx = lastIdx;
        }
        if (idx >= 0 && idx <= lastIdx) baseSfen = c.sfenRecord->at(idx);
    }
    if (baseSfen.isEmpty()) {
        if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
            baseSfen = *c.currentSfenStr;
        } else if (c.resumeSfenStr && !c.resumeSfenStr->isEmpty()) {
            baseSfen = *c.resumeSfenStr;
        } else {
            baseSfen = QStringLiteral("%1 %2 %3 %4")
                           .arg(board->convertBoardToSfen(),
                                board->currentPlayer(),
                                board->convertStandToSfen(),
                                QString::number(1));
        }
    }

    // 最小SFENへ正規化し、手番/手数を強制上書き
    const QString minimal  = toMinimalSfen(baseSfen);
    const QString adjusted = forceTurnAndPly(minimal, desiredTurn, /*ply*/1);

    // 盤へ適用し、編集モードへ
    board->setSfen(adjusted);
    c.view->setPositionEditMode(true);
    c.view->setMouseClickMode(true);
    c.view->update();

    // メニュー切替（MainWindow へ通知）
    if (c.onEnterEditMenu) c.onEnterEditMenu();

    // ハイライト等の初期化
    if (c.bic) {
        c.bic->setMode(BoardInteractionController::Mode::Edit);
        c.bic->clearAllHighlights();
    }

    // GC の手番を盤へ同期
    const QString bw = board->currentPlayer();
    c.gc->setCurrentPlayer(fromBW(bw));

    // 「編集終了」ボタン表示
    if (c.onShowEditExitButton) c.onShowEditExitButton();

    // startSfenStr にも記録
    if (c.sfenRecord && c.startSfenStr) {
        if (!c.sfenRecord->isEmpty()) *c.startSfenStr = c.sfenRecord->first();
        else                          *c.startSfenStr = adjusted;
    }
}

void PositionEditController::finishPositionEditing(const PositionEditController::FinishEditContext& c)
{
    if (!c.gc || !c.view || !c.view->board()) return;

    ShogiBoard* board = c.view->board();

    // 盤モデルから SFEN を再構成（最小SFEN）
    const QString bw = board->currentPlayer();
    const QString sfenNow = QStringLiteral("%1 %2 %3 %4")
                                .arg(board->convertBoardToSfen(),
                                     bw,
                                     board->convertStandToSfen(),
                                     QString::number(1));

    // 盤面へ反映し、描画
    board->setSfen(sfenNow);
    c.view->setPositionEditMode(false);
    c.view->setMouseClickMode(false);
    c.view->update();

    // sfenRecord を 0手局面として再保存
    if (c.sfenRecord) {
        c.sfenRecord->clear();
        c.sfenRecord->push_back(sfenNow);
    }

    // startSfen にも反映
    if (c.startSfenStr) {
        if (c.sfenRecord && !c.sfenRecord->isEmpty()) *c.startSfenStr = c.sfenRecord->first();
        else                                          *c.startSfenStr = sfenNow;
    }

    // 再開フラグを落とす
    if (c.isResumeFromCurrent) *c.isResumeFromCurrent = false;

    // UI 後片付け
    if (c.bic) {
        c.bic->clearAllHighlights();
    }
    if (c.onHideEditExitButton) c.onHideEditExitButton();

    // メニュー非表示（MainWindow 実装のコールバック）
    if (c.onLeaveEditMenu) c.onLeaveEditMenu();

    // 編集セッションの参照を保持（アクション用スロットで利用）
    m_view = c.view;
    m_gc   = c.gc;
    m_bic  = c.bic;
}

// ======================================================================
// 盤面操作API
// ======================================================================

void PositionEditController::resetPiecesToStand(ShogiView* view, BoardInteractionController* bic)
{
    if (!view) return;
    if (bic) bic->clearAllHighlights();
    view->resetAndEqualizePiecesOnStands();
}

void PositionEditController::setStandardStartPosition(ShogiView* view, BoardInteractionController* bic)
{
    if (!view) return;
    if (bic) bic->clearAllHighlights();
    view->initializeToFlatStartingPosition();
}

void PositionEditController::setTsumeShogiStartPosition(ShogiView* view, BoardInteractionController* bic)
{
    if (!view) return;
    if (bic) bic->clearAllHighlights();
    view->shogiProblemInitialPosition();
}

// ======================================================================
// 編集終了ボタン制御
// ======================================================================

void PositionEditController::showEditExitButtonOnBoard(ShogiView* view, QObject* receiver, const char* finishSlot)
{
    if (!view) return;

    // ShogiView 側で作成&配置（必要なら内部で生成）
    view->relayoutEditExitButton();

    if (QPushButton* exitBtn = view->findChild<QPushButton*>(QStringLiteral("editExitButton"))) {
        // 重複接続防止
        QObject::disconnect(exitBtn, SIGNAL(clicked()), receiver, finishSlot);
        QObject::connect(exitBtn, SIGNAL(clicked()), receiver, finishSlot);
        exitBtn->show();
        exitBtn->raise();
    }
}

void PositionEditController::hideEditExitButtonOnBoard(ShogiView* view)
{
    if (!view) return;
    if (QPushButton* exitBtn = view->findChild<QPushButton*>(QStringLiteral("editExitButton"))) {
        exitBtn->hide();
    }
}

// ======================================================================
// アクション用スロット
// ======================================================================

void PositionEditController::onReturnAllPiecesOnStandTriggered()
{
    if (!m_view) return;
    resetPiecesToStand(m_view, m_bic);
}

void PositionEditController::onFlatHandInitialPositionTriggered()
{
    if (!m_view) return;
    setStandardStartPosition(m_view, m_bic);
}

void PositionEditController::onShogiProblemInitialPositionTriggered()
{
    if (!m_view) return;
    setTsumeShogiStartPosition(m_view, m_bic);
}

void PositionEditController::onToggleSideToMoveTriggered()
{
    if (!m_gc) return;
    auto cur = m_gc->currentPlayer();
    m_gc->setCurrentPlayer(
        (cur == ShogiGameController::Player1)
            ? ShogiGameController::Player2
            : ShogiGameController::Player1);
    if (m_view) m_view->update();
}

// ======================================================================
// 編集中の着手適用
// ======================================================================

bool PositionEditController::applyEditMove(const QPoint& from,
                                           const QPoint& to,
                                           ShogiView* view,
                                           ShogiGameController* gc,
                                           BoardInteractionController* bic)
{
    if (!view || !gc || !bic) return false;

    QPoint hFrom = from, hTo = to;
    const bool ok = gc->editPosition(hFrom, hTo);

    // UI 後処理（ドラッグ終了／結果通知）
    view->endDrag();
    bic->onMoveApplied(hFrom, hTo, ok);

    if (ok) {
        view->update();
    }
    return ok;
}
