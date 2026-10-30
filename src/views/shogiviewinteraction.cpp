/// @file shogiviewinteraction.cpp
/// @brief 将棋盤面のドラッグ操作の状態と持ち上げた駒の描画を担うクラスの実装

#include "shogiviewinteraction.h"
#include "piecepainter.h"
#include "shogiboard.h"
#include "boardconstants.h"

#include <QPainter>

ShogiViewInteraction::ShogiViewInteraction() {}

// ─────────────────────────── ドラッグ操作 ───────────────────────────

// ドラッグ操作の開始処理。
// 役割：
//  - つまみ上げ元（from）の位置からドラッグ対象の駒文字を取得し、ドラッグ用の一時状態をセット
//  - 駒台（file=10/11）からのドラッグでは在庫（枚数）が 1 以上あることを確認してから開始
//  - 駒台表示更新のため、一時カウント（m_tempPieceStandCounts）を作成し、つまみ上げた分を減算
void ShogiViewInteraction::startDrag(const QPoint &from, ShogiBoard* board,
                                      const QPoint& cursorWidgetPos, Turn pieceBoxSide)
{
    if (!board) return;

    // 【駒台からのドラッグ可否チェック】
    // file=10/11 は駒台。対象駒の在庫が 0 以下ならドラッグ開始しない。
    if (from.x() >= BoardConstants::kBlackStandFile) {
        Piece piece = board->pieceCharacter(from.x(), from.y());
        if (!board->isPieceAvailableOnStand(piece, from.x())) return;
    }

    // 【ドラッグ状態の確立】
    m_dragging  = true;                               // ドラッグ中フラグ
    m_dragFrom  = from;                               // つまみ上げ元（盤/駒台の座標）
    m_dragPiece = board->pieceCharacter(from.x(), from.y()); // 対象駒
    if (from.x() == BoardConstants::kPieceBoxFile && pieceBoxSide == Turn::White)
        m_dragPiece = toWhite(m_dragPiece);
    m_dragPos   = cursorWidgetPos;                    // 現在のポインタ位置（ウィジェット座標）

    // 【駒台の一時枚数マップを作成】
    // 画面上はドラッグで 1 枚減った見え方にするため、つまみ上げた分をデクリメント。
    m_tempPieceStandCounts.clear();
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (int kind = 0; kind < kHandKindCount; ++kind) {
            const Piece piece = makePiece(side, static_cast<PieceKind>(kind));
            m_tempPieceStandCounts.insert(piece, board->pieceStandCount(piece));
        }
    }
    if (from.x() == 10 || from.x() == 11) {
        m_tempPieceStandCounts[m_dragPiece]--;        // 一時的に在庫を減らす
    }
}

// ドラッグ操作の終了/キャンセル処理。
// 役割：ドラッグ中フラグを落とし、一時枚数マップを破棄して表示を元に戻す。
// 備考：実際のドロップ適用（駒の移動/配置反映）は、呼び出し側のロジックで行う想定。
void ShogiViewInteraction::endDrag()
{
    m_dragging = false;                // ドラッグ終了
    m_tempPieceStandCounts.clear();    // 一時的な駒台枚数をクリア（表示を通常状態へ）
}

void ShogiViewInteraction::setBoxDragSide(Turn side)
{
    if (m_dragging && m_dragFrom.x() == BoardConstants::kPieceBoxFile)
        m_dragPiece = side == Turn::Black ? toBlack(m_dragPiece) : toWhite(m_dragPiece);
}

// 【ドラッグ中の駒を描画】
// 方針：paintEvent で開始済みの QPainter を使い回すため、ここで新たに QPainter を生成しない。
// 前提：本関数は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さない。
// 備考：ドラッグ位置 m_dragPos の中心に、1マス分でアイコンを描画する。
void ShogiViewInteraction::drawDraggingPiece(QPainter& painter, QSize field,
                                              const QIcon& icon, const BoardVisuals& visuals) const
{
    // 【前提確認】ドラッグ中でなければ何もしない／駒種が空白・画像が無ければ描かない
    if (!m_dragging || m_dragPiece == Piece::None || icon.isNull()) return;

    // 【描画】既存の painter を用いて中央揃えでペイント（状態は汚さない）
    PiecePainter::draw(painter, icon, draggingCellRect(field), visuals);
}

QRect ShogiViewInteraction::draggingPieceRect(QSize field, const BoardVisuals& visuals) const
{
    if (!m_dragging || m_dragPiece == Piece::None) return {};
    // 端数の座標は外側へ丸め、さらに1ピクセル広げて描き残しを防ぐ。
    return PiecePainter::paintedRect(draggingCellRect(field), visuals)
        .toAlignedRect().adjusted(-1, -1, 1, 1);
}

// ドラッグ座標を中心に据えた1マス分の矩形（縦長マス）
QRect ShogiViewInteraction::draggingCellRect(QSize field) const
{
    return {m_dragPos.x() - field.width() / 2, m_dragPos.y() - field.height() / 2,
            field.width(), field.height()};
}

// ─────────────────────────── ドラッグ位置更新 ───────────────────────

void ShogiViewInteraction::updateDragPos(const QPoint& pos)
{
    m_dragPos = pos;
}

// ─────────────────────────── モード設定 ─────────────────────────

// 局面編集モードフラグのセッター。
// 注意：UI レイアウト更新は ShogiView 側の責務。本クラスはフラグ管理のみ。
void ShogiViewInteraction::setPositionEditMode(bool positionEditMode)
{
    m_positionEditMode = positionEditMode;
}
