#include "positioneditcontroller.h"

#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
// BoardInteractionController を使っていなければこの include は外してOK
#include "boardinteractioncontroller.h"

#include <QStringList>
#include <Qt>

namespace {

// "board turn stand ply" 形式の SFEN の手番だけを b/w に上書きし、手数を 1 にする
static QString forceTurnInSfen(const QString& sfen, QChar turnBW)
{
    QStringList toks = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (toks.size() >= 2) {
        toks[1] = QString(turnBW);
        if (toks.size() >= 4) toks[3] = QStringLiteral("1");
        return toks.join(QLatin1Char(' '));
    }
    return sfen;
}

static inline QString defaultInitialSfen()
{
    return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

} // namespace

void PositionEditController::beginPositionEditing(const BeginEditContext& c)
{
    if (!c.view || !c.gc) return;
    if (!c.view->board()) return;

    // 0) 現在の GUI 手番を取得（未設定は先手）
    ShogiGameController::Player preSide = c.gc->currentPlayer();
    if (preSide == ShogiGameController::NoPlayer) preSide = ShogiGameController::Player1;
    const QChar desiredTurn = (preSide == ShogiGameController::Player2) ? QLatin1Char('w')
                                                                        : QLatin1Char('b');

    // 1) 編集開始 SFEN
    QString baseSfen;
    if (c.sfenRecord && !c.sfenRecord->isEmpty()) {
        const int lastIdx = c.sfenRecord->size() - 1;
        int idx = -1;
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
        if (!c.startSfen.isEmpty())       baseSfen = c.startSfen;
        else if (!c.currentSfen.isEmpty()) baseSfen = c.currentSfen;
        else                                baseSfen = defaultInitialSfen();
    }

    // 2) 手番を現在の GUI 手番に合わせる
    baseSfen = forceTurnInSfen(baseSfen, desiredTurn);

    // 3) 盤へ適用し、編集モードへ
    ShogiBoard* board = c.view->board();
    board->setSfen(baseSfen);
    c.view->setPositionEditMode(true);
    c.view->setMouseClickMode(true);
    c.view->update();

    // 4) ハイライト等の初期化
    if (c.bic) {
        c.bic->setMode(BoardInteractionController::Mode::Edit);
        c.bic->clearAllHighlights();
    }

    // 5) GC の手番を盤に同期
    const QString bw = board->currentPlayer();
    c.gc->setCurrentPlayer(
        (bw == QLatin1String("w")) ? ShogiGameController::Player2
                                   : ShogiGameController::Player1);

    // 6) 「局面編集終了」ボタン表示（任意）
    if (c.onShowEditExitButton) c.onShowEditExitButton();
}

void PositionEditController::finishPositionEditing(const PositionEditController::FinishEditContext& c)
{
    if (!c.gc || !c.view || !c.view->board()) return;

    ShogiBoard* board = c.view->board();

    // 盤モデルから SFEN を再構成（手数は 1 に統一）
    const QString bw = board->currentPlayer();
    const QString sfenNow = QStringLiteral("%1 %2 %3 %4")
                                .arg(board->convertBoardToSfen(),
                                     bw,
                                     board->convertStandToSfen(),
                                     QString::number(1));

    // 盤面へ反映し、描画
    board->setSfen(sfenNow);
    c.view->applyBoardAndRender(board);

    // 編集結果を sfenRecord に書き戻す
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
        // 必要なら通常モードへ戻す: c.bic->setMode(BoardInteractionController::Mode::Normal);
    }
    if (c.onHideEditExitButton) c.onHideEditExitButton();
}
