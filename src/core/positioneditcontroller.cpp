#include "positioneditcontroller.h"

#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "boardinteractioncontroller.h"

#include <QStringList>
#include <QRegularExpression>
#include <Qt>

namespace {

// 将棋の平手初期配置（board部分）
static const QString kInitialBoard =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
static const QString kInitialStand = QStringLiteral("-");

// 入力文字列（startpos / sfen ... / すでに最小SFEN / それ以外）を最小SFENに正規化
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

    // 1) "startpos"（moves は編集開始では無視）
    if (parts[0] == QStringLiteral("startpos")) {
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    // 2) "sfen <board> <turn> <stand> <ply> (moves ...)?"
    if (parts[0] == QStringLiteral("sfen")) {
        if (parts.size() >= 5) {
            // moves は無視、最小SFENを返す
            return QStringLiteral("%1 %2 %3 %4").arg(parts[1], parts[2], parts[3], parts[4]);
        }
        // 壊れていれば平手へ
        return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
    }

    // 3) 既に最小SFEN（board に / を含む）
    if (parts.size() >= 4 && parts[0].contains(QLatin1Char('/'))) {
        return QStringLiteral("%1 %2 %3 %4").arg(parts[0], parts[1], parts[2], parts[3]);
    }

    // 4) 不明形式は平手へ
    return QStringLiteral("%1 %2 %3 %4").arg(kInitialBoard, "b", kInitialStand, "1");
}

// 最小SFEN（board turn stand ply）を受け取り、手番と手数を上書きして返す
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
    // Player1=先手(b), Player2=後手(w)
    return (p == ShogiGameController::Player2) ? QLatin1Char('w') : QLatin1Char('b');
}

static ShogiGameController::Player fromBW(const QString& bw)
{
    return (bw == QStringLiteral("w"))
    ? ShogiGameController::Player2
    : ShogiGameController::Player1;
}

} // namespace

void PositionEditController::beginPositionEditing(const BeginEditContext& c)
{
    if (!c.view || !c.gc) return;
    if (!c.view->board()) return;

    ShogiBoard* board = c.view->board();

    // 0) 既存 GC 手番から desiredTurn を決める（未設定なら先手扱い）
    ShogiGameController::Player preSide = c.gc->currentPlayer();
    if (preSide == ShogiGameController::NoPlayer) preSide = ShogiGameController::Player1;
    const QChar desiredTurn = toBW(preSide);

    // 1) 編集開始SFENの決定（record / current / resume / 盤 から）
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
        if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
            baseSfen = *c.currentSfenStr;
        } else if (c.resumeSfenStr && !c.resumeSfenStr->isEmpty()) {
            baseSfen = *c.resumeSfenStr;
        } else {
            // 盤の現在状態から最小SFENを組み立てる
            baseSfen = QStringLiteral("%1 %2 %3 %4")
                           .arg(board->convertBoardToSfen(),
                                board->currentPlayer(),
                                board->convertStandToSfen(),
                                QString::number(1));
        }
    }

    // ★ ここが今回の修正ポイント：
    //    startpos / sfen ... / 既に最小SFEN のいずれでも最小SFENに正規化してから
    //    手番と手数を強制上書きする
    const QString minimal = toMinimalSfen(baseSfen);
    const QString adjusted = forceTurnAndPly(minimal, desiredTurn, /*ply*/1);

    // 3) 盤へ適用し、編集モードへ
    board->setSfen(adjusted);
    c.view->setPositionEditMode(true);
    c.view->setMouseClickMode(true);
    c.view->update();

    // 4) ハイライト等の初期化
    if (c.bic) {
        c.bic->setMode(BoardInteractionController::Mode::Edit);
        c.bic->clearAllHighlights();
    }

    // 5) GC の手番を盤へ同期
    const QString bw = board->currentPlayer();
    c.gc->setCurrentPlayer(fromBW(bw));

    // 6) UI: 「編集終了」ボタン表示
    if (c.onShowEditExitButton) c.onShowEditExitButton();

    // 7) 任意: startSfenStr にも記録（必要なら）
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
}
