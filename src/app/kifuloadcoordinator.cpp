#include "kifuloadcoordinator.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTableWidget>

static inline QString pickLabelForDisp(const KifDisplayItem& d)
{
    return d.prettyMove;
}

inline int rankLetterToNum(QChar r) { // 'a'..'i' -> 1..9
    ushort u = r.toLower().unicode();
    return (u < 'a' || u > 'i') ? -1 : int(u - 'a') + 1;
}

inline QChar dropLetterWithSide(QChar upper, bool black) {
    return black ? upper.toUpper() : upper.toLower();
}

// 成駒トークン("+P"等) -> 1文字表現。非成駒はトークンそのまま1文字。
inline QChar tokenToOneChar(const QString& tok) {
    if (tok.isEmpty()) return QLatin1Char(' ');
    if (tok.size() == 1) return tok.at(0);
    static const QHash<QString,QChar> map = {
                                              {"+P",'Q'},{"+L",'M'},{"+N",'O'},{"+S",'T'},{"+B",'C'},{"+R",'U'},
                                              {"+p",'q'},{"+l",'m'},{"+n",'o'},{"+s",'t'},{"+b",'c'},{"+r",'u'},
                                              };
    const auto it = map.find(tok);
    return it == map.end() ? QLatin1Char(' ') : *it;
}

// ★打ちの fromSquare を駒台座標にマップ
inline QPoint dropFromSquare(QChar dropUpper, bool black) {
    const int x = black ? 9 : 10; // 先手=9, 後手=10
    int y = -1;
    switch (dropUpper.toUpper().unicode()) {
    case 'P': y = black ? 0 : 8; break;
    case 'L': y = black ? 1 : 7; break;
    case 'N': y = black ? 2 : 6; break;
    case 'S': y = black ? 3 : 5; break;
    case 'G': y = 4;            break; // 共通
    case 'B': y = black ? 5 : 3; break;
    case 'R': y = black ? 6 : 2; break;
    default:  y = -1;           break;
    }
    return QPoint(x, y);
}

using BCDI = ::BranchCandidateDisplayItem;

KifuLoadCoordinator::KifuLoadCoordinator(QObject* parent)
    : QObject(parent)
{
}

