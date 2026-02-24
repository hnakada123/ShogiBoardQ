/// @file boardsyncpresenter.cpp
/// @brief 盤面同期プレゼンタクラスの実装

#include "boardsyncpresenter.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "boardinteractioncontroller.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "logcategories.h"

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
    // 文字列プレビュー用
    auto preview = [](const QString& s) -> QString {
        return (s.size() > 200) ? (s.left(200) + QStringLiteral(" ...")) : s;
    };

    qCDebug(lcUi) << "applySfenAtPly enter"
            << "reqPly=" << ply
            << "rec*=" << static_cast<const void*>(m_sfenHistory)
            << "gc=" << m_gc
            << "board=" << (m_gc ? m_gc->board() : nullptr)
            << "view=" << m_view;

    // ガード＆早期リターン
    if (!m_sfenHistory || m_sfenHistory->isEmpty() || !m_gc || !m_gc->board()) {
        qCWarning(lcUi) << "applySfenAtPly guard failed:"
                   << "rec*=" << static_cast<const void*>(m_sfenHistory)
                   << "isEmpty?=" << (m_sfenHistory ? m_sfenHistory->isEmpty() : true)
                   << "gc=" << m_gc << "board?=" << (m_gc ? m_gc->board() : nullptr);
        return;
    }

    const int size   = static_cast<int>(m_sfenHistory->size());
    const int maxIdx = size - 1;

    // 終局（投了など）行の判定：SFEN は増えないため reqPly > maxIdx になる
    const bool isTerminalRow = (ply > maxIdx);

    // 実際に適用するインデックスはクランプ（終局行なら常に末尾の SFEN を使う）
    const int idx = qBound(0, ply, maxIdx);

    qCDebug(lcUi).noquote()
        << QString("applySfenAtPly params reqPly=%1 size=%2 maxIdx=%3 idx=%4 terminalRow=%5")
               .arg(ply).arg(size).arg(maxIdx).arg(idx).arg(isTerminalRow);

    if (isTerminalRow) {
        // 例：開始局面 + 4手 + 投了 → size=5, reqPly=5, idx=4
        qCDebug(lcUi).noquote()
            << QString("TERMINAL-ROW: non-move row (e.g. resignation). Using last SFEN at idx=%1 (size-1).")
                   .arg(idx);
        if (ply == size) {
            qCDebug(lcUi) << "TERMINAL-ROW detail: reqPly == size (expected for resignation right after last move).";
        } else if (ply > size) {
            qCWarning(lcUi).noquote()
            << QString("TERMINAL-ROW anomaly: reqPly(%1) > size(%2). Upstream should not overshoot too much.")
                    .arg(ply).arg(size);
        }
    }

    const QString sfen = m_sfenHistory->at(idx);

    qCDebug(lcUi).noquote() << QString("applySfenAtPly reqPly=%1 idx=%2 size=%3 rec*=%4")
                             .arg(ply).arg(idx).arg(size)
                             .arg(reinterpret_cast<quintptr>(m_sfenHistory), 0, 16);

    if (size > 0) {
        qCDebug(lcUi).noquote() << "head[0]= "   << preview(m_sfenHistory->first());
        qCDebug(lcUi).noquote() << "tail[last]= "<< preview(m_sfenHistory->last());
    }
    qCDebug(lcUi).noquote() << "pick[" << idx << "]= " << preview(sfen);

    // --- 追加: リスト全体に "position " 混入がないか軽くスキャン（最初の1件だけ報告） ---
    {
        int bad = -1;
        for (int i = 0; i < size; ++i) {
            if (m_sfenHistory->at(i).startsWith(QLatin1String("position "))) { bad = i; break; }
        }
        if (bad >= 0) {
            qCWarning(lcUi).noquote() << "*** NON-SFEN DETECTED in m_sfenHistory at index "
                                 << bad << ": " << preview(m_sfenHistory->at(bad));
        }
    }

    // --- 追加: 周辺ダンプ（idx±3 だけ） ---
    {
        const int from = qMax(0, idx - 3);
        const int to   = qMin(size - 1, idx + 3);
        for (int i = from; i <= to; ++i) {
            const QString p = m_sfenHistory->at(i);
            qCDebug(lcUi).noquote() << QString("win[%1]= %2").arg(i).arg(preview(p));
        }
    }

    // --- 追加: pick文字列の基本妥当性チェック ---
    if (sfen.startsWith(QLatin1String("position "))) {
        qCWarning(lcUi) << "*** NON-SFEN passed to presenter (starts with 'position ') at idx=" << idx;
    }

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts);
    if (parts.size() == 4) {
        const QString& boardField = parts[0];
        const QString& turnField  = parts[1];
        const QString& standField = parts[2];
        const QString& moveField  = parts[3];

        qCDebug(lcUi).noquote() << "fields"
                          << " board=" << boardField
                          << " turn=" << turnField
                          << " stand=" << standField
                          << " move=" << moveField;

        // 9段の盤 → スラッシュは8本のはず
        const int slashCount = static_cast<int>(boardField.count(QLatin1Char('/')));
        if (slashCount != 8) {
            qCWarning(lcUi) << "suspicious board field: slashCount=" << slashCount << "(expected 8)";
        }

        if (turnField != QLatin1String("b") && turnField != QLatin1String("w")) {
            qCWarning(lcUi) << "suspicious turn field:" << turnField;
        }

        bool moveOk = false;
        const int moveNum = moveField.toInt(&moveOk);
        if (!moveOk || moveNum <= 0) {
            qCWarning(lcUi) << "suspicious move field:" << moveField;
        }

        if (idx == 0) {
            if (moveField != QLatin1String("1")) {
                qCWarning(lcUi) << "head move number is not 1:" << moveField;
            }
        }

        // 終局行の場合、最後の SFEN の move 番号と reqPly の関係をメモ
        if (isTerminalRow) {
            qCDebug(lcUi).noquote()
            << QString("terminal note: last SFEN's move=%1, reqPly=%2 (no new SFEN for resignation)")
                    .arg(moveField).arg(ply);
        }

    } else {
        qCWarning(lcUi).noquote() << "fields malformed (need 4 parts) size="
                             << parts.size() << " sfen=" << preview(sfen);
    }

    // --- 実適用（前後でログ） ---
    qCDebug(lcUi).noquote() << "setSfen() <= " << preview(sfen);
    m_gc->board()->setSfen(sfen);
    qCDebug(lcUi) << "setSfen() applied";

    if (m_view) {
        m_view->applyBoardAndRender(m_gc->board());
        qCDebug(lcUi) << "view->applyBoardAndRender() done";
    }

    // トレーラ：この関数は「盤面適用」担当。
    // 投了など終局行のハイライト消去は syncBoardAndHighlightsAtRow() 側で行う想定。
    qCDebug(lcUi).noquote() << "applySfenAtPly leave"
                      << " reqPly=" << ply
                      << " idx=" << idx
                      << " terminalRow=" << isTerminalRow;
}

