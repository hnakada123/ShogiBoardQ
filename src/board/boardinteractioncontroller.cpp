/// @file boardinteractioncontroller.cpp
/// @brief 盤面上のクリック・ドラッグ操作とハイライト管理の実装

#include "boardinteractioncontroller.h"
#include "boardcoordinates.h"
#include "boardinputroute.h"

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogitypes.h"
#include "appsettings.h"
#include "enginemovevalidator.h"
#include "logcategories.h"

// ======================================================================
// 初期化
// ======================================================================

BoardInteractionController::BoardInteractionController(ShogiView* view,
                                                       ShogiGameController* gc,
                                                       QObject* parent)
    : QObject(parent), m_view(view), m_gc(gc)
{
    if (Q_UNLIKELY(!m_view) || Q_UNLIKELY(!m_gc)) {
        qCWarning(lcBoard, "BoardInteractionController: required dependency is null (view=%p, gc=%p)",
                  static_cast<void*>(m_view), static_cast<void*>(m_gc));
        return;
    }

    // 手番が替わったら（指した・手番だけを変えた）、つまんでいた駒と行き先の表示をやめる
    connect(m_gc->board(), &ShogiBoard::turnChanged, this, &BoardInteractionController::cancelPendingClick);
}

void BoardInteractionController::setInputRoute(BoardInputRoute* route)
{
    if (m_route != route) cancelPendingClick();
    m_route = route;
}

bool BoardInteractionController::acceptsInput() const
{
    return m_route && m_route->acceptsInput();
}

bool BoardInteractionController::isEditing() const
{
    return m_route && m_route->isEditing();
}

// ======================================================================
// 公開スロット
// ======================================================================

void BoardInteractionController::onLeftClick(const QPoint& pt)
{
    // 人間が指せる手番でなければ（行き先が受け付けなければ）クリックを無視する
    if (!acceptsInput()) {
        qCDebug(lcBoard) << "onLeftClick ignored: input not accepted";
        return;
    }

    // 駒台クリック時：枚数0を弾く（1stクリックのときのみ）
    if (!m_waitingSecondClick && pt.x() >= kBlackStandFile) {
        const Piece piece = m_view->board()->pieceCharacter(pt.x(), pt.y());
        if (!m_view->board()->isPieceAvailableOnStand(piece, pt.x())) {
            return; // 持ち駒がないマスはドラッグさせない
        }
    }

    if (!m_waitingSecondClick) {
        // 1stクリック：選択＆ドラッグ開始
        m_view->startDrag(pt);
        m_waitingSecondClick = true;

        selectPieceAndHighlight(pt);

        // 空白マス等で m_clickPoint が設定されなかった場合はドラッグをキャンセル
        if (m_clickPoint.isNull() || m_clickPoint != pt) {
            finalizeDrag();
            return;
        }

        return;
    }

    // 2ndクリック：同一マスならキャンセル扱い
    m_waitingSecondClick = false;
    if (pt == m_clickPoint) {
        finalizeDrag();
        return;
    }

    // 行き先へ渡す（指すのは行き先。結果は onMoveApplied で返る）
    const QPoint from = m_clickPoint;
    const QPoint to   = pt;
    clearLegalMoveHighlights();

    // 成/不成ダイアログ表示中もドラッグ状態を維持するため、ここではドラッグを終了しない。
    // ShogiGameController 側で昇格判定後に endDragSignal を emit し、
    // それが ShogiView::endDrag() につながっている（合成ルートが接続する）。
    m_route->submit(from, to);
}

void BoardInteractionController::onRightClick(const QPoint& pt)
{
    // 2ndクリック待ち中ならキャンセル
    if (m_waitingSecondClick) {
        finalizeDrag();
        return;
    }

    // 編集モード時のみ成/不成トグル
    if (isEditing()) togglePiecePromotionOnClick(pt);
}

void BoardInteractionController::onMoveApplied(const QPoint& from, const QPoint& to, bool success)
{
    if (!success) {
        // 非合法手：選択とドラッグ状態をクリア
        finalizeDrag();
        return;
    }

    // 合法手：直前の手の移動元・移動先を出し、選択は消す
    showMoveHighlights(from, to);
    clearSelectionHighlight();
}

void BoardInteractionController::showMoveHighlights(const QPoint& from, const QPoint& to)
{
    // from が有効な座標かチェック（盤上: 1-9, 駒台: 10-11）。駒打ちで座標が取れなければ移動元は出さない
    const bool validFrom = (from.x() >= 1 && from.x() <= 11 && from.y() >= 1 && from.y() <= 9);
    ShogiViewHighlighting* highlighting = m_view->highlighting();
    highlighting->setHighlights(BoardHighlightLayer::LastMoveFrom, validFrom ? QList<QPoint>{from} : QList<QPoint>{});
    highlighting->setHighlights(BoardHighlightLayer::LastMoveTo, {to});
}

