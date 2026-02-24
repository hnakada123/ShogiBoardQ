/// @file shogiviewinteraction.cpp
/// @brief 将棋盤面のマウス操作・ドラッグの責務を担うクラスの実装

#include "shogiviewinteraction.h"
#include "shogiviewlayout.h"
#include "shogiboard.h"

#include <QPainter>
#include <QtMath>

ShogiViewInteraction::ShogiViewInteraction() {}

// ─────────────────────────── 座標変換 ───────────────────────────

// 反転/非反転に応じて適切な座標変換を呼び出すエントリポイント。
// 備考：反転判定以外のロジック（駒台判定、成→未成の正規化など）は
//       getClickedSquareIn*State() 側で行う。
QPoint ShogiViewInteraction::getClickedSquare(const QPoint &clickPosition,
                                               const ShogiViewLayout& layout,
                                               ShogiBoard* board) const
{
    // 【反転時】上下左右が入れ替わるため、反転用の座標変換を使用
    if (layout.flipMode()) {
        return getClickedSquareInFlippedState(clickPosition, layout, board);
    } else {
        // 【通常時】デフォルトの座標変換を使用
        return getClickedSquareInDefaultState(clickPosition, layout, board);
    }
}

QPoint ShogiViewInteraction::getClickedSquareInDefaultState(const QPoint& pos,
                                                             const ShogiViewLayout& layout,
                                                             ShogiBoard* board) const
{
    // 成駒を元の駒種に正規化（P/L/N/S/B/R/K/G の小文字を返す）
    auto baseFrom = [](QChar ch) -> QChar {
        const QChar l = ch.toLower();
        if      (l == 'q') return QChar('p'); // と金 -> 歩
        else if (l == 'm') return QChar('l'); // 成香 -> 香
        else if (l == 'o') return QChar('n'); // 成桂 -> 桂
        else if (l == 't') return QChar('s'); // 成銀 -> 銀
        else if (l == 'c') return QChar('b'); // 馬   -> 角
        else if (l == 'u') return QChar('r'); // 龍   -> 飛
        return l; // 金/玉/未成はそのまま
    };

    if (!board) return QPoint();

    const QSize fs = layout.fieldSize().isValid()
                         ? layout.fieldSize()
                         : QSize(layout.squareSize(),
                                 qRound(layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
    const int w = fs.width();
    const int h = fs.height();

    // 盤の外形
    const QRect boardRect(layout.offsetX(), layout.offsetY(),
                          w * board->files(), h * board->ranks());

    // 1) 盤内クリック（デフォルト向き）
    if (boardRect.contains(pos)) {
        const int xIn = pos.x() - boardRect.left();
        const int yIn = pos.y() - boardRect.top();
        const int colFromLeft = xIn / w;             // 0..8
        const int rowFromTop  = yIn / h;             // 0..8
        const int file = board->files() - colFromLeft; // 9..1
        const int rank = rowFromTop + 1;                 // 1..9
        return QPoint(file, rank);
    }

    // 2) 駒台の外接矩形（正しい左右・上下に修正）
    //   先手（file=10）：右側 layout.param2() 基準、下帯 rank 5..8
    const QRect senteStandRect(layout.offsetX() + layout.param2(),
                               layout.offsetY() + 5 * h, 2 * w, 4 * h);
    //   後手（file=11）：左側 layout.param1() 基準、上帯 rank 0..3
    const QRect goteStandRect (layout.offsetX() - layout.param1(),
                              layout.offsetY() + 0 * h, 2 * w, 4 * h);

    // 3) 局面編集モード中のドラッグドロップ先（駒種でセル固定）
    if (m_positionEditMode && m_dragging) {
        if (senteStandRect.contains(pos)) {
            const QChar p = pieceToChar(m_dragPiece);
            if (p != QChar(' ')) {
                const QChar up = baseFrom(p).toUpper(); // 成→未成→大文字
                if      (up == 'P') return QPoint(10, 1);
                else if (up == 'L') return QPoint(10, 2);
                else if (up == 'N') return QPoint(10, 3);
                else if (up == 'S') return QPoint(10, 4);
                else if (up == 'G') return QPoint(10, 5);
                else if (up == 'B') return QPoint(10, 6);
                else if (up == 'R') return QPoint(10, 7);
                else if (up == 'K') return QPoint(10, 8);
            }
            return QPoint();
        }
        if (goteStandRect.contains(pos)) {
            const QChar p = pieceToChar(m_dragPiece);
            if (p != QChar(' ')) {
                const QChar low = baseFrom(p); // 成→未成（小文字）
                if      (low == 'p') return QPoint(11, 9);
                else if (low == 'l') return QPoint(11, 8);
                else if (low == 'n') return QPoint(11, 7);
                else if (low == 's') return QPoint(11, 6);
                else if (low == 'g') return QPoint(11, 5);
                else if (low == 'b') return QPoint(11, 4);
                else if (low == 'r') return QPoint(11, 3);
                else if (low == 'k') return QPoint(11, 2);
            }
            return QPoint();
        }
        // 盤外かつ駒台外
        return QPoint();
    }

    // 4) （1stクリック）従来の駒台セル判定はそのまま
    {
        // 先手（右側＝layout.param2() 側） rank 5..8
        float tempFile = static_cast<float>(pos.x() - layout.param2() - layout.offsetX()) / static_cast<float>(w);
        float tempRank = static_cast<float>(pos.y() - layout.offsetY()) / static_cast<float>(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 0) {
            if (rank == 8) return QPoint(10, 1); // 歩 P
            if (rank == 7) return QPoint(10, 3); // 桂 N
            if (rank == 6) return QPoint(10, 5); // 金 G
            if (rank == 5) return QPoint(10, 7); // 飛 R
        } else if (file == 1) {
            if (rank == 8) return QPoint(10, 2); // 香 L
            if (rank == 7) return QPoint(10, 4); // 銀 S
            if (rank == 6) return QPoint(10, 6); // 角 B
            if (rank == 5) return QPoint(10, 8); // 玉 K
        }
    }
    {
        // 後手（左側＝layout.param1() 側） rank 0..3
        float tempFile = static_cast<float>(pos.x() + layout.param1() - layout.offsetX()) / static_cast<float>(w);
        float tempRank = static_cast<float>(pos.y() - layout.offsetY()) / static_cast<float>(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 1) {
            if (rank == 0) return QPoint(11, 9); // 歩 p
            if (rank == 1) return QPoint(11, 7); // 桂 n
            if (rank == 2) return QPoint(11, 5); // 金 g
            if (rank == 3) return QPoint(11, 3); // 飛 r
        } else if (file == 0) {
            if (rank == 0) return QPoint(11, 8); // 香 l
            if (rank == 1) return QPoint(11, 6); // 銀 s
            if (rank == 2) return QPoint(11, 4); // 角 b
            if (rank == 3) return QPoint(11, 2); // 玉 k
        }
    }

    return QPoint();
}

QPoint ShogiViewInteraction::getClickedSquareInFlippedState(const QPoint& pos,
                                                             const ShogiViewLayout& layout,
                                                             ShogiBoard* board) const
{
    auto baseFrom = [](QChar ch) -> QChar {
        const QChar l = ch.toLower();
        if      (l == 'q') return QChar('p');
        else if (l == 'm') return QChar('l');
        else if (l == 'o') return QChar('n');
        else if (l == 't') return QChar('s');
        else if (l == 'c') return QChar('b');
        else if (l == 'u') return QChar('r');
        return l;
    };

    if (!board) return QPoint();

    const QSize fs = layout.fieldSize().isValid()
                         ? layout.fieldSize()
                         : QSize(layout.squareSize(),
                                 qRound(layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
    const int w = fs.width();
    const int h = fs.height();

    const QRect boardRect(layout.offsetX(), layout.offsetY(),
                          w * board->files(), h * board->ranks());

    // 1) 盤内クリック（反転：file 左→右、rank 上→下で逆）
    if (boardRect.contains(pos)) {
        const int xIn = pos.x() - boardRect.left();
        const int yIn = pos.y() - boardRect.top();
        const int colFromLeft = xIn / w;
        const int rowFromTop  = yIn / h;

        const int file = colFromLeft + 1;               // 1..9
        const int rank = board->ranks() - rowFromTop; // 9..1
        return QPoint(file, rank);
    }

    // 2) 駒台の外接矩形（反転時は先手が左側、後手が右側）
    //   先手（file=10）：左側 layout.param1() 基準、上帯 rank 0..3
    const QRect senteStandRect(layout.offsetX() - layout.param1(),
                               layout.offsetY() + 0 * h, 2 * w, 4 * h);
    //   後手（file=11）：右側 layout.param2() 基準、下帯 rank 5..8
    const QRect goteStandRect (layout.offsetX() + layout.param2(),
                               layout.offsetY() + 5 * h, 2 * w, 4 * h);

    // 3) ドラッグ中の駒台ドロップ
    if (m_positionEditMode && m_dragging) {
        if (senteStandRect.contains(pos)) {
            const QChar p = pieceToChar(m_dragPiece);
            if (p != QChar(' ')) {
                const QChar up = baseFrom(p).toUpper();
                if      (up == 'P') return QPoint(10, 1);
                else if (up == 'L') return QPoint(10, 2);
                else if (up == 'N') return QPoint(10, 3);
                else if (up == 'S') return QPoint(10, 4);
                else if (up == 'G') return QPoint(10, 5);
                else if (up == 'B') return QPoint(10, 6);
                else if (up == 'R') return QPoint(10, 7);
                else if (up == 'K') return QPoint(10, 8);
            }
            return QPoint();
        }
        if (goteStandRect.contains(pos)) {
            const QChar p = pieceToChar(m_dragPiece);
            if (p != QChar(' ')) {
                const QChar low = baseFrom(p);
                if      (low == 'p') return QPoint(11, 9);
                else if (low == 'l') return QPoint(11, 8);
                else if (low == 'n') return QPoint(11, 7);
                else if (low == 's') return QPoint(11, 6);
                else if (low == 'g') return QPoint(11, 5);
                else if (low == 'b') return QPoint(11, 4);
                else if (low == 'r') return QPoint(11, 3);
                else if (low == 'k') return QPoint(11, 2);
            }
            return QPoint();
        }
        return QPoint();
    }

    // 4) （1stクリック）反転時の駒台セル判定
    //    反転時は先手駒台が左側（layout.param1()使用）、後手駒台が右側（layout.param2()使用）
    {
        // 先手駒台（左側＝layout.param1() 側） rank 0..3
        float tempFile = static_cast<float>(pos.x() + layout.param1() - layout.offsetX()) / static_cast<float>(w);
        float tempRank = static_cast<float>(pos.y() - layout.offsetY()) / static_cast<float>(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 1) {
            if (rank == 0) return QPoint(10, 1); // 歩 P
            if (rank == 1) return QPoint(10, 3); // 桂 N
            if (rank == 2) return QPoint(10, 5); // 金 G
            if (rank == 3) return QPoint(10, 7); // 飛 R
        } else if (file == 0) {
            if (rank == 0) return QPoint(10, 2); // 香 L
            if (rank == 1) return QPoint(10, 4); // 銀 S
            if (rank == 2) return QPoint(10, 6); // 角 B
            if (rank == 3) return QPoint(10, 8); // 玉 K
        }
    }
    {
        // 後手駒台（右側＝layout.param2() 側） rank 5..8
        float tempFile = static_cast<float>(pos.x() - layout.param2() - layout.offsetX()) / static_cast<float>(w);
        float tempRank = static_cast<float>(pos.y() - layout.offsetY()) / static_cast<float>(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 0) {
            if (rank == 8) return QPoint(11, 9); // 歩 p
            if (rank == 7) return QPoint(11, 7); // 桂 n
            if (rank == 6) return QPoint(11, 5); // 金 g
            if (rank == 5) return QPoint(11, 3); // 飛 r
        } else if (file == 1) {
            if (rank == 8) return QPoint(11, 8); // 香 l
            if (rank == 7) return QPoint(11, 6); // 銀 s
            if (rank == 6) return QPoint(11, 4); // 角 b
            if (rank == 5) return QPoint(11, 2); // 玉 k
        }
    }

    return QPoint();
}

// ─────────────────────────── ドラッグ操作 ───────────────────────────

// ドラッグ操作の開始処理。
// 役割：
//  - つまみ上げ元（from）の位置からドラッグ対象の駒文字を取得し、ドラッグ用の一時状態をセット
//  - 駒台（file=10/11）からのドラッグでは在庫（枚数）が 1 以上あることを確認してから開始
//  - 駒台表示更新のため、一時カウント（m_tempPieceStandCounts）を作成し、つまみ上げた分を減算
void ShogiViewInteraction::startDrag(const QPoint &from, ShogiBoard* board,
                                      const QPoint& cursorWidgetPos)
{
    if (!board) return;

    // 【駒台からのドラッグ可否チェック】
    // file=10/11 は駒台。対象駒の在庫が 0 以下ならドラッグ開始しない。
    if ((from.x() == 10 || from.x() == 11)) {
        Piece piece = board->getPieceCharacter(from.x(), from.y());
        if (board->m_pieceStand.value(piece) <= 0) return;
    }

    // 【ドラッグ状態の確立】
    m_dragging  = true;                               // ドラッグ中フラグ
    m_dragFrom  = from;                               // つまみ上げ元（盤/駒台の座標）
    m_dragPiece = board->getPieceCharacter(from.x(), from.y()); // 対象駒
    m_dragPos   = cursorWidgetPos;                    // 現在のポインタ位置（ウィジェット座標）

    // 【駒台の一時枚数マップを作成】
    // 画面上はドラッグで 1 枚減った見え方にするため、つまみ上げた分をデクリメント。
    m_tempPieceStandCounts = board->m_pieceStand;
    if (from.x() == 10 || from.x() == 11) {
        m_dragFromStand = true;                       // 駒台からのドラッグ
        m_tempPieceStandCounts[m_dragPiece]--;        // 一時的に在庫を減らす
    } else {
        m_dragFromStand = false;                      // 盤上からのドラッグ
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

// 【ドラッグ中の駒を描画】
// 方針：paintEvent で開始済みの QPainter を使い回すため、ここで新たに QPainter を生成しない。
// 前提：本関数は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さない。
// 備考：ドラッグ位置 m_dragPos の中心に、1マス分でアイコンを描画する。
void ShogiViewInteraction::drawDraggingPiece(QPainter& painter,
                                              const ShogiViewLayout& layout,
                                              const QMap<QChar, QIcon>& pieces)
{
    // 【前提確認】ドラッグ中でなければ何もしない／駒種が空白なら描かない
    if (!m_dragging || m_dragPiece == Piece::None) return;

    // 【アイコン取得】該当駒のアイコンが無ければ描かない（安全弁）
    const QIcon icon = pieces.value(pieceToChar(m_dragPiece), QIcon());
    if (icon.isNull()) return;

    // 【描画矩形算出】ドラッグ座標を矩形の中心に据える（縦長マス）
    const QSize fs = layout.fieldSize();
    const QRect r(m_dragPos.x() - fs.width() / 2, m_dragPos.y() - fs.height() / 2,
                  fs.width(), fs.height());

    // 【描画】既存の painter を用いて中央揃えでペイント（状態は汚さない）
    icon.paint(&painter, r, Qt::AlignCenter);
}

// ─────────────────────────── ドラッグ位置更新 ───────────────────────

void ShogiViewInteraction::updateDragPos(const QPoint& pos)
{
    m_dragPos = pos;
}

// ─────────────────────────── モード設定 ─────────────────────────

// マウス操作の入力モードを設定するセッター。
// 役割：クリックでの選択/移動（true）か、それ以外の操作方針（false）かを切り替えるためのフラグ。
void ShogiViewInteraction::setMouseClickMode(bool mouseClickMode)
{
    m_mouseClickMode = mouseClickMode;
}

// 局面編集モードフラグのセッター。
// 注意：UI レイアウト更新は ShogiView 側の責務。本クラスはフラグ管理のみ。
void ShogiViewInteraction::setPositionEditMode(bool positionEditMode)
{
    m_positionEditMode = positionEditMode;
}