void KifuLoadCoordinator::loadKifuFromFile(const QString& filePath)
{
    // --- IN ログ ---
    qDebug().noquote() << "[MAIN] loadKifuFromFile IN file=" << filePath;

    // ★ ロード中フラグ（applyResolvedRowAndSelect 等の分岐更新を抑止）
    m_loadingKifu = true;

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    const QString initialSfen = prepareInitialSfen(filePath, teaiLabel);

    // 2) 解析（本譜＋分岐＋コメント）を一括取得
    KifParseResult res;
    QString parseWarn;
    KifToSfenConverter::parseWithVariations(filePath, res, &parseWarn);

    // 先手/後手名などヘッダ反映
    {
        const QList<KifGameInfoItem> infoItems = KifToSfenConverter::extractGameInfo(filePath);
        populateGameInfo(infoItems);
        applyPlayersFromGameInfo(infoItems);
    }

    // 本譜（表示／USI）
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    m_usiMoves = res.mainline.usiMoves;

    // 終局/中断判定（見た目文字列で簡易判定）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) if (s.contains(kw)) return true;
        return false;
    };
    const bool hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));

    if (m_usiMoves.isEmpty() && !hasTerminal && disp.isEmpty()) {
        const QString errorMessage = tr("読み込み失敗 %1 から指し手を取得できませんでした。").arg(filePath);
        emit errorOccurred(errorMessage);
        qDebug().noquote() << "[MAIN] loadKifuFromFile OUT (no moves)";
        m_loadingKifu = false; // 早期return時も必ず解除
        return;
    }

    // 3) 本譜の SFEN 列と m_gameMoves を再構築
    rebuildSfenRecord(initialSfen, m_usiMoves, hasTerminal);
    rebuildGameMoves(initialSfen, m_usiMoves);

    // 3.5) USI position コマンド列を構築（0..N）
    //     initialSfen は prepareInitialSfen() が返す手合い込みの SFEN
    //     m_usiMoves は 1..N の USI 文字列（"7g7f" 等）
    m_positionStrList.clear();
    m_positionStrList.reserve(m_usiMoves.size() + 1);

    const QString base = QStringLiteral("position sfen %1").arg(initialSfen);
    m_positionStrList.push_back(base);  // 0手目：moves なし

    QStringList acc; // 先頭からの累積
    acc.reserve(m_usiMoves.size());
    for (int i = 0; i < m_usiMoves.size(); ++i) {
        acc.push_back(m_usiMoves.at(i));
        // i+1 手目：先頭から i+1 個の moves を連結
        m_positionStrList.push_back(base + QStringLiteral(" moves ") + acc.join(' '));
    }

    // （任意）ログで確認
    qDebug().noquote() << "[USI] position list built. count=" << m_positionStrList.size();
    if (!m_positionStrList.isEmpty()) {
        qDebug().noquote() << "[USI] pos[0]=" << m_positionStrList.first();
        if (m_positionStrList.size() > 1)
            qDebug().noquote() << "[USI] pos[1]=" << m_positionStrList.at(1);
    }

    // 4) 棋譜表示へ反映（本譜）
    emit displayGameRecord(disp);

    // 5) 本譜スナップショットを保持（以降の解決・描画に使用）
    m_dispMain = disp;          // 表示列（1..N）
    m_sfenMain = *m_sfenRecord; // 0..N の局面列
    m_gmMain   = m_gameMoves;   // 1..N のUSIムーブ

    // 6) 変化を取りまとめ（必要に応じて保持：Plan生成やツリー表示では m_resolvedRows を主に使用）
    m_variationsByPly.clear();
    m_variationsSeq.clear();
    for (const KifVariation& kv : std::as_const(res.variations)) {
        KifLine L = kv.line;
        L.startPly = kv.startPly;         // “その変化が始まる絶対手数（1-origin）”
        if (L.disp.isEmpty()) continue;
        m_variationsByPly[L.startPly].push_back(L);
        m_variationsSeq.push_back(L);     // 入力順（KIF出現順）を保持
    }

    // 7) 棋譜テーブルの初期選択（開始局面を選択）
    if (m_recordPane && m_recordPane->kifuView()) {
        QTableView* view = m_recordPane->kifuView();
        if (view->model() && view->model()->rowCount() > 0) {
            const QModelIndex idx0 = view->model()->index(0, 0);
            if (view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollTo(idx0, QAbstractItemView::PositionAtTop);
        }
    }

    // 8) 解決行を1本（本譜のみ）作成 → 0手適用
    m_resolvedRows.clear();

    ResolvedRow r;
    r.startPly = 1;
    r.parent   = -1;           // ★本譜
    r.disp     = disp;          // 1..N
    r.sfen     = *m_sfenRecord; // 0..N
    r.gm       = m_gameMoves;   // 1..N
    r.varIndex = -1;            // 本譜

    m_resolvedRows.push_back(r);
    m_activeResolvedRow = 0;
    m_activePly         = 0;

    // apply 内では m_loadingKifu を見て分岐候補の更新を抑止
    applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/0);
}

void KifuLoadCoordinator::setGameInfoTable(QTableWidget *newGameInfoTable)
{
    m_gameInfoTable = newGameInfoTable;
}

void KifuLoadCoordinator::setTab(QTabWidget *newTab)
{
    m_tab = newTab;
}

void KifuLoadCoordinator::setShogiView(ShogiView *newShogiView)
{
    m_shogiView = newShogiView;
}

void KifuLoadCoordinator::setSfenRecord(QStringList *newSfenRecord)
{
    m_sfenRecord = newSfenRecord;
}

void KifuLoadCoordinator::setGameMoves(const QVector<ShogiMove> &newGameMoves)
{
    m_gameMoves = newGameMoves;
}

void KifuLoadCoordinator::setPositionStrList(const QStringList &newPositionStrList)
{
    m_positionStrList = newPositionStrList;
}

void KifuLoadCoordinator::setDispMain(const QList<KifDisplayItem> &newDispMain)
{
    m_dispMain = newDispMain;
}

void KifuLoadCoordinator::setSfenMain(const QStringList &newSfenMain)
{
    m_sfenMain = newSfenMain;
}

void KifuLoadCoordinator::setGmMain(const QVector<ShogiMove> &newGmMain)
{
    m_gmMain = newGmMain;
}

