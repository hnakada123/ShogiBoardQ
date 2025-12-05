#include "boardinteractioncontroller.h"

#include "shogiview.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"          // board()->getPieceCharacter(), getPieceStand()
#include <QColor>
#include <QClipboard>
#include <QApplication>
#include <QDebug>

// ======================= ctor =======================
BoardInteractionController::BoardInteractionController(ShogiView* view,
                                                       ShogiGameController* gc,
                                                       QObject* parent)
    : QObject(parent), m_view(view), m_gc(gc)
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_gc);
}

// ===================== public slots =====================

void BoardInteractionController::onLeftClick(const QPoint& pt)
{
    // --- 1) “ドラッグ開始/終了”の 2クリック制御（元 onShogiViewClicked） ---
    // 駒台クリック時：枚数0を弾く（1stクリックのときのみ）
    if (!m_waitingSecondClick && (pt.x() == kBlackStandFile || pt.x() == kWhiteStandFile)) {
        const QChar piece = m_view->board()->getPieceCharacter(pt.x(), pt.y());
        if (m_view->board()->getPieceStand().value(piece) <= 0) {
            return; // 何も持ってないマスはドラッグさせない
        }
    }

    if (!m_waitingSecondClick) {
        // 1st クリック：選択＆ドラッグ開始
        m_firstClick = pt;
        m_view->startDrag(pt);
        m_waitingSecondClick = true;

        // 選択ハイライトを更新
        selectPieceAndHighlight(pt);
        emit selectionChanged(pt);
        return;
    }

    // 2nd クリック：同一マスならキャンセル扱い
    m_waitingSecondClick = false;
    if (pt == m_clickPoint) {
        finalizeDrag(); // ← ここでは従来通りキャンセル時のみドラッグ終了
        return;
    }

    // --- 2) モードに関わらず「移動要求」を発火（適用は呼び元） ---
    const QPoint from = m_clickPoint;
    const QPoint to   = pt;

    // 直前の赤ハイライトは、適用が成功したタイミングで描くためここでは触らない
    // ★重要★ ここではドラッグを終了しない。
    //         理由：成/不成ダイアログ表示中も “相手駒に重なった状態” を維持したい。
    //         ドラッグの終了は ShogiGameController 側で昇格判定後に endDragSignal を emit し、
    //         MainWindow::endDrag() 経由で ShogiView::endDrag() が呼ばれて行われる。
    emit moveRequested(from, to);
}

void BoardInteractionController::onRightClick(const QPoint& pt)
{
    // 2nd クリック待ち中ならキャンセル（元 onShogiViewRightClicked）
    if (m_waitingSecondClick) {
        finalizeDrag();
        return;
    }

    // Edit モード時のみ成/不成トグル
    if (m_mode == Mode::Edit) {
        togglePiecePromotionOnClick(pt);
    }
}

void BoardInteractionController::onMoveApplied(const QPoint& from, const QPoint& to, bool success)
{
    // 呼び元（Main/Coordinator）が合法判定＆適用後に呼ぶ
    if (!success) {
        // 非合法：選択とドラッグ状態をクリア
        finalizeDrag();
        return;
    }

    // 合法：ハイライト更新（元 addMoveHighlights と同等の見た目に）
    // 「直前の移動元」を薄い赤、「移動先」を黄色
    // 既存の“前回移動先/移動元2”も整合のため更新/クリア
    if (m_selectedField2) deleteHighlight(m_selectedField2);
    addNewHighlight(m_selectedField2, from, QColor(255, 0, 0, 50)); // 赤

    updateHighlight(m_movedField, to, Qt::yellow);                  // 黄

    // クリック状態のリセット（次の手の入力へ）
    resetSelectionAndHighlight(); // ← 選択（オレンジ）は消すが、直前手の赤/黄は残す
}

