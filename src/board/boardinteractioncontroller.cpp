/// @file boardinteractioncontroller.cpp
/// @brief 盤面上のクリック・ドラッグ操作とハイライト管理の実装

#include "boardinteractioncontroller.h"
#include "boardcoordinates.h"
#include "boardinputroute.h"
#include "positioneditrules.h"

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "shogitypes.h"
#include "appsettings.h"
#include "enginemovevalidator.h"
#include "legalmovestatus.h"
#include "logcategories.h"

#include <utility>

// ======================================================================
// 初期化
// ======================================================================

BoardInteractionController::BoardInteractionController(ShogiView* view, ShogiBoard* board, QObject* parent)
    : QObject(parent), m_view(view), m_board(board)
{
    if (Q_UNLIKELY(!m_view) || Q_UNLIKELY(!m_board)) {
        qCWarning(lcBoard, "BoardInteractionController: required dependency is null (view=%p, board=%p)",
                  static_cast<void*>(m_view), static_cast<void*>(m_board));
        return;
    }

    // 盤は起動から終了まで1つ（表示の盤と同じ）。手番が替わった（指した・手番だけを変えた）・局面を置き換えたら、
    // つまんでいた駒と行き先の表示をやめる。局面が変わったら行き先の表示だけを消す
    connect(m_board, &ShogiBoard::turnChanged, this, &BoardInteractionController::cancelPendingClick);
    connect(m_board, &ShogiBoard::boardReset, this, &BoardInteractionController::cancelPendingClick);
    connect(m_board, &ShogiBoard::dataChanged, this, &BoardInteractionController::clearLegalMoveHighlights);
}

void BoardInteractionController::setInputRoute(BoardInputRoute* route)
{
    if (m_route != route) cancelPendingClick();
    m_route = route;
}

void BoardInteractionController::setPromotionChooser(PromotionChooser chooser)
{
    m_promotionChooser = std::move(chooser);
}

std::optional<UsiMove> BoardInteractionController::moveFromClick(const QPoint& from, const QPoint& to) const
{
    if (!m_board) return std::nullopt;
    const Turn mover = m_board->currentPlayer();
    const bool onBoard = from.x() >= 1 && from.x() <= BoardConstants::kBoardSize
        && from.y() >= 1 && from.y() <= BoardConstants::kBoardSize;
    // 駒台から打てるのは手番の側の駒台の駒（玉は打てない。並びは BoardCoordinates）
    const int ownStand = mover == Turn::Black ? kBlackStandFile : kWhiteStandFile;
    const Piece standPiece =
        from.x() == ownStand ? BoardCoordinates::standPieceAt(from.x(), from.y()) : Piece::None;
    const bool fromHand = standPiece != Piece::None && pieceKind(standPiece) != PieceKind::King;
    const std::optional<UsiMove> candidate =
        (onBoard || fromHand) ? BoardCoordinates::usiMoveFromSquares(from, to, false) : std::nullopt;
    if (!candidate) {
        qCDebug(lcBoard) << "moveFromClick: not a move" << from << to;
        return std::nullopt;
    }
    if (!candidate->isDrop() && m_board->pieceAt(candidate->from) == Piece::None) {
        qCDebug(lcBoard) << "moveFromClick: the source square is empty" << from;
        return std::nullopt;
    }

    // 成り・不成のどちらが合法かで決める。両方とも合法なら対局者に選ばせる
    const LegalMoveStatus status = EngineMoveValidator::legalStatus(m_board->position(), mover, *candidate);
    UsiMove move = *candidate;
    if (status.promotingMoveExists && status.nonPromotingMoveExists) {
        move.promote = m_promotionChooser ? m_promotionChooser() : false;
    } else if (status.promotingMoveExists) {
        move.promote = true;
    } else if (!status.nonPromotingMoveExists) {
        return std::nullopt;
    }
    return move;
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
        const Piece piece = m_board->pieceCharacter(pt.x(), pt.y());
        if (!m_board->isPieceAvailableOnStand(piece, pt.x())) {
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

    // 成・不成を尋ねている間（行き先が moveFromClick で尋ねる）もドラッグの表示を保ち、行き先から戻ったら終える
    m_route->submit(from, to);
    m_view->endDrag();
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
    auto* board = m_board;
    const int file = field.x();
    const int rank = field.y();
    const Piece value = board->pieceCharacter(file, rank);

    // 局面編集でないとき、相手の駒台は触らせない
    if (!isEditing()) {
        const Turn turn = m_board->currentPlayer();
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
    if (!m_legalMovesVisible || !m_waitingSecondClick || m_clickPoint.isNull() || !m_view || !m_board
        || isEditing() || !acceptsInput()) return;

    const auto position = EngineMoveValidator::load(m_board->position(), m_board->currentPlayer());
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
    auto* board = m_board;
    const int file = field.x();
    const int rank = field.y();

    // 盤外（駒台）や空白は何もしない
    if (file >= kBlackStandFile) return;
    if (board->pieceCharacter(file, rank) == Piece::None) return;

    // 二歩や行き所のない段で成れない形は飛ばす（局面編集の規則）
    PositionEditRules::cyclePiece(*m_board, BoardSquare{file, rank});

    clearSelectionHighlight();
}