void KifuLoadCoordinator::setVariationsByPly(const QHash<int, QList<KifLine> > &newVariationsByPly)
{
    m_variationsByPly = newVariationsByPly;
}

void KifuLoadCoordinator::setVariationsSeq(const QList<KifLine> &newVariationsSeq)
{
    m_variationsSeq = newVariationsSeq;
}

void KifuLoadCoordinator::setRecordPane(RecordPane *newRecordPane)
{
    m_recordPane = newRecordPane;
}

void KifuLoadCoordinator::setResolvedRows(const QVector<ResolvedRow> &newResolvedRows)
{
    m_resolvedRows = newResolvedRows;
}

QString KifuLoadCoordinator::prepareInitialSfen(const QString& filePath, QString& teaiLabel) const
{
    const QString sfen = KifToSfenConverter::detectInitialSfenFromFile(filePath, &teaiLabel);
    return sfen.isEmpty()
               ? QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
               : sfen;
}

void KifuLoadCoordinator::populateGameInfo(const QList<KifGameInfoItem>& items)
{
    m_gameInfoTable->clearContents();
    m_gameInfoTable->setRowCount(items.size());

    for (int row = 0; row < items.size(); ++row) {
        const auto& it = items.at(row);
        auto *keyItem   = new QTableWidgetItem(it.key);
        auto *valueItem = new QTableWidgetItem(it.value);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        m_gameInfoTable->setItem(row, 0, keyItem);
        m_gameInfoTable->setItem(row, 1, valueItem);
    }

    m_gameInfoTable->resizeColumnToContents(0);

    // まだタブに載ってなければ、このタイミングで追加しておくと確実
    addGameInfoTabIfMissing();
}

void KifuLoadCoordinator::addGameInfoTabIfMissing()
{
    if (!m_tab) return;

    // Dock で表示していたら解除
    if (m_gameInfoDock && m_gameInfoDock->widget() == m_gameInfoTable) {
        m_gameInfoDock->setWidget(nullptr);
        m_gameInfoDock->deleteLater();
        m_gameInfoDock = nullptr;
    }

    // まだタブに無ければ追加
    if (m_tab->indexOf(m_gameInfoTable) == -1) {
        int anchorIdx = -1;

        // 1) EngineAnalysisTab（検討タブ）の直後に入れる
        if (m_analysisTab)
            anchorIdx = m_tab->indexOf(m_analysisTab);

        // 2) 念のため、タブタイトルで「コメント/Comments」を探してその直後に入れるフォールバック
        if (anchorIdx < 0) {
            for (int i = 0; i < m_tab->count(); ++i) {
                const QString t = m_tab->tabText(i);
                if (t.contains(tr("コメント")) || t.contains("Comments", Qt::CaseInsensitive)) {
                    anchorIdx = i;
                    break;
                }
            }
        }

        const int insertPos = (anchorIdx >= 0) ? anchorIdx + 1 : m_tab->count();
        m_tab->insertTab(insertPos, m_gameInfoTable, tr("対局情報"));
    }
}

QString KifuLoadCoordinator::findGameInfoValue(const QList<KifGameInfoItem>& items,
                                      const QStringList& keys) const
{
    for (const auto& it : items) {
        // KifGameInfoItem.key は「先手」「後手」等（末尾コロンは normalize 済み）
        if (keys.contains(it.key)) {
            const QString v = it.value.trimmed();
            if (!v.isEmpty()) return v;
        }
    }
    return QString();
}

void KifuLoadCoordinator::applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items)
{
    // 優先度：
    // 1) 先手／後手
    // 2) 下手／上手（→ 下手=Black, 上手=White）
    // 3) 先手省略名／後手省略名
    QString black = findGameInfoValue(items, { QStringLiteral("先手") });
    QString white = findGameInfoValue(items, { QStringLiteral("後手") });

    // 下手/上手にも対応（必要な側だけ補完）
    const QString shitate = findGameInfoValue(items, { QStringLiteral("下手") });
    const QString uwate   = findGameInfoValue(items, { QStringLiteral("上手") });

    if (black.isEmpty() && !shitate.isEmpty())
        black = shitate;                   // 下手 → Black
    if (white.isEmpty() && !uwate.isEmpty())
        white = uwate;                     // 上手 → White

    // 省略名でのフォールバック
    if (black.isEmpty())
        black = findGameInfoValue(items, { QStringLiteral("先手省略名") });
    if (white.isEmpty())
        white = findGameInfoValue(items, { QStringLiteral("後手省略名") });

    // 取得できた方だけ反映（既存表示を尊重）
    if (!black.isEmpty() && m_shogiView)
        m_shogiView->setBlackPlayerName(black);
    if (!white.isEmpty() && m_shogiView)
        m_shogiView->setWhitePlayerName(white);
}