void BoardInteractionController::clearAllHighlights()
{
    cancelPendingClick();
    m_view->highlighting()->setHighlights(BoardHighlightLayer::LastMoveFrom, {});
    m_view->highlighting()->setHighlights(BoardHighlightLayer::LastMoveTo, {});
}

void BoardInteractionController::cancelPendingClick()
{
    finalizeDrag();
}

void BoardInteractionController::clearSelectionHighlight()
{
    clearLegalMoveHighlights();
    m_waitingSecondClick = false;
    m_clickPoint = QPoint();
    // 選択だけ消し、直前の手のハイライトは残す
    m_view->highlighting()->setHighlights(BoardHighlightLayer::Selection, {});
}

// ======================================================================
// プライベートヘルパ
// ======================================================================

void BoardInteractionController::selectPieceAndHighlight(const QPoint& field)
{
    auto* board = m_view->board();
    const int file = field.x();
    const int rank = field.y();
    const Piece value = board->pieceCharacter(file, rank);

    // 局面編集でないとき、相手の駒台は触らせない
    if (!isEditing()) {
        const Turn turn = m_gc->currentPlayer();
        const bool isMyStand = (turn == Turn::Black && file == kBlackStandFile)
                               || (turn == Turn::White && file == kWhiteStandFile);
        if ((file >= kBlackStandFile) && !isMyStand) {
            m_clickPoint = QPoint();
            finalizeDrag();
            return;
        }
    }

    // 駒台：枚数0は無視
    const bool isStand = (file >= kBlackStandFile);
    if (isStand && !board->isPieceAvailableOnStand(value, file)) {
        m_clickPoint = QPoint();
        return;
    }

    // 盤上の空白は選択しない
    if (value == Piece::None) {
        m_clickPoint = QPoint();
        return;
    }

    // 選択状態を保持してハイライトする
    m_clickPoint = field;
    m_view->highlighting()->setHighlights(BoardHighlightLayer::Selection, {field});

    // 対局開始で盤モデルが差し替わるため、選択時に現在の盤へ接続する。
    connect(board, &ShogiBoard::boardReset,
            this, &BoardInteractionController::cancelPendingClick, Qt::UniqueConnection);
    connect(board, &ShogiBoard::dataChanged,
            this, &BoardInteractionController::clearLegalMoveHighlights, Qt::UniqueConnection);
    refreshLegalMoveHighlights();
}

void BoardInteractionController::setLegalMovesVisible(bool visible)
{
    m_legalMovesVisible = visible;
    AppSettings::legalMovesVisible.set(visible);
    refreshLegalMoveHighlights();
}

void BoardInteractionController::clearLegalMoveHighlights()
{
    if (m_view) m_view->highlighting()->setHighlights(BoardHighlightLayer::LegalDestinations, {});
}

void BoardInteractionController::refreshLegalMoveHighlights()
{
    clearLegalMoveHighlights();
    if (!m_legalMovesVisible || !m_waitingSecondClick || m_clickPoint.isNull() || !m_view || !m_view->board()
        || !m_gc || isEditing() || !acceptsInput()) return;

    auto* board = m_view->board();
    const auto position = EngineMoveValidator::load(board->position(), m_gc->currentPlayer());
    if (!position.synced) return;

    // 局面の読み込みは1回だけ行い、成り・不成のどちらかが合法なマスを表示する。
    QList<QPoint> destinations;
    for (int rank = 1; rank <= 9; ++rank) {
        for (int file = 1; file <= 9; ++file) {
            const auto move = BoardCoordinates::usiMoveFromSquares(m_clickPoint, QPoint(file, rank), false);
            if (!move) continue;
            const auto status = EngineMoveValidator::legalStatus(position, *move);
            if (status.nonPromotingMoveExists || status.promotingMoveExists) destinations.append(QPoint(file, rank));
        }
    }
    m_view->highlighting()->setHighlights(BoardHighlightLayer::LegalDestinations, destinations);
}

void BoardInteractionController::finalizeDrag()
{
    if (m_view) m_view->endDrag();
    m_waitingSecondClick = false;
    clearSelectionHighlight();
}

void BoardInteractionController::togglePiecePromotionOnClick(const QPoint& field)
{
    auto* board = m_view->board();
    const int file = field.x();
    const int rank = field.y();

    // 盤外（駒台）や空白は何もしない
    if (file >= kBlackStandFile) return;
    if (board->pieceCharacter(file, rank) == Piece::None) return;

    // 二歩や行き所のない段で成れない形は盤が飛ばす
    m_gc->board()->promoteOrDemotePiece(file, rank);

    clearSelectionHighlight();
}