// 盤面・ハイライト同期（行 → 盤面）
void BoardSyncPresenter::syncBoardAndHighlightsAtRow(int ply) const
{
    qCDebug(lcUi).noquote() << "syncBoardAndHighlightsAtRow CALLED"
                       << "ply=" << ply
                       << "m_sfenHistory*=" << static_cast<const void*>(m_sfenHistory)
                       << "m_sfenHistory.size=" << (m_sfenHistory ? m_sfenHistory->size() : -1);

    // 依存チェック
    if (!m_sfenHistory || !m_gc || !m_gc->board()) {
        qCDebug(lcUi).noquote() << "syncBoardAndHighlightsAtRow ABORT:"
                           << "sfenRecord*=" << static_cast<const void*>(m_sfenHistory)
                           << "sfenRecord.empty?=" << (m_sfenHistory ? m_sfenHistory->isEmpty() : true)
                           << "gc*=" << static_cast<const void*>(m_gc)
                           << "board*=" << (m_gc? static_cast<const void*>(m_gc->board()) : nullptr)
                           << " ply=" << ply;
        return;
    }

    const int size   = static_cast<int>(m_sfenHistory->size());
    const int maxIdx = size - 1;
    if (maxIdx < 0) {
        qCDebug(lcUi).noquote() << "syncBoardAndHighlightsAtRow: EMPTY sfenRecord! ply=" << ply;
        return;
    }

    const bool isTerminalRow = (ply > maxIdx);
    const int  safePly       = qBound(0, ply, maxIdx);

    qCDebug(lcUi).noquote()
        << "syncBoardAndHighlightsAtRow processing"
        << " reqPly=" << ply
        << " safePly=" << safePly
        << " size=" << size
        << " maxIdx=" << maxIdx
        << " isTerminalRow=" << isTerminalRow;

    // デバッグ: sfenRecordの先頭と末尾を表示
    if (size > 0) {
        qCDebug(lcUi).noquote() << "sfenRecord[0]=" << m_sfenHistory->at(0).left(60);
        if (safePly > 0 && safePly < size) {
            qCDebug(lcUi).noquote() << "sfenRecord[" << safePly << "]=" << m_sfenHistory->at(safePly).left(60);
        }
    }

    //重要：先に盤面を適用（元の実装と同じ順序を維持）
    // applySfenAtPly 内でクランプされる想定のため reqPly のまま渡す
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

    // 1局面分の SFEN（盤面部）を 9x9 のトークングリッドへ展開
    //FIX：段(y)は「後手側=上」を原点(0)にする → y = r
    auto parseOneBoard = [](const QString& sfen, QString grid[9][9])->bool {
        for (int y=0; y<9; ++y) for (int x=0; x<9; ++x) grid[y][x].clear();

        if (sfen.isEmpty()) return false;
        const QString boardField = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts).value(0);
        const QStringList rows   = boardField.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (rows.size() != 9) return false;

        for (int r = 0; r < 9; ++r) {
            const QString& row = rows.at(r);
            const int y = r;    // 上(後手側)=0, 下(先手側)=8
            int x = 8;          // 筋は右(9筋)=8 → 左(1筋)=0 へ詰める

            for (qsizetype i = 0; i < row.size(); ++i) {
                const QChar ch = row.at(i);
                if (ch.isDigit()) {
                    x -= (ch.toLatin1() - '0'); // 連続空白
                } else if (ch == QLatin1Char('+')) {
                    if (i + 1 >= row.size() || x < 0) return false;
                    grid[y][x] = QStringLiteral("+") + row.at(++i); // 成り駒
                    --x;
                } else {
                    if (x < 0) return false;
                    grid[y][x] = QString(ch); // 通常駒
                    --x;
                }
            }
            if (x != -1) return false; // 9マス分ちょうどで終わっていない
        }
        return true;
    };

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

    // prev/curr のグリッド差分から from/to を推定
    // 駒打ちの場合は droppedPiece に打った駒種を設定
    auto deduceByDiff = [&](const QString& a, const QString& b,
                            QPoint& from, QPoint& to, QChar& droppedPiece)->bool {
        QString ga[9][9], gb[9][9];
        if (!parseOneBoard(a, ga) || !parseOneBoard(b, gb)) return false;

        bool foundTo = false;
        droppedPiece = QChar();

        for (int y = 0; y < 9; ++y) {
            for (int x = 0; x < 9; ++x) {
                if (ga[y][x] == gb[y][x]) continue;
                if (!ga[y][x].isEmpty() && gb[y][x].isEmpty()) {
                    from = QPoint(x, y);   // 元が空いた
                } else if (ga[y][x].isEmpty() && !gb[y][x].isEmpty()) {
                    to = QPoint(x, y); foundTo = true;       // 駒打ち：空→駒
                    // 打った駒種を記録（成駒は打てないので先頭文字）
                    droppedPiece = gb[y][x].at(0);
                } else {
                    to = QPoint(x, y); foundTo = true;       // 駒移動または駒取り
                }
            }
        }
        // 駒打ちは from 不在でも OK（to があればハイライト可能）
        return foundTo;
    };

    QPoint from(-1,-1), to(-1,-1);
    QChar droppedPiece;
    bool ok = deduceByDiff(prev, curr, from, to, droppedPiece);

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

    const QPoint to1   = toOne(to);
    const bool   hasFrom = (from.x() >= 0 && from.y() >= 0);
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
    auto parseOneBoard = [](const QString& sfen, QString grid[9][9]) -> bool {
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 9; ++x)
                grid[y][x].clear();

        if (sfen.isEmpty()) return false;
        const QString boardField = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts).value(0);
        const QStringList rows = boardField.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (rows.size() != 9) return false;

        for (int r = 0; r < 9; ++r) {
            const QString& row = rows.at(r);
            const int y = r;
            int x = 8;

            for (qsizetype i = 0; i < row.size(); ++i) {
                const QChar ch = row.at(i);
                if (ch.isDigit()) {
                    x -= (ch.toLatin1() - '0');
                } else if (ch == QLatin1Char('+')) {
                    if (i + 1 >= row.size() || x < 0) return false;
                    grid[y][x] = QStringLiteral("+") + row.at(++i);
                    --x;
                } else {
                    if (x < 0) return false;
                    grid[y][x] = QString(ch);
                    --x;
                }
            }
            if (x != -1) return false;
        }
        return true;
    };

    auto deduceByDiff = [&](const QString& a, const QString& b,
                            QPoint& from, QPoint& to, QChar& droppedPiece) -> bool {
        QString ga[9][9], gb[9][9];
        if (!parseOneBoard(a, ga) || !parseOneBoard(b, gb)) return false;

        bool foundTo = false;
        droppedPiece = QChar();

        for (int y = 0; y < 9; ++y) {
            for (int x = 0; x < 9; ++x) {
                if (ga[y][x] == gb[y][x]) continue;
                if (!ga[y][x].isEmpty() && gb[y][x].isEmpty()) {
                    from = QPoint(x, y);
                } else if (ga[y][x].isEmpty() && !gb[y][x].isEmpty()) {
                    to = QPoint(x, y); foundTo = true;
                    droppedPiece = gb[y][x].at(0);
                } else {
                    to = QPoint(x, y); foundTo = true;
                }
            }
        }
        return foundTo;
    };

    QPoint from(-1, -1), to(-1, -1);
    QChar droppedPiece;
    bool ok = deduceByDiff(prevSfen, currentSfen, from, to, droppedPiece);

    if (!ok) {
        qCDebug(lcUi) << "loadBoardWithHighlights: SFEN diff failed, clearing highlights";
        m_bic->clearAllHighlights();
        return;
    }

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