void KifuLoadCoordinator::rebuildSfenRecord(const QString& initialSfen, const QStringList& usiMoves, bool hasTerminal)
{
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(initialSfen)) {
        tracer.setFromSfen(QStringLiteral(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
    }

    QStringList list;
    list << tracer.toSfenString();          // 0) 初期
    for (const QString& mv : usiMoves) {
        tracer.applyUsiMove(mv);
        list << tracer.toSfenString();      // 1..N) 各手後
    }
    if (hasTerminal) {
        list << tracer.toSfenString();      // N+1) 終局/中断表示用
    }

    if (!m_sfenRecord) m_sfenRecord = new QStringList;
    *m_sfenRecord = list;                    // COW
}

void KifuLoadCoordinator::rebuildGameMoves(const QString& initialSfen, const QStringList& usiMoves)
{
    m_gameMoves.clear();

    SfenPositionTracer tracer;
    tracer.setFromSfen(initialSfen);

    for (const QString& usi : usiMoves) {
        // ドロップ: "P*5e"
        if (usi.size() >= 4 && usi.at(1) == QLatin1Char('*')) {
            const bool black = tracer.blackToMove();
            const QChar dropUpper = usi.at(0).toUpper();      // P/L/N/S/G/B/R
            const int  file = usi.at(2).toLatin1() - '0';     // '1'..'9'
            const int  rank = rankLetterToNum(usi.at(3));     // 1..9
            if (file < 1 || file > 9 || rank < 1 || rank > 9) { tracer.applyUsiMove(usi); continue; }

            const QPoint from = dropFromSquare(dropUpper, black);     // ★ 駒台
            const QPoint to(file - 1, rank - 1);
            const QChar  moving   = dropLetterWithSide(dropUpper, black);
            const QChar  captured = QLatin1Char(' ');
            const bool   promo    = false;

            m_gameMoves.push_back(ShogiMove(from, to, moving, captured, promo));
            tracer.applyUsiMove(usi);
            continue;
        }

        // 通常手: "7g7f" or "2b3c+"
        if (usi.size() < 4) { tracer.applyUsiMove(usi); continue; }

        const int ff = usi.at(0).toLatin1() - '0';
        const int rf = rankLetterToNum(usi.at(1));
        const int ft = usi.at(2).toLatin1() - '0';
        const int rt = rankLetterToNum(usi.at(3));
        const bool isProm = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));
        if (ff<1||ff>9||rf<1||rf>9||ft<1||ft>9||rt<1||rt>9) { tracer.applyUsiMove(usi); continue; }

        const QString fromTok = tracer.tokenAtFileRank(ff, usi.at(1));
        const QString toTok   = tracer.tokenAtFileRank(ft, usi.at(3));

        const QPoint from(ff - 1, rf - 1);
        const QPoint to  (ft - 1, rt - 1);

        const QChar moving   = tokenToOneChar(fromTok);
        const QChar captured = tokenToOneChar(toTok);

        m_gameMoves.push_back(ShogiMove(from, to, moving, captured, isProm));
        tracer.applyUsiMove(usi);
    }
}