void BoardInteractionController::showMoveHighlights(const QPoint& from, const QPoint& to)
{
    // 局面移動で「この手」を強調したいとき
    // from が有効な座標かチェック（盤上: 1-9, 駒台: 10-11）
    const bool validFrom = (from.x() >= 1 && from.x() <= 11 && from.y() >= 1 && from.y() <= 9);
    if (validFrom) {
        updateHighlight(m_selectedField2, from, QColor(255, 0, 0, 50));
    } else {
        // 無効な座標の場合（駒打ちで座標が取得できなかった場合など）は移動元ハイライトを削除
        deleteHighlight(m_selectedField2);
    }
    updateHighlight(m_movedField, to, Qt::yellow);
}

void BoardInteractionController::clearAllHighlights()
{
    deleteHighlight(m_selectedField);
    deleteHighlight(m_selectedField2);
    deleteHighlight(m_movedField);
}

void BoardInteractionController::clearSelectionHighlight()
{
    // resetSelectionAndHighlight() と同義だが公開メソッドとして提供
    m_clickPoint = QPoint();
    deleteHighlight(m_selectedField); // 選択（オレンジ）だけ消す
    // m_selectedField2（赤）と m_movedField（黄）は残す
}

// ===================== private helpers =====================

void BoardInteractionController::selectPieceAndHighlight(const QPoint& field)
{
    auto* board = m_view->board();
    const int file = field.x();
    const int rank = field.y();
    const QChar value = board->getPieceCharacter(file, rank);

    // 盤編集モードでないとき、相手の駒台は触らせない
    if (!m_view->positionEditMode()) {
        const bool p1Turn = (m_gc->currentPlayer() == ShogiGameController::Player1);
        const bool isMyStand = ( (p1Turn && file == kBlackStandFile) ||
                                (!p1Turn && file == kWhiteStandFile) );
        if ((file >= kBlackStandFile) && !isMyStand) {
            finalizeDrag();
            return; // 相手の駒台は無視
        }
    }

    // 駒台：枚数0は無視
    const bool isStand = (file == kBlackStandFile || file == kWhiteStandFile);
    if (isStand && board->getPieceStand().value(value) <= 0) return;

    // 盤上の空白は選択しない
    if (value == QChar(' ')) return;

    // 選択状態を保持＆ハイライト（オレンジ）
    m_clickPoint = field;

    if (m_selectedField) deleteHighlight(m_selectedField);
    m_selectedField = new ShogiView::FieldHighlight(file, rank, QColor(255, 128, 0, 70));
    m_view->addHighlight(m_selectedField);
}

void BoardInteractionController::updateHighlight(ShogiView::FieldHighlight*& hl,
                                                 const QPoint& field,
                                                 const QColor& color)
{
    if (hl) deleteHighlight(hl);
    addNewHighlight(hl, field, color);
}

void BoardInteractionController::deleteHighlight(ShogiView::FieldHighlight*& hl)
{
    if (!hl) return;
    m_view->removeHighlight(hl);
    delete hl;
    hl = nullptr;
}

void BoardInteractionController::addNewHighlight(ShogiView::FieldHighlight*& hl,
                                                 const QPoint& pos,
                                                 const QColor& color)
{
    hl = new ShogiView::FieldHighlight(pos.x(), pos.y(), color);
    m_view->addHighlight(hl);
}

void BoardInteractionController::resetSelectionAndHighlight()
{
    m_clickPoint = QPoint();
    deleteHighlight(m_selectedField); // 選択（オレンジ）は消す
    // m_selectedField2（赤）と m_movedField（黄）は残す
}

void BoardInteractionController::finalizeDrag()
{
    m_view->endDrag();
    m_waitingSecondClick = false;
    resetSelectionAndHighlight();
}

void BoardInteractionController::togglePiecePromotionOnClick(const QPoint& field)
{
    // 右クリック → 成/不成トグル（駒台の駒は成れない）
    auto* board = m_view->board();
    const int file = field.x();
    const int rank = field.y();

    // 盤外（駒台）や空白は何もしない
    if (file >= kBlackStandFile) return;
    if (board->getPieceCharacter(file, rank) == QChar(' ')) return;

    // 成/不成の切り替え
    m_gc->switchPiecePromotionStatusOnRightClick(file, rank);

    // 選択ハイライトをリセット
    resetSelectionAndHighlight();
}
