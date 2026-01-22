#include "boardsyncpresenter.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "boardinteractioncontroller.h"
#include "shogiboard.h"
#include "shogimove.h"
#include <QDebug>

BoardSyncPresenter::BoardSyncPresenter(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_bic(d.bic)
    , m_sfenRecord(d.sfenRecord)
    , m_gameMoves(d.gameMoves)
{
}

void BoardSyncPresenter::applySfenAtPly(int ply) const
{
    // 文字列プレビュー用
    auto preview = [](const QString& s) -> QString {
        return (s.size() > 200) ? (s.left(200) + QStringLiteral(" ...")) : s;
    };

    qInfo() << "[PRESENTER] applySfenAtPly enter"
            << "reqPly=" << ply
            << "rec*=" << static_cast<const void*>(m_sfenRecord)
            << "gc=" << m_gc
            << "board=" << (m_gc ? m_gc->board() : nullptr)
            << "view=" << m_view;

    // ガード＆早期リターン
    if (!m_sfenRecord || m_sfenRecord->isEmpty() || !m_gc || !m_gc->board()) {
        qWarning() << "[PRESENTER] applySfenAtPly guard failed:"
                   << "rec*=" << static_cast<const void*>(m_sfenRecord)
                   << "isEmpty?=" << (m_sfenRecord ? m_sfenRecord->isEmpty() : true)
                   << "gc=" << m_gc << "board?=" << (m_gc ? m_gc->board() : nullptr);
        return;
    }

    const int size   = static_cast<int>(m_sfenRecord->size());
    const int maxIdx = size - 1;

    // ★ 終局（投了など）行の判定：SFEN は増えないため reqPly > maxIdx になる
    const bool isTerminalRow = (ply > maxIdx);

    // ★ 実際に適用するインデックスはクランプ（終局行なら常に末尾の SFEN を使う）
    const int idx = qBound(0, ply, maxIdx);

    qInfo().noquote()
        << QString("[PRESENTER] applySfenAtPly params reqPly=%1 size=%2 maxIdx=%3 idx=%4 terminalRow=%5")
               .arg(ply).arg(size).arg(maxIdx).arg(idx).arg(isTerminalRow);

    if (isTerminalRow) {
        // 例：開始局面 + 4手 + 投了 → size=5, reqPly=5, idx=4
        qInfo().noquote()
            << QString("[PRESENTER] TERMINAL-ROW: non-move row (e.g. resignation). Using last SFEN at idx=%1 (size-1).")
                   .arg(idx);
        if (ply == size) {
            qInfo() << "[PRESENTER] TERMINAL-ROW detail: reqPly == size (expected for resignation right after last move).";
        } else if (ply > size) {
            qWarning().noquote()
            << QString("[PRESENTER] TERMINAL-ROW anomaly: reqPly(%1) > size(%2). Upstream should not overshoot too much.")
                    .arg(ply).arg(size);
        }
    }

    const QString sfen = m_sfenRecord->at(idx);

    qInfo().noquote() << QString("[PRESENTER] applySfenAtPly reqPly=%1 idx=%2 size=%3 rec*=%4")
                             .arg(ply).arg(idx).arg(size)
                             .arg(reinterpret_cast<quintptr>(m_sfenRecord), 0, 16);

    if (size > 0) {
        qInfo().noquote() << "[PRESENTER] head[0]= "   << preview(m_sfenRecord->first());
        qInfo().noquote() << "[PRESENTER] tail[last]= "<< preview(m_sfenRecord->last());
    }
    qInfo().noquote() << "[PRESENTER] pick[" << idx << "]= " << preview(sfen);

    // --- 追加: リスト全体に "position " 混入がないか軽くスキャン（最初の1件だけ報告） ---
    {
        int bad = -1;
        for (int i = 0; i < size; ++i) {
            if (m_sfenRecord->at(i).startsWith(QLatin1String("position "))) { bad = i; break; }
        }
        if (bad >= 0) {
            qWarning().noquote() << "[PRESENTER] *** NON-SFEN DETECTED in m_sfenRecord at index "
                                 << bad << ": " << preview(m_sfenRecord->at(bad));
        }
    }

    // --- 追加: 周辺ダンプ（idx±3 だけ） ---
    {
        const int from = qMax(0, idx - 3);
        const int to   = qMin(size - 1, idx + 3);
        for (int i = from; i <= to; ++i) {
            const QString p = m_sfenRecord->at(i);
            qInfo().noquote() << QString("[PRESENTER] win[%1]= %2").arg(i).arg(preview(p));
        }
    }

    // --- 追加: pick文字列の基本妥当性チェック ---
    if (sfen.startsWith(QLatin1String("position "))) {
        qWarning() << "[PRESENTER] *** NON-SFEN passed to presenter (starts with 'position ') at idx=" << idx;
    }

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts);
    if (parts.size() == 4) {
        const QString& boardField = parts[0];
        const QString& turnField  = parts[1];
        const QString& standField = parts[2];
        const QString& moveField  = parts[3];

        qInfo().noquote() << "[PRESENTER] fields"
                          << " board=" << boardField
                          << " turn=" << turnField
                          << " stand=" << standField
                          << " move=" << moveField;

        // 9段の盤 → スラッシュは8本のはず
        const int slashCount = static_cast<int>(boardField.count(QLatin1Char('/')));
        if (slashCount != 8) {
            qWarning() << "[PRESENTER] suspicious board field: slashCount=" << slashCount << "(expected 8)";
        }

        if (turnField != QLatin1String("b") && turnField != QLatin1String("w")) {
            qWarning() << "[PRESENTER] suspicious turn field:" << turnField;
        }

        bool moveOk = false;
        const int moveNum = moveField.toInt(&moveOk);
        if (!moveOk || moveNum <= 0) {
            qWarning() << "[PRESENTER] suspicious move field:" << moveField;
        }

        if (idx == 0) {
            if (moveField != QLatin1String("1")) {
                qWarning() << "[PRESENTER] head move number is not 1:" << moveField;
            }
        }

        // ★ 終局行の場合、最後の SFEN の move 番号と reqPly の関係をメモ
        if (isTerminalRow) {
            qInfo().noquote()
            << QString("[PRESENTER] terminal note: last SFEN's move=%1, reqPly=%2 (no new SFEN for resignation)")
                    .arg(moveField).arg(ply);
        }

    } else {
        qWarning().noquote() << "[PRESENTER] fields malformed (need 4 parts) size="
                             << parts.size() << " sfen=" << preview(sfen);
    }

    // --- 実適用（前後でログ） ---
    qInfo().noquote() << "[PRESENTER] setSfen() <= " << preview(sfen);
    m_gc->board()->setSfen(sfen);
    qInfo() << "[PRESENTER] setSfen() applied";

    if (m_view) {
        m_view->applyBoardAndRender(m_gc->board());
        qInfo() << "[PRESENTER] view->applyBoardAndRender() done";
    }

    // ★ トレーラ：この関数は「盤面適用」担当。
    // 投了など終局行のハイライト消去は syncBoardAndHighlightsAtRow() 側で行う想定。
    qInfo().noquote() << "[PRESENTER] applySfenAtPly leave"
                      << " reqPly=" << ply
                      << " idx=" << idx
                      << " terminalRow=" << isTerminalRow;
}