void KifuLoadCoordinator::applyResolvedRowAndSelect(int row, int selPly)
{
    if (row < 0 || row >= m_resolvedRows.size()) {
        qDebug() << "[APPLY] invalid row =" << row << " rows=" << m_resolvedRows.size();
        return;
    }

    // 行を確定
    m_activeResolvedRow = row;
    const auto& r = m_resolvedRows[row];

    // 盤面＆棋譜モデル（まず行データを丸ごと差し替え）
    *m_sfenRecord = r.sfen;   // 0..N のSFEN列
    m_gameMoves   = r.gm;     // 1..N のUSI列

    // ★ 手数を正規化して共有メンバへ反映（0=初期局面）
    const int maxPly = r.disp.size();
    m_activePly = qBound(0, selPly, maxPly);

    // 棋譜欄へ反映（モデル差し替え＋選択手へスクロール）
    showRecordAtPly(r.disp, m_activePly);
    m_currentSelectedPly = m_activePly;

    // RecordPane 経由で安全に選択・スクロール
    if (m_kifuRecordModel) {
        const QModelIndex idx = m_kifuRecordModel->index(m_activePly, 0);
        if (idx.isValid()) {
            if (m_recordPane) {
                if (QTableView* view = m_recordPane->kifuView()) {
                    if (view->model() == m_kifuRecordModel) {
                        view->setCurrentIndex(idx);
                        view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                    }
                }
            }
        }
    }

    // ======== 分岐候補の更新（Plan 方式に一本化）========
    {
        if (m_loadingKifu) {
            // ★ 読み込み中は分岐更新をスキップ（ノイズ抑制）
            qDebug() << "[BRANCH] skip during loading (applyResolvedRowAndSelect)";
            if (m_kifuBranchModel) {
                m_kifuBranchModel->clearBranchCandidates();
                m_kifuBranchModel->setHasBackToMainRow(false);
            }
            if (QTableView* view = m_kifuBranchView
                                       ? m_kifuBranchView
                                       : (m_recordPane ? m_recordPane->branchView() : nullptr)) {
                if (view) {
                    view->setVisible(false);
                    view->setEnabled(false);
                }
            }
        } else {
            // ★ ここだけで十分：Plan から分岐候補欄を構築・表示
            showBranchCandidatesFromPlan(/*row*/m_activeResolvedRow, /*ply1*/m_activePly);
        }
    }
    // ======== 差し替えここまで ========

    // 盤面ハイライト・矢印ボタン
    emit syncBoardAndHighlightsAtRow(m_activePly);
    emit enableArrowButtons();

    qDebug().noquote() << "[APPLY] row=" << row
                       << " ply=" << m_activePly
                       << " rows=" << m_resolvedRows.size()
                       << " dispSz=" << r.disp.size();

    // 分岐ツリー側の黄色ハイライト同期（EngineAnalysisTab に移譲）
    if (m_analysisTab) {
        m_analysisTab->highlightBranchTreeAt(/*row*/m_activeResolvedRow,
                                             /*ply*/m_activePly,
                                             /*centerOn*/false);
    }

    // ★ 棋譜欄の「分岐あり」行をオレンジでマーク
    updateKifuBranchMarkersForActiveRow();
}

// 現在表示用の棋譜列（disp）を使ってモデルを再構成し、selectPly 行を選択・同期する
void KifuLoadCoordinator::showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly)
{
    // いま表示中の棋譜列を保持（分岐⇄本譜の復帰で再利用）
    m_dispCurrent = disp;

    // （既存）モデルへ反映：ここで displayGameRecord(disp) が呼ばれ、
    // その過程で m_currentMoveIndex が 0 に戻る実装になっている
    displayGameRecord(disp);

    // ★ RecordPane 内のビューを使う
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view || !view->model()) return;

    // 行数（0 は「=== 開始局面 ===」、1..N が各手）
    const int rc  = view->model()->rowCount();
    const int row = qBound(0, selectPly, rc > 0 ? rc - 1 : 0);

    // 対象行を選択
    const QModelIndex idx = view->model()->index(row, 0);
    if (!idx.isValid()) return;

    if (auto* sel = view->selectionModel()) {
        sel->setCurrentIndex(idx,
                             QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        view->setCurrentIndex(idx);
    }
    view->scrollTo(idx, QAbstractItemView::PositionAtCenter);

    // ＝＝＝＝＝＝＝＝＝＝＝＝ ここが肝 ＝＝＝＝＝＝＝＝＝＝＝＝
    // displayGameRecord() が 0 に戻した “現在の手数” を、選択行へ復元する。
    // （row は 0=開始局面, 1..N=それぞれの手。よって「次の追記」は row+1 手目になる）
    m_currentSelectedPly = row;
    m_currentMoveIndex   = row;

    // 盤面・ハイライトも現在手に同期（applySfenAtCurrentPly は m_currentSelectedPly を参照）
    emit syncBoardAndHighlightsAtRow(row);
}

