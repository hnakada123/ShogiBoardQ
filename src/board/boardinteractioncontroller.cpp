/// @file boardinteractioncontroller.cpp
/// @brief 盤面上のクリック・ドラッグ操作とハイライト管理の実装

#include "boardinteractioncontroller.h"

#include "shogiview.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include <QColor>
#include <QClipboard>
#include <QApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBoard, "shogi.board")

// ======================================================================
// 初期化
// ======================================================================

BoardInteractionController::BoardInteractionController(ShogiView* view,
                                                       ShogiGameController* gc,
                                                       QObject* parent)
    : QObject(parent), m_view(view), m_gc(gc)
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_gc);

    // ShogiView::removeHighlightAllData() が呼ばれたときに
    // 本クラスが保持するハイライトポインタをnullにする（ダングリングポインタ防止）
    connect(m_view, &ShogiView::highlightsCleared, this, &BoardInteractionController::onHighlightsCleared);
}

// ======================================================================
// 公開スロット
// ======================================================================

void BoardInteractionController::onLeftClick(const QPoint& pt)
{
    // 編集モード以外では、人間の手番かどうかをチェック
    // コールバックがtrueを返すと人間の手番、falseなら相手の手番なのでクリックを無視
    if (m_mode != Mode::Edit && m_isHumanTurnCb && !m_isHumanTurnCb()) {
        qCDebug(lcBoard) << "onLeftClick ignored: not human's turn";
        return;
    }

    // 駒台クリック時：枚数0を弾く（1stクリックのときのみ）
    if (!m_waitingSecondClick && (pt.x() == kBlackStandFile || pt.x() == kWhiteStandFile)) {
        const QChar piece = m_view->board()->getPieceCharacter(pt.x(), pt.y());
        if (m_view->board()->getPieceStand().value(piece) <= 0) {
            return; // 持ち駒がないマスはドラッグさせない
        }
    }

    if (!m_waitingSecondClick) {
        // 1stクリック：選択＆ドラッグ開始
        m_firstClick = pt;
        m_view->startDrag(pt);
        m_waitingSecondClick = true;

        selectPieceAndHighlight(pt);

        // 空白マス等で m_clickPoint が設定されなかった場合はドラッグをキャンセル
        if (m_clickPoint.isNull() || m_clickPoint != pt) {
            finalizeDrag();
            return;
        }

        emit selectionChanged(pt);
        return;
    }

    // 2ndクリック：同一マスならキャンセル扱い
    m_waitingSecondClick = false;
    if (pt == m_clickPoint) {
        finalizeDrag();
        return;
    }

    // モードに関わらず「移動要求」を発火（適用は呼び元が行う）
    const QPoint from = m_clickPoint;
    const QPoint to   = pt;

    // 成/不成ダイアログ表示中もドラッグ状態を維持するため、ここではドラッグを終了しない。
    // ShogiGameController 側で昇格判定後に endDragSignal を emit し、
    // MainWindow::endDrag() 経由で ShogiView::endDrag() が呼ばれる。
    emit moveRequested(from, to);
}

void BoardInteractionController::onRightClick(const QPoint& pt)
{
    // 2ndクリック待ち中ならキャンセル
    if (m_waitingSecondClick) {
        finalizeDrag();
        return;
    }

    // 編集モード時のみ成/不成トグル
    if (m_mode == Mode::Edit) {
        togglePiecePromotionOnClick(pt);
    }
}

void BoardInteractionController::onMoveApplied(const QPoint& from, const QPoint& to, bool success)
{
    if (!success) {
        // 非合法手：選択とドラッグ状態をクリア
        finalizeDrag();
        return;
    }

    // 合法手：直前の移動元を薄い赤、移動先を黄色でハイライト
    if (m_selectedField2) deleteHighlight(m_selectedField2);
    addNewHighlight(m_selectedField2, from, QColor(255, 0, 0, 50));

    updateHighlight(m_movedField, to, QColor(255, 255, 0));

    // 選択（オレンジ）は消すが、直前手の赤/黄は残す
    resetSelectionAndHighlight();
}

void BoardInteractionController::showMoveHighlights(const QPoint& from, const QPoint& to)
{
    // from が有効な座標かチェック（盤上: 1-9, 駒台: 10-11）
    const bool validFrom = (from.x() >= 1 && from.x() <= 11 && from.y() >= 1 && from.y() <= 9);
    if (validFrom) {
        updateHighlight(m_selectedField2, from, QColor(255, 0, 0, 50));
    } else {
        // 駒打ちで座標が取得できなかった場合など、移動元ハイライトを削除
        deleteHighlight(m_selectedField2);
    }
    updateHighlight(m_movedField, to, QColor(255, 255, 0));
}

void BoardInteractionController::clearAllHighlights()
{
    deleteHighlight(m_selectedField);
    deleteHighlight(m_selectedField2);
    deleteHighlight(m_movedField);
}

void BoardInteractionController::onHighlightsCleared()
{
    // ShogiView::removeHighlightAllData() が呼ばれた
    // 実体はすでに qDeleteAll で破棄されているのでポインタだけ null にする
    m_selectedField  = nullptr;
    m_selectedField2 = nullptr;
    m_movedField     = nullptr;
}

void BoardInteractionController::clearSelectionHighlight()
{
    m_clickPoint = QPoint();
    deleteHighlight(m_selectedField); // 選択（オレンジ）だけ消す
    // m_selectedField2（赤）と m_movedField（黄）は残す
}

// ======================================================================
// プライベートヘルパ
// ======================================================================

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
            m_clickPoint = QPoint();
            finalizeDrag();
            return;
        }
    }

    // 駒台：枚数0は無視
    const bool isStand = (file == kBlackStandFile || file == kWhiteStandFile);
    if (isStand && board->getPieceStand().value(value) <= 0) {
        m_clickPoint = QPoint();
        return;
    }

    // 盤上の空白は選択しない
    if (value == QChar(' ')) {
        m_clickPoint = QPoint();
        return;
    }

    // 選択状態を保持＆オレンジハイライト表示
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
    auto* board = m_view->board();
    const int file = field.x();
    const int rank = field.y();

    // 盤外（駒台）や空白は何もしない
    if (file >= kBlackStandFile) return;
    if (board->getPieceCharacter(file, rank) == QChar(' ')) return;

    m_gc->switchPiecePromotionStatusOnRightClick(file, rank);

    resetSelectionAndHighlight();
}