// 盤面・ハイライト同期（行 → 盤面）
void BoardSyncPresenter::syncBoardAndHighlightsAtRow(int ply) const
{
    // 依存チェック
    if (!m_sfenRecord || !m_gc || !m_gc->board()) {
        qDebug().noquote() << "[PRESENTER] syncBoardAndHighlightsAtRow ABORT:"
                           << "sfenRecord?" << (m_sfenRecord!=nullptr)
                           << "gc?"        << (m_gc!=nullptr)
                           << "board?"     << (m_gc? (m_gc->board()!=nullptr) : false)
                           << " ply="      << ply;
        return;
    }

    const int size   = static_cast<int>(m_sfenRecord->size());
    const int maxIdx = size - 1;
    if (maxIdx < 0) {
        qDebug().noquote() << "[PRESENTER] syncBoardAndHighlightsAtRow: empty sfenRecord ply=" << ply;
        return;
    }

    const bool isTerminalRow = (ply > maxIdx);
    const int  safePly       = qBound(0, ply, maxIdx);

    qDebug().noquote()
        << "[PRESENTER] syncBoardAndHighlightsAtRow enter"
        << " reqPly=" << ply
        << " safePly=" << safePly
        << " size=" << size
        << " maxIdx=" << maxIdx
        << " isTerminalRow=" << isTerminalRow;

    // ★重要：先に盤面を適用（元の実装と同じ順序を維持）
    // applySfenAtPly 内でクランプされる想定のため reqPly のまま渡す
    applySfenAtPly(ply);

    // ハイライト器（BIC）が無ければここで終了（盤面だけは更新済み）
    if (!m_bic) {
        qDebug().noquote() << "[PRESENTER] syncBoardAndHighlightsAtRow: no BIC; skip highlights";
        return;
    }

    // 開始局面（0手目）や終端行（投了等）の行はハイライト消去のみ
    if (safePly <= 0 || isTerminalRow) {
        qDebug().noquote() << "[PRESENTER] syncBoardAndHighlightsAtRow: clear highlights"
                           << " reason=" << (safePly<=0 ? "startpos" : "terminalRow");
        m_bic->clearAllHighlights();
        return;
    }

    // ======== ここからハイライト推定（SFEN差分） ========
    auto sfenAt = [&](int idx)->QString {
        if (!m_sfenRecord || idx < 0 || idx >= m_sfenRecord->size()) return QString();
        return m_sfenRecord->at(idx);
    };
    const QString prev = sfenAt(safePly - 1);
    const QString curr = sfenAt(safePly);

    // 1局面分の SFEN（盤面部）を 9x9 のトークングリッドへ展開
    // ★FIX：段(y)は「後手側=上」を原点(0)にする → y = r
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
    auto pieceToStandCoord = [](QChar piece) -> QPoint {
        // 先手（大文字）の場合: file=10
        // 後手（小文字）の場合: file=11
        const bool isBlack = piece.isUpper();
        const int file = isBlack ? 10 : 11;
        const QChar upper = piece.toUpper();

        // 駒種→rank のマッピング（shogiview.cpp の standPseudoToBase の逆引き）
        // 先手駒台（file=10）:
        //   右列(baseFile=1): K→8, B→6, S→4, L→2
        //   左列(baseFile=2): R→7, G→5, N→3, P→1
        // 後手駒台（file=11）:
        //   左列(baseFile=1): r→3, g→5, n→7, p→9
        //   右列(baseFile=2): k→2, b→4, s→6, l→8
        int rank = -1;
        if (isBlack) {
            // 先手駒台
            switch (upper.toLatin1()) {
            case 'K': rank = 8; break;  // 玉
            case 'R': rank = 7; break;  // 飛
            case 'B': rank = 6; break;  // 角
            case 'G': rank = 5; break;  // 金
            case 'S': rank = 4; break;  // 銀
            case 'N': rank = 3; break;  // 桂
            case 'L': rank = 2; break;  // 香
            case 'P': rank = 1; break;  // 歩
            default: break;
            }
        } else {
            // 後手駒台
            switch (upper.toLatin1()) {
            case 'K': rank = 2; break;  // 玉
            case 'R': rank = 3; break;  // 飛
            case 'B': rank = 4; break;  // 角
            case 'G': rank = 5; break;  // 金
            case 'S': rank = 6; break;  // 銀
            case 'N': rank = 7; break;  // 桂
            case 'L': rank = 8; break;  // 香
            case 'P': rank = 9; break;  // 歩
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
        qDebug().noquote() << "[PRESENTER] highlight: SFEN diff failed, trying fallback"
                           << " safePly=" << safePly
                           << " mvIdx=" << mvIdx
                           << " gameMoves=" << (m_gameMoves ? QString::number(m_gameMoves->size()) : "null")
                           << " prev=" << prev.left(50)
                           << " curr=" << curr.left(50);
        if (!m_gameMoves || mvIdx < 0 || mvIdx >= m_gameMoves->size()) {
            qDebug().noquote() << "[PRESENTER] highlight: fallback FAILED, clearing highlights"
                               << " reason=" << (!m_gameMoves ? "gameMoves null" :
                                                 (mvIdx < 0 ? "mvIdx<0" : "mvIdx>=size"));
            m_bic->clearAllHighlights();
            return;
        }
        const ShogiMove& last = m_gameMoves->at(mvIdx);
        const bool hasFrom = (last.fromSquare.x() >= 0 && last.fromSquare.y() >= 0);

        qDebug().noquote() << "[PRESENTER] highlight(fallback:gameMoves)"
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
    qDebug().noquote() << "[PRESENTER] highlight(by sfen-diff)"
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
        const QPoint standCoord = pieceToStandCoord(droppedPiece);
        qDebug().noquote() << "[PRESENTER] drop highlight: piece=" << droppedPiece
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