void KifuLoadCoordinator::showBranchCandidatesFromPlan(int row, int ply1)
{
    if (!m_branchCtl || !m_kifuBranchModel) return;

    // 0手目や行範囲外は非表示
    if (ply1 <= 0 || row < 0 || row >= m_resolvedRows.size()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    // Plan 参照
    const auto itRow = m_branchDisplayPlan.constFind(row);
    if (itRow == m_branchDisplayPlan.constEnd()) {
        // Plan なし → 非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }
    const auto& mp = itRow.value();
    const auto itP = mp.constFind(ply1);
    if (itP == mp.constEnd()) {
        // この手の Plan なし → 非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    const BranchCandidateDisplay& plan = itP.value(); // ply/baseLabel/items

    // 「候補が1つ＆現在指し手と同じなら隠す」ルール
    const QString currentLbl = [&, this]{
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    }();

    bool hide = false;
    if (plan.items.size() == 1) {
        const auto& only = plan.items.front();
        if (!only.label.isEmpty() && only.label == currentLbl) hide = true;
    }

    if (hide || plan.items.isEmpty()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    // 表示（Controller経由で Plan をそのまま流し込む）
    // MainWindow ローカル型 → 公開型(グローバル)へ明示変換
    QVector<BCDI> pubItems;
    pubItems.reserve(plan.items.size());
    for (const auto& it : plan.items) {
        BCDI x;
        x.row      = it.row;
        x.varN     = it.varN;
        x.lineName = it.lineName;
        x.label    = it.label;
        pubItems.push_back(std::move(x));
    }
    m_branchCtl->refreshCandidatesFromPlan(ply1, pubItems, plan.baseLabel);

    // ビューの可視化
    if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
        const int rows = m_kifuBranchModel->rowCount();
        const bool show = (rows > 0);
        view->setVisible(show);
        view->setEnabled(show);
        if (show) {
            const QModelIndex idx0 = m_kifuBranchModel->index(0, 0);
            if (idx0.isValid() && view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollToTop();
        }
    }

    // UI 状態
    m_branchPlyContext   = ply1;
    m_activeResolvedRow  = row; // ←行は applyResolvedRowAndSelect でも更新されますが念のため同期
}

// ====== アクティブ行に対する「分岐あり手」の再計算 → ビュー再描画 ======
void KifuLoadCoordinator::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    // まずビュー参照を取得（nullでも安全に抜ける）
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);

    if (m_resolvedRows.isEmpty()) {
        if (view && view->viewport()) view->viewport()->update();
        return;
    }

    const int active = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[active];

    // r.disp: 1..N の手表示, r.sfen: 0..N の局面列
    for (int ply = 1; ply <= r.disp.size(); ++ply) {
        const int idx = ply - 1;                 // sfen は ply-1 が基底
        if (idx < 0 || idx >= r.sfen.size()) continue;

        const auto itPly = m_branchIndex.constFind(ply);
        if (itPly == m_branchIndex.constEnd()) continue;

        const QString base = r.sfen.at(idx);
        const auto itBase = itPly->constFind(base);
        if (itBase == itPly->constEnd()) continue;

        // 同じ手目に候補が2つ以上 → 分岐点としてマーキング
        if (itBase->size() >= 2) {
            m_branchablePlySet.insert(ply);
        }
    }

    // デリゲート装着（未装着ならここで装着）
    ensureBranchRowDelegateInstalled();

    // 再描画
    if (view && view->viewport()) view->viewport()->update();
}

void KifuLoadCoordinator::ensureBranchRowDelegateInstalled()
{
    // RecordPane から棋譜ビューを取得
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view) return;

    if (!m_branchRowDelegate) {
        // デリゲートをビューの子として作成し、ビューに適用
        m_branchRowDelegate = new BranchRowDelegate(view);
        view->setItemDelegate(m_branchRowDelegate);
    } else {
        // 念のため親が違う場合は付け替え
        if (m_branchRowDelegate->parent() != view) {
            m_branchRowDelegate->setParent(view);
            view->setItemDelegate(m_branchRowDelegate);
        }
    }

    // 「分岐あり」マーカーの集合をデリゲートへ渡す
    m_branchRowDelegate->setMarkers(&m_branchablePlySet);
}
