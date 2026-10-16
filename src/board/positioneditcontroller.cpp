/// @file positioneditcontroller.cpp
/// @brief 局面編集モードの開始・終了・盤操作の実装

#include "positioneditcontroller.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "boardinteractioncontroller.h"

#include <QPushButton>
#include <QStringList>
#include <QRegularExpression>
#include <Qt>

// ======================================================================
// 無名名前空間ヘルパ
// ======================================================================

namespace {

void resetEditInteraction(ShogiView* view, BoardInteractionController* bic)
{
    view->endDrag();
    view->highlighting()->removeHighlightAllData();
    if (bic) {
        bic->cancelPendingClick();
        bic->clearAllHighlights();
    }
}

/// 将棋の平手初期配置（board部分）
static constexpr char kInitialBoard[] =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL";
static constexpr char kInitialStand[] = "-";

/// 入力文字列（startpos / sfen ... / 最小SFEN / 不明形式）を最小SFENに正規化する
static QString toMinimalSfen(const QString& in)
{
    const QString s = in.trimmed();
    if (s.isEmpty()) {
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    const QStringList parts = s.split(whitespace, Qt::SkipEmptyParts);
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

} // namespace

// ======================================================================
// 編集ライフサイクル
// ======================================================================

bool PositionEditController::beginPositionEditing(const BeginEditContext& c)
{
    if (!c.view || !c.gc || !c.view->board()) return false;

    m_view = c.view;
    m_gc   = c.gc;
    m_bic  = c.bic;

    ShogiBoard* board = c.view->board();

    // 棋譜読込直後も、表示している盤面の手番を編集開始時に保持する。
    const QChar desiredTurn = board->currentPlayer() == Turn::White ? QLatin1Char('w') : QLatin1Char('b');

    // 表示中の局面を編集する
    QString baseSfen = c.displayedSfen;
    if (baseSfen.isEmpty()) {
        baseSfen = board->position().toSfen(board->currentPlayer(), 1);
    }

    // 最小SFENへ正規化し、手番/手数を強制上書き
    const QString minimal  = toMinimalSfen(baseSfen);
    const QString adjusted = forceTurnAndPly(minimal, desiredTurn, /*ply*/1);

    // 盤へ適用し、編集モードへ
    resetEditInteraction(c.view, c.bic);
    board->setSfen(adjusted);
    c.view->setPositionEditMode(true);
    c.view->update();

    // ハイライト等の初期化
    if (c.bic) {
        m_prevBoardMode = static_cast<int>(c.bic->mode());
        c.bic->setMode(BoardInteractionController::Mode::Edit);
        c.bic->clearAllHighlights();
    } else {
        m_prevBoardMode = -1;
    }
    return true;
}

QString PositionEditController::finishPositionEditing(const PositionEditController::FinishEditContext& c)
{
    if (!c.gc || !c.view || !c.view->board()) return {};

    ShogiBoard* board = c.view->board();

    // GC の手番（編集中に手番変更された値）で、編集した局面の SFEN にする
    const QString sfenNow = board->position().toSfen(c.gc->currentPlayer(), 1);

    // 盤面へ反映し、描画
    resetEditInteraction(c.view, c.bic);
    board->setSfen(sfenNow);
    c.view->setPositionEditMode(false);
    c.view->update();

    // UI 後片付け
    if (c.bic) {
        if (m_prevBoardMode >= 0) {
            c.bic->setMode(static_cast<BoardInteractionController::Mode>(m_prevBoardMode));
        } else {
            c.bic->setMode(BoardInteractionController::Mode::HumanVsHuman);
        }
        c.bic->clearAllHighlights();
    }

    // 編集セッションの参照を保持（アクション用スロットで利用）

    m_view = c.view;
    m_gc   = c.gc;
    m_bic  = c.bic;
    m_prevBoardMode = -1;
    return sfenNow;
}

// ======================================================================
// 盤面操作API
// ======================================================================

void PositionEditController::resetPiecesToBox(ShogiView* view, BoardInteractionController* bic)
{
    if (!view) return;
    resetEditInteraction(view, bic);
    view->board()->resetGameBoard();
}

void PositionEditController::setStandardStartPosition(ShogiView* view, BoardInteractionController* bic)
{
    if (!view) return;
    resetEditInteraction(view, bic);
    view->board()->setSfen(SfenUtils::hirateSfen());
}

void PositionEditController::setTsumeShogiStartPosition(ShogiView* view, BoardInteractionController* bic)
{
    if (!view) return;
    resetEditInteraction(view, bic);
    // 詰将棋の初期局面の見本（使わない駒は玉方の持駒）
    view->board()->setSfen(QStringLiteral("5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1"));
}

// ======================================================================
// 編集終了ボタン制御
// ======================================================================

void PositionEditController::showEditExitButtonOnBoard(ShogiView* view)
{
    if (!view) return;

    // ShogiView 側で作成&配置（必要なら内部で生成）
    view->relayoutEditExitButton();

    if (QPushButton* exitBtn = view->findChild<QPushButton*>(QStringLiteral("editExitButton"))) {
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

void PositionEditController::onReturnAllPiecesToBoxTriggered()
{
    if (!m_view) return;
    resetPiecesToBox(m_view, m_bic);
}

void PositionEditController::onFlatHandInitialPositionTriggered()
{
    if (!m_view) return;
    setStandardStartPosition(m_view, m_bic);
    if (m_gc) m_gc->setCurrentPlayer(Turn::Black);
}

void PositionEditController::onShogiProblemInitialPositionTriggered()
{
    if (!m_view) return;
    setTsumeShogiStartPosition(m_view, m_bic);
    if (m_gc) m_gc->setCurrentPlayer(Turn::Black);
}

void PositionEditController::onToggleSideToMoveTriggered()
{
    if (!m_gc) return;
    const auto next = (m_gc->currentPlayer() == Turn::Black)
                          ? Turn::White : Turn::Black;
    // 編集中の配置は再構築せず、手番だけ同期する。
    m_gc->setCurrentPlayer(next);
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
    const bool ok = gc->editPosition(hFrom, hTo, view->pieceBoxSide());

    // UI 後処理（ドラッグ終了／結果通知）。盤・駒台・駒箱の変化は ShogiBoard の通知で
    // 変わった所だけ描き直されるので、ここで盤全体は描き直さない。
    view->endDrag();
    bic->onMoveApplied(hFrom, hTo, ok);
    return ok;
}
