/// @file boardsyncpresenter.cpp
/// @brief 盤面同期プレゼンタクラスの実装

#include "boardsyncpresenter.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "boardinteractioncontroller.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "logcategories.h"
#include "sfendiffutils.h"

BoardSyncPresenter::BoardSyncPresenter(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_bic(d.bic)
    , m_sfenHistory(d.sfenRecord)
    , m_gameMoves(d.gameMoves)
{
}

void BoardSyncPresenter::applySfenAtPly(int ply) const
{
    // ガード＆早期リターン
    if (!m_sfenHistory || m_sfenHistory->isEmpty() || !m_gc || !m_gc->board()) {
        qCWarning(lcUi) << "applySfenAtPly guard failed:"
                   << "rec*=" << static_cast<const void*>(m_sfenHistory)
                   << "gc=" << m_gc;
        return;
    }

    const int size   = static_cast<int>(m_sfenHistory->size());
    const int maxIdx = size - 1;

    // 終局（投了など）行の判定：SFEN は増えないため reqPly > maxIdx になる
    const bool isTerminalRow = (ply > maxIdx);

    // 実際に適用するインデックスはクランプ（終局行なら常に末尾の SFEN を使う）
    const int idx = qBound(0, ply, maxIdx);

    // ローカルコピーで安全にアクセス（外部から QStringList が変更されても影響なし）
    const QString sfen = m_sfenHistory->at(idx);

    qCDebug(lcUi).noquote()
        << QString("applySfenAtPly reqPly=%1 idx=%2 size=%3 terminalRow=%4")
               .arg(ply).arg(idx).arg(size).arg(isTerminalRow);

    if (isTerminalRow && ply > size) {
        qCWarning(lcUi).noquote()
            << QString("TERMINAL-ROW anomaly: reqPly(%1) > size(%2).")
                    .arg(ply).arg(size);
    }

    if (sfen.startsWith(QLatin1String("position "))) {
        qCWarning(lcUi) << "NON-SFEN passed to presenter at idx=" << idx;
    }

    // 盤面適用
    m_gc->board()->setSfen(sfen);

    if (m_view) {
        m_view->applyBoardAndRender(m_gc->board());
    }
}

// 盤面・ハイライト同期（行 → 盤面）
void BoardSyncPresenter::syncBoardAndHighlightsAtRow(int ply) const
{
    // 依存チェック
    if (!m_sfenHistory || !m_gc || !m_gc->board()) {
        qCDebug(lcUi) << "syncBoardAndHighlightsAtRow ABORT: ply=" << ply;
        return;
    }

    const int size   = static_cast<int>(m_sfenHistory->size());
    const int maxIdx = size - 1;
    if (maxIdx < 0) {
        qCDebug(lcUi) << "syncBoardAndHighlightsAtRow: EMPTY sfenRecord ply=" << ply;
        return;
    }

    const bool isTerminalRow = (ply > maxIdx);
    const int  safePly       = qBound(0, ply, maxIdx);

    qCDebug(lcUi).noquote()
        << QString("syncBoardAndHighlightsAtRow ply=%1 safePly=%2 size=%3")
               .arg(ply).arg(safePly).arg(size);

    // 先に盤面を適用（applySfenAtPly 内でクランプされる）
    applySfenAtPly(ply);

    // ハイライト器（BIC）が無ければここで終了（盤面だけは更新済み）
    if (!m_bic) {
        qCDebug(lcUi).noquote() << "syncBoardAndHighlightsAtRow: no BIC; skip highlights";
        return;
    }

    // 開始局面（0手目）や終端行（投了等）の行はハイライト消去のみ
    if (safePly <= 0 || isTerminalRow) {
        qCDebug(lcUi).noquote() << "syncBoardAndHighlightsAtRow: clear highlights"
                           << " reason=" << (safePly<=0 ? "startpos" : "terminalRow");
        m_bic->clearAllHighlights();
        return;
    }

    // ======== ここからハイライト推定（SFEN差分） ========
    auto sfenAt = [&](int idx)->QString {
        if (!m_sfenHistory || idx < 0 || idx >= m_sfenHistory->size()) return QString();
        return m_sfenHistory->at(idx);
    };
    const QString prev = sfenAt(safePly - 1);
    const QString curr = sfenAt(safePly);

    // 駒種から駒台の疑似座標を取得（先手: file=10, 後手: file=11）
    // 駒種（大文字=先手, 小文字=後手）→ 疑似座標を返す
    auto pieceToStandCoord = [](Piece piece) -> QPoint {
        // 先手（大文字）の場合: file=10
        // 後手（小文字）の場合: file=11
        const bool black = isBlackPiece(piece);
        const int file = black ? 10 : 11;
        const Piece upper = toBlack(piece);

        // 駒種→rank のマッピング（shogiview.cpp の standPseudoToBase の逆引き）
        // 先手駒台（file=10）:
        //   右列(baseFile=1): K→8, B→6, S→4, L→2
        //   左列(baseFile=2): R→7, G→5, N→3, P→1
        // 後手駒台（file=11）:
        //   左列(baseFile=1): r→3, g→5, n→7, p→9
        //   右列(baseFile=2): k→2, b→4, s→6, l→8
        int rank = -1;
        if (black) {
            // 先手駒台
            switch (upper) {
            case Piece::BlackKing:   rank = 8; break;  // 玉
            case Piece::BlackRook:   rank = 7; break;  // 飛
            case Piece::BlackBishop: rank = 6; break;  // 角
            case Piece::BlackGold:   rank = 5; break;  // 金
            case Piece::BlackSilver: rank = 4; break;  // 銀
            case Piece::BlackKnight: rank = 3; break;  // 桂
            case Piece::BlackLance:  rank = 2; break;  // 香
            case Piece::BlackPawn:   rank = 1; break;  // 歩
            default: break;
            }
        } else {
            // 後手駒台
            switch (upper) {
            case Piece::BlackKing:   rank = 2; break;  // 玉
            case Piece::BlackRook:   rank = 3; break;  // 飛
            case Piece::BlackBishop: rank = 4; break;  // 角
            case Piece::BlackGold:   rank = 5; break;  // 金
            case Piece::BlackSilver: rank = 6; break;  // 銀
            case Piece::BlackKnight: rank = 7; break;  // 桂
            case Piece::BlackLance:  rank = 8; break;  // 香
            case Piece::BlackPawn:   rank = 9; break;  // 歩
            default: break;
            }
        }

        if (rank < 0) return QPoint(-1, -1);
        return QPoint(file, rank);
    };

    SfenDiffUtils::DiffResult diff;
    const bool ok = SfenDiffUtils::diffBoards(prev, curr, diff) && diff.isSingleMovePattern();
    const QPoint from = diff.from;
    const QPoint to = diff.to;
    const QChar droppedPiece = diff.droppedPiece;

    if (!ok) {
        // フォールバック：対局中に積んだ m_gameMoves（HvH等）を参照
        const int mvIdx = safePly - 1;
        qCDebug(lcUi).noquote() << "highlight: SFEN diff failed, trying fallback"
                           << " safePly=" << safePly
                           << " mvIdx=" << mvIdx
                           << " gameMoves=" << (m_gameMoves ? QString::number(m_gameMoves->size()) : "null")
                           << " prev=" << prev.left(50)
                           << " curr=" << curr.left(50);
        if (!m_gameMoves || mvIdx < 0 || mvIdx >= m_gameMoves->size()) {
            qCDebug(lcUi).noquote() << "highlight: fallback FAILED, clearing highlights"
                               << " reason=" << (!m_gameMoves ? "gameMoves null" :
                                                 (mvIdx < 0 ? "mvIdx<0" : "mvIdx>=size"));
            m_bic->clearAllHighlights();
            return;
        }
        const ShogiMove& last = m_gameMoves->at(mvIdx);
        const bool hasFrom = (last.fromSquare.x() >= 0 && last.fromSquare.y() >= 0);

        qCDebug(lcUi).noquote() << "highlight(fallback:gameMoves)"
                           << " mvIdx=" << mvIdx
                           << " from=(" << last.fromSquare.x() << "," << last.fromSquare.y() << ")"
                           << " to=("   << last.toSquare.x()   << "," << last.toSquare.y()   << ")"
                           << " hasFrom=" << hasFrom;

        const QPoint to1 = toOne(last.toSquare);
        if (hasFrom) {
            const QPoint from1 = toOne(last.fromSquare);
            m_bic->showMoveHighlights(from1, to1);
        } else {
            // 駒打ち：movingPieceから駒台座標を取得
            const QPoint standCoord = pieceToStandCoord(last.movingPiece);
            m_bic->showMoveHighlights(standCoord, to1);
        }
        return;
    }

    const QPoint to1 = toOne(to);
    const bool hasFrom = (from.x() >= 0 && from.y() >= 0);
    qCDebug(lcUi).noquote() << "highlight(by sfen-diff)"
                       << " ply=" << safePly
                       << " from=(" << from.x() << "," << from.y() << ")"
                       << " to=("   << to.x()   << "," << to.y()   << ")"
                       << " hasFrom=" << hasFrom
                       << " droppedPiece=" << droppedPiece;

    if (hasFrom) {
        const QPoint from1 = toOne(from);
        m_bic->showMoveHighlights(from1, to1);
    } else if (!droppedPiece.isNull()) {
        // 駒打ち：打った駒種から駒台の疑似座標を取得
        const QPoint standCoord = pieceToStandCoord(charToPiece(droppedPiece));
        qCDebug(lcUi).noquote() << "drop highlight: piece=" << droppedPiece
                           << " standCoord=(" << standCoord.x() << "," << standCoord.y() << ")";
        m_bic->showMoveHighlights(standCoord, to1);
    } else {
        // 駒打ちだが駒種不明（通常は到達しない）
        m_bic->showMoveHighlights(QPoint(-1, -1), to1);
    }
}

void BoardSyncPresenter::clearHighlights() const
{
    if (m_bic) m_bic->clearAllHighlights();
}

void BoardSyncPresenter::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen) const
{
    qCDebug(lcUi).noquote() << "loadBoardWithHighlights ENTER"
                       << "currentSfen=" << currentSfen.left(40)
                       << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(40));

    if (currentSfen.isEmpty()) {
        qCWarning(lcUi) << "loadBoardWithHighlights: empty currentSfen, skipping";
        return;
    }

    // 1. 盤面を更新
    if (m_gc && m_gc->board() && m_view) {
        m_gc->board()->setSfen(currentSfen);
        m_view->applyBoardAndRender(m_gc->board());
    }

    // 2. ハイライトを計算・表示
    if (!m_bic) {
        qCWarning(lcUi) << "loadBoardWithHighlights: m_bic is null, skipping highlights";
        return;
    }

    // 開始局面（prevSfenが空）の場合はハイライトをクリア
    if (prevSfen.isEmpty()) {
        m_bic->clearAllHighlights();
        qCDebug(lcUi) << "loadBoardWithHighlights: cleared highlights (start position)";
        return;
    }

    // SFEN差分からfrom/toを計算
    SfenDiffUtils::DiffResult diff;
    const bool ok = SfenDiffUtils::diffBoards(prevSfen, currentSfen, diff)
                    && diff.isSingleMovePattern();

    if (!ok) {
        qCDebug(lcUi) << "loadBoardWithHighlights: SFEN diff failed, clearing highlights";
        m_bic->clearAllHighlights();
        return;
    }

    const QPoint from = diff.from;
    const QPoint to = diff.to;
    const QPoint to1 = toOne(to);
    const bool hasFrom = (from.x() >= 0 && from.y() >= 0);

    qCDebug(lcUi).noquote() << "loadBoardWithHighlights: highlight"
                       << "from=(" << from.x() << "," << from.y() << ")"
                       << "to=(" << to.x() << "," << to.y() << ")"
                       << "hasFrom=" << hasFrom;

    if (hasFrom) {
        const QPoint from1 = toOne(from);
        m_bic->showMoveHighlights(from1, to1);
    } else {
        // 駒打ちの場合: toのみハイライト
        m_bic->showMoveHighlights(QPoint(-1, -1), to1);
    }

    qCDebug(lcUi) << "loadBoardWithHighlights LEAVE";
}
