#include "kifuloadcoordinator.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"
#include "recordpane.h"              // これまで通り（前方宣言側）
#include "kifurecordlistmodel.h"     // ← 実体定義（必須）
#include "kifubranchlistmodel.h"     // ← 実体定義（必須）

#include <QStyledItemDelegate>
#include <QAbstractItemView>         // view->model() を使うなら
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTableWidget>
#include <QPainter>
#include <QFileInfo>

// デバッグのオン/オフ（必要に応じて false に）
static bool kGM_VERBOSE = true;

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

static inline QString lineNameForRow(int row) {
    return (row == 0) ? QStringLiteral("Main") : QStringLiteral("Var%1").arg(row - 1);
}

// 1セルを "file-rank" 表示（USI基準とL2R基準の両方を出す）
static inline QString idxHuman(int idx) {
    const int col = idx % 9;       // 0..8   左→右
    const int row = idx / 9;       // 0..8   上→下
    const int fileL2R = col + 1;   // 1..9   左→右
    const int rankTop = row + 1;   // 1..9   上→下
    const int fileUSI = 9 - col;   // 9..1   右→左（一般的なUSIの筋）
    const int rankUSI = 9 - row;   // 9..1   下→上（一般的なUSIの段）
    return QStringLiteral("[idx=%1 L2R(%2,%3) USI(%4,%5)]")
        .arg(idx).arg(fileL2R).arg(rankTop).arg(fileUSI).arg(rankUSI);
}

// 盤面(81マス)をトークン列に展開（空は ""。駒は "P","p","+P" のように '+' 付きも保持）
static QVector<QString> sfenBoardTo81Tokens(const QString& sfen)
{
    const QString board = sfen.section(QLatin1Char(' '), 0, 0); // 盤面部分
    QVector<QString> cells; cells.reserve(81);

    for (int i = 0; i < board.size() && cells.size() < 81; ++i) {
        const QChar ch = board.at(i);
        if (ch == QLatin1Char('/')) continue;
        if (ch.isDigit()) {
            const int n = ch.digitValue();
            for (int k = 0; k < n; ++k) cells.push_back(QString()); // 空マス
            continue;
        }
        if (ch == QLatin1Char('+')) {
            if (i + 1 < board.size()) {
                cells.push_back(QStringLiteral("+") + board.at(i + 1));
                ++i;
            }
            continue;
        }
        // 通常駒1文字
        cells.push_back(QString(ch));
    }
    while (cells.size() < 81) cells.push_back(QString());

    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] sfenBoardTo81Tokens parsed"
                           << " len=" << cells.size()
                           << " board=\"" << board << "\"";
    }
    return cells;
}

static inline bool tokenEmpty(const QString& t) { return t.isEmpty(); }
static inline bool tokenPromoted(const QString& t) { return (!t.isEmpty() && t.at(0) == QLatin1Char('+')); }
static inline QChar tokenBasePiece(const QString& t) {
    if (t.isEmpty()) return QChar();
    return t.back(); // '+P' → 'P', 'p' → 'p'
}

// SFENペアから 1手分の ShogiMove を復元（差分なし=終局などは false）
static bool deriveMoveFromSfenPair(const QString& prevSfen,
                                   const QString& nextSfen,
                                   ShogiMove* out)
{
    const QVector<QString> A = sfenBoardTo81Tokens(prevSfen);
    const QVector<QString> B = sfenBoardTo81Tokens(nextSfen);

    int fromIdx = -1, toIdx = -1;
    QString fromTok, toTokPrev, toTokNew;

    // どのセルが変わったか（詳細ログ用）
    QVector<int> diffs; diffs.reserve(4);

    for (int i = 0; i < 81; ++i) {
        const QString& a = A.at(i);
        const QString& b = B.at(i);
        if (a == b) continue;

        diffs.push_back(i);

        // 移動元：a に駒があり b が空
        if (!tokenEmpty(a) && tokenEmpty(b)) {
            fromIdx = i;
            fromTok = a;
            continue;
        }
        // 移動先：b に駒があり a と異なる
        if (!tokenEmpty(b) && a != b) {
            toIdx     = i;
            toTokNew  = b;
            toTokPrev = a; // 取りがあった場合は a に相手駒がいた
        }
    }

    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] deriveMoveFromSfenPair  diffs=" << diffs.size();
        for (int i = 0; i < diffs.size(); ++i) {
            const int d = diffs.at(i);
            qDebug().noquote()
                << "      diff[" << i << "] idx=" << d
                << "  A=\"" << A.at(d) << "\""
                << "  B=\"" << B.at(d) << "\""
                << "  " << idxHuman(d);
        }
        qDebug().noquote() << "      picked fromIdx=" << fromIdx
                           << (fromIdx>=0 ? (" tok=\""+fromTok+"\" "+idxHuman(fromIdx)) : QString())
                           << "  toIdx=" << toIdx
                           << (toIdx>=0 ? (" tokPrev=\""+toTokPrev+"\" tokNew=\""+toTokNew+"\" "+idxHuman(toIdx)) : QString());
    }

    if (fromIdx < 0 && toIdx < 0) {
        // 盤が全く同じ → 投了など「着手なし」
        if (kGM_VERBOSE) qDebug() << "[GM] no board delta (resign/terminal/comment only)";
        return false;
    }

    // 盤座標 ← idx
    auto idxToPointL2R     = [](int idx)->QPoint { return QPoint(idx % 9, idx / 9); };
    auto idxToPointFlipped = [](int idx)->QPoint { return QPoint(8 - (idx % 9), idx / 9); };

    // 出力フィールド（※最終的に out へ入れるのは FLIP 側）
    QPoint from(-1, -1), to(-1, -1);
    QChar moving, captured;
    bool isPromotion = false;

    if (fromIdx < 0 && toIdx >= 0) {
        // 打つ手（ドロップ）
        from = QPoint(-1, -1);
        // ★ 採用は FLIP 側
        to   = idxToPointFlipped(toIdx);
        moving    = tokenBasePiece(toTokNew);              // 駒台から打った駒
        captured  = QChar();                               // 取りは無い
        isPromotion = false;                               // 打ちは成りなし
    } else if (fromIdx >= 0 && toIdx >= 0) {
        // 通常移動（★ 採用は FLIP 側）
        from     = idxToPointFlipped(fromIdx);
        to       = idxToPointFlipped(toIdx);
        moving   = tokenBasePiece(fromTok);               // 元の升の駒（非成り形）
        captured = tokenEmpty(toTokPrev) ? QChar() : tokenBasePiece(toTokPrev);
        isPromotion = tokenPromoted(toTokNew);
    } else {
        if (kGM_VERBOSE) qDebug() << "[GM] inconsistent from/to detection";
        return false;
    }

    if (kGM_VERBOSE) {
        // 参考ログ：L2R と FLIP の両方を出す（採用は FLIP）
        QPoint l2rFrom(-1,-1), l2rTo(-1,-1);
        if (fromIdx >= 0) l2rFrom = idxToPointL2R(fromIdx);
        if (toIdx   >= 0) l2rTo   = idxToPointL2R(toIdx);

        QPoint flipFrom(-1,-1), flipTo(-1,-1);
        if (fromIdx >= 0) flipFrom = idxToPointFlipped(fromIdx);
        if (toIdx   >= 0) flipTo   = idxToPointFlipped(toIdx);

        qDebug().noquote()
            << "      L2R  from=" << l2rFrom << " to=" << l2rTo;
        qDebug().noquote()
            << "      FLIP from=" << flipFrom << " to=" << flipTo << "  <-- chosen";

        qDebug().noquote()
            << "      moving=" << moving
            << " captured=" << (captured.isNull() ? QChar(' ') : captured)
            << " promoted=" << (isPromotion ? "T" : "F");
    }

    if (out) *out = ShogiMove(from, to, moving, captured, isPromotion);
    return true;
}

using BCDI = ::BranchCandidateDisplayItem;

KifuLoadCoordinator::KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
                                         QVector<ResolvedRow>& resolvedRows,
                                         QStringList& positionStrList,
                                         int& activeResolvedRow,
                                         int& activePly,
                                         int& currentSelectedPly,
                                         int& currentMoveIndex,
                                         QStringList* sfenRecord,
                                         QTableWidget* gameInfoTable,
                                         QDockWidget* gameInfoDock,
                                         EngineAnalysisTab* analysisTab,
                                         QTabWidget* tab,
                                         ShogiView* shogiView,
                                         RecordPane* recordPane,
                                         KifuRecordListModel* kifuRecordModel,
                                         KifuBranchListModel* kifuBranchModel,
                                         BranchCandidatesController* branchCtl,
                                         QTableView* kifuBranchView,
                                         QObject* parent)
    : QObject(parent)
    , m_gameMoves(gameMoves)                // ← 参照メンバに束縛（同一実体を共有）
    , m_resolvedRows(resolvedRows)          // ← 同上
    , m_positionStrList(positionStrList)    // ← 同上
    , m_activeResolvedRow(activeResolvedRow)
    , m_activePly(activePly)
    , m_currentSelectedPly(currentSelectedPly)
    , m_currentMoveIndex(currentMoveIndex)
    , m_sfenRecord(sfenRecord)
    , m_gameInfoTable(gameInfoTable)
    , m_gameInfoDock(gameInfoDock)
    , m_analysisTab(analysisTab)
    , m_tab(tab)
    , m_shogiView(shogiView)
    , m_recordPane(recordPane)
    , m_kifuRecordModel(kifuRecordModel)
    , m_kifuBranchModel(kifuBranchModel)
    , m_branchCtl(branchCtl)
    , m_kifuBranchView(kifuBranchView)
{
    // 必要ならデバッグ時にチェック
    // Q_ASSERT(m_sfenRecord && "sfenRecord must not be null");
    // ここで初期同期が必要ならシグナル発火や内部初期化を追加してください。
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

    // 9) UIの整合
    emit enableArrowButtons();
    logImportSummary(filePath, m_usiMoves, disp, teaiLabel, parseWarn, QString());

    // 10) 解決済み行を構築（親探索規則で親子関係を決定）
    buildResolvedLinesAfterLoad();

    // 11) 分岐レポート → Plan 構築（Plan方式の基礎データ）
    dumpBranchSplitReport();
    emit buildBranchCandidateDisplayPlan();
    dumpBranchCandidateDisplayPlan();

    // 12) 分岐ツリーへ供給（黄色ハイライトは applyResolvedRowAndSelect 内で同期）
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rows;
        rows.reserve(m_resolvedRows.size());

        // ★ detach 回避：QList を const 化して range-for
        for (const auto& r : std::as_const(m_resolvedRows)) {
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = r.startPly;
            x.disp     = r.disp;
            x.sfen     = r.sfen;
            rows.push_back(std::move(x));
        }

        m_analysisTab->setBranchTreeRows(rows);
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }

    // 13) VariationEngine への投入（他機能で必要な可能性があるため保持）
    if (!m_varEngine) m_varEngine = std::make_unique<KifuVariationEngine>();
    {
        QVector<UsiMove> usiMain;
        usiMain.reserve(m_usiMoves.size());
        for (const auto& u : std::as_const(m_usiMoves)) usiMain.push_back(UsiMove(u));
        m_varEngine->ingest(res, m_sfenMain, usiMain, m_dispMain);
    }

    // ←ここで SFEN を各行に流し込む
    ensureResolvedRowsHaveFullSfen();
    // ←そして表を出す
    dumpAllRowsSfenTable();

    ensureResolvedRowsHaveFullGameMoves();

    dumpAllLinesGameMoves();

    // 14) （Plan方式化に伴い）WL 構築や従来の候補再計算は廃止
    // 15) ブランチ候補ワイヤリング（planActivated -> applyResolvedRowAndSelect）
    if (!m_branchCtl) {
        emit setupBranchCandidatesWiring_(); // 内部で planActivated の connect を済ませる
    }

    if (m_kifuBranchModel) {
        // 起動直後は候補を出さない（0手目）：モデルクリア＆ビュー非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
    }

    // ★ ロード完了 → 抑止解除
    m_loadingKifu = false;

    // 16) ツリーは読み込み後ロック（ユーザ操作で枝を生やさない）
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked after load";

    qDebug().noquote() << "[MAIN] loadKifuFromFile OUT";
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

void KifuLoadCoordinator::setResolvedRows(QVector<ResolvedRow> &newResolvedRows)
{
    m_resolvedRows = newResolvedRows;
}

void KifuLoadCoordinator::setActiveResolvedRow(int newActiveResolvedRow)
{
    m_activeResolvedRow = newActiveResolvedRow;
}

void KifuLoadCoordinator::setActivePly(int newActivePly)
{
    m_activePly = newActivePly;
}

void KifuLoadCoordinator::setCurrentSelectedPly(int newCurrentSelectedPly)
{
    m_currentSelectedPly = newCurrentSelectedPly;
}

void KifuLoadCoordinator::setCurrentMoveIndex(int newCurrentMoveIndex)
{
    m_currentMoveIndex = newCurrentMoveIndex;
}

void KifuLoadCoordinator::setKifuRecordModel(KifuRecordListModel *newKifuRecordModel)
{
    m_kifuRecordModel = newKifuRecordModel;
}

void KifuLoadCoordinator::setKifuBranchModel(KifuBranchListModel *newKifuBranchModel)
{
    m_kifuBranchModel = newKifuBranchModel;
}

void KifuLoadCoordinator::setBranchCtl(BranchCandidatesController *newBranchCtl)
{
    m_branchCtl = newBranchCtl;
}

void KifuLoadCoordinator::setKifuBranchView(QTableView *newKifuBranchView)
{
    m_kifuBranchView = newKifuBranchView;
}

void KifuLoadCoordinator::setBranchPlyContext(int newBranchPlyContext)
{
    m_branchPlyContext = newBranchPlyContext;
}

void KifuLoadCoordinator::setBranchablePlySet(const QSet<int> &newBranchablePlySet)
{
    m_branchablePlySet = newBranchablePlySet;
}

void KifuLoadCoordinator::setBranchIndex(const QHash<int, QHash<QString, QList<BranchCandidate> > > &newBranchIndex)
{
    m_branchIndex = newBranchIndex;
}

void KifuLoadCoordinator::setBranchDisplayPlan(const QHash<int, QMap<int, BranchCandidateDisplay> > &newBranchDisplayPlan)
{
    m_branchDisplayPlan = newBranchDisplayPlan;
}

void KifuLoadCoordinator::setVarEngine(std::unique_ptr<KifuVariationEngine> newVarEngine)
{
    m_varEngine = std::move(newVarEngine);
}

void KifuLoadCoordinator::setBranchTreeLocked(bool newBranchTreeLocked)
{
    m_branchTreeLocked = newBranchTreeLocked;
}

void KifuLoadCoordinator::setGameInfoDock(QDockWidget *newGameInfoDock)
{
    m_gameInfoDock = newGameInfoDock;
}

void KifuLoadCoordinator::setLoadingKifu(bool newLoadingKifu)
{
    m_loadingKifu = newLoadingKifu;
}

void KifuLoadCoordinator::setAnalysisTab(EngineAnalysisTab *newAnalysisTab)
{
    m_analysisTab = newAnalysisTab;
}

void KifuLoadCoordinator::setUsiMoves(const QStringList &newUsiMoves)
{
    m_usiMoves = newUsiMoves;
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

KifuLoadCoordinator::BranchRowDelegate::BranchRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

KifuLoadCoordinator::BranchRowDelegate::~BranchRowDelegate() = default;

void KifuLoadCoordinator::BranchRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    QStyledItemDelegate::initStyleOption(&opt, index);

    const bool isBranchable = (m_marks && m_marks->contains(index.row()));

    // 選択時のハイライトと衝突しないように、未選択時のみオレンジ背景
    if (isBranchable && !(opt.state & QStyle::State_Selected)) {
        painter->save();
        painter->fillRect(opt.rect, QColor(255, 220, 160));
        painter->restore();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

void KifuLoadCoordinator::logImportSummary(const QString& filePath,
                                  const QStringList& usiMoves,
                                  const QList<KifDisplayItem>& disp,
                                  const QString& teaiLabel,
                                  const QString& warnParse,
                                  const QString& warnConvert) const
{
    if (!warnParse.isEmpty())
        qWarning().noquote() << "[KIF parse warnings]\n" << warnParse.trimmed();
    if (!warnConvert.isEmpty())
        qWarning().noquote() << "[KIF convert warnings]\n" << warnConvert.trimmed();

    qDebug().noquote() << QStringLiteral("KIF読込: %1手（%2）")
                              .arg(usiMoves.size())
                              .arg(QFileInfo(filePath).fileName());
    for (int i = 0; i < qMin(5, usiMoves.size()); ++i) {
        qDebug().noquote() << QStringLiteral("USI[%1]: %2")
        .arg(i + 1)
            .arg(usiMoves.at(i));
    }

    qDebug().noquote() << QStringLiteral("手合割: %1")
                              .arg(teaiLabel.isEmpty()
                                       ? QStringLiteral("平手(既定)")
                                       : teaiLabel);

    // 本譜（表示用）。コメントがあれば直後に出力。
    for (const auto& it : disp) {
        const QString time = it.timeText.isEmpty()
        ? QStringLiteral("00:00/00:00:00")
        : it.timeText;
        qDebug().noquote() << QStringLiteral("「%1」「%2」").arg(it.prettyMove, time);
        if (!it.comment.trimmed().isEmpty()) {
            qDebug().noquote() << QStringLiteral("  └ コメント: %1")
                                      .arg(it.comment.trimmed());
        }
    }

    // SFEN（抜粋）
    if (m_sfenRecord) {
        for (int i = 0; i < qMin(12, m_sfenRecord->size()); ++i) {
            qDebug().noquote() << QStringLiteral("%1) %2")
            .arg(i)
                .arg(m_sfenRecord->at(i));
        }
    }

    // m_gameMoves（従来通り）
    qDebug() << "m_gameMoves size:" << m_gameMoves.size();
    for (int i = 0; i < m_gameMoves.size(); ++i) {
        qDebug().noquote() << QString("%1) ").arg(i + 1) << m_gameMoves[i];
    }
}

void KifuLoadCoordinator::buildResolvedLinesAfterLoad()
{
    // 既存: Resolved行の構築や m_varEngine->ingest(), rebuildBranchWhitelist() の後
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked (after buildResolvedLinesAfterLoad)";

    // 本関数は「後勝ち」をやめ、各変化の“親”を
    // 直前→さらに前…と遡って「b(=この変化の startPly) > a(=先行変化の startPly)」
    // を満たす最初の変化行にする（無ければ本譜 row=0）という規則で構築する。

    m_resolvedRows.clear();

    // --- 行0 = 本譜 ---
    ResolvedRow mainRow;
    mainRow.startPly = 1;
    mainRow.parent   = -1;           // ★追加：本譜の親は -1 を明示
    mainRow.disp     = m_dispMain;   // 1..N（表示）
    mainRow.sfen     = m_sfenMain;   // 0..N（局面）
    mainRow.gm       = m_gmMain;     // 1..N（USIムーブ）
    mainRow.varIndex = -1;           // 本譜
    m_resolvedRows.push_back(mainRow);

    if (m_variationsSeq.isEmpty()) {
        qDebug().noquote() << "[RESOLVED] only mainline (no variations).";
        return;
    }

    // 変化 vi → 解決済み行 index の写像（empty の変化は -1）
    QVector<int> varToRowIndex(m_variationsSeq.size(), -1);

    // 親行を解決するローカルラムダ：
    // 直前→さらに前…へ遡り、startPly(prev) < startPly(cur) を満たす
    // 先行変化を見つけたら、その変化の“解決済み行 index”を返す。無ければ 0（本譜）。
    auto resolveParentRowIndex = [&](int vi)->int {
        const int b = qMax(1, m_variationsSeq.at(vi).startPly);
        for (int prev = vi - 1; prev >= 0; --prev) {
            const KifLine& p = m_variationsSeq.at(prev);
            if (p.disp.isEmpty()) continue;          // 空の変化は親候補にしない
            const int a = qMax(1, p.startPly);
            if (b > a && varToRowIndex.at(prev) >= 0) {
                return varToRowIndex.at(prev);        // その変化の“行 index”
            }
        }
        return 0; // 見つからなければ本譜
    };

    // --- 行1.. = 各変化 ---
    for (int vi = 0; vi < m_variationsSeq.size(); ++vi) {
        const KifLine& v = m_variationsSeq.at(vi);
        if (v.disp.isEmpty()) {
            varToRowIndex[vi] = -1;
            continue;
        }

        const int start = qMax(1, v.startPly);   // 1-origin
        const int parentRow = resolveParentRowIndex(vi);
        const ResolvedRow& base = m_resolvedRows.at(parentRow);

        // 1) プレフィクス（1..start-1）を“親行”から切り出し、足りない分は本譜で補完
        QList<KifDisplayItem> prefixDisp = base.disp;
        if (prefixDisp.size() > start - 1) prefixDisp.resize(start - 1);

        QVector<ShogiMove> prefixGm = base.gm;
        if (prefixGm.size() > start - 1) prefixGm.resize(start - 1);

        QStringList prefixSfen = base.sfen;
        if (prefixSfen.size() > start) prefixSfen.resize(start);

        // 親行が短い場合は本譜で延長（disp/gm/sfen を一致させる）
        while (prefixDisp.size() < start - 1 && prefixDisp.size() < m_dispMain.size()) {
            const int idx = prefixDisp.size(); // 0.. → 手数は idx+1
            prefixDisp.append(m_dispMain.at(idx));
        }
        while (prefixGm.size() < start - 1 && prefixGm.size() < m_gmMain.size()) {
            const int idx = prefixGm.size();
            prefixGm.append(m_gmMain.at(idx));
        }
        while (prefixSfen.size() < start && prefixSfen.size() < m_sfenMain.size()) {
            const int sidx = prefixSfen.size(); // 0.. → sfen は 0=初期,1=1手後...
            prefixSfen.append(m_sfenMain.at(sidx));
        }

        // 2) 変化本体を連結
        ResolvedRow row;
        row.startPly = start;
        row.parent   = parentRow;    // ★追加：親行 index を保存
        row.varIndex = vi;

        row.disp = prefixDisp;
        row.disp += v.disp;

        row.gm = prefixGm;
        row.gm += v.gameMoves;

        // v.sfenList は [0]=基底（start-1 手後）, [1..]=各手後 を想定
        row.sfen = prefixSfen;                  // 0..start-1 は既に parent→本譜で整合
        if (v.sfenList.size() >= 2) {
            row.sfen += v.sfenList.mid(1);      // start.. の各手後
        } else if (v.sfenList.size() == 1) {
            // 念のため：基底のみしか無い場合でも、prefix は揃っているので何もしない
        }

        // 追加
        m_resolvedRows.push_back(row);
        varToRowIndex[vi] = m_resolvedRows.size() - 1;
    }

    qDebug().noquote() << "[RESOLVED] lines=" << m_resolvedRows.size();

    auto varIdOf = [&](int varIndex)->int {
        if (varIndex < 0) return -1; // 本譜
        if (m_varEngine) {
            const int id = m_varEngine->varIdForSourceIndex(varIndex);
            return (id >= 0) ? id : (varIndex + 1); // フォールバック
        }
        return varIndex + 1; // 既存仕様に準拠（空行スキップ条件が一致している前提）
    };

    for (int i = 0; i < m_resolvedRows.size(); ++i) {
        const auto& r = m_resolvedRows[i];
        QStringList pm; pm.reserve(r.disp.size());
        for (const auto& d : r.disp) pm << d.prettyMove;

        const bool isMain = (i == 0);
        const QString label = isMain
                                  ? "Main"
                                  : QString("Var%1(id=%2)").arg(r.varIndex).arg(varIdOf(r.varIndex)); // ★ id 併記

        // ★修正：表示も保存済みの r.parent を使用（再計算しない）
        qDebug().noquote()
            << "  [" << label << "]"
            << " parent=" << r.parent
            << " start="  << r.startPly
            << " ndisp="  << r.disp.size()
            << " nsfen="  << r.sfen.size()
            << " seq=「 " << pm.join(" / ") << " 」";
    }
}

// 行番号から表示名を作る（Main / VarN）
QString KifuLoadCoordinator::rowNameFor_(int row) const
{
    if (row < 0 || row >= m_resolvedRows.size()) return QString("<?>");
    const auto& rr = m_resolvedRows[row];
    return (rr.varIndex < 0) ? QStringLiteral("Main")
                             : QStringLiteral("Var%1").arg(rr.varIndex);
}

// 1始まり hand-ply のラベル（無ければ ""）
QString KifuLoadCoordinator::labelAt_(const ResolvedRow& rr, int ply) const
{
    const int li = ply - 1;
    if (li < 0 || li >= rr.disp.size()) return QString();
    return pickLabelForDisp(rr.disp.at(li));
}

// 1..p までの完全一致（両方に手が存在し、かつ全ラベル一致）なら true
bool KifuLoadCoordinator::prefixEqualsUpTo_(int rowA, int rowB, int p) const
{
    if (rowA < 0 || rowA >= m_resolvedRows.size()) return false;
    if (rowB < 0 || rowB >= m_resolvedRows.size()) return false;
    const auto& A = m_resolvedRows[rowA];
    const auto& B = m_resolvedRows[rowB];
    for (int k = 1; k <= p; ++k) {
        const QString a = labelAt_(A, k);
        const QString b = labelAt_(B, k);
        if (a.isEmpty() || b.isEmpty() || a != b) return false;
    }
    return true;
}

void KifuLoadCoordinator::dumpBranchSplitReport() const
{
    if (m_resolvedRows.isEmpty()) return;

    // 行ごとに出力
    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        const QString header = rowNameFor_(r);
        qDebug().noquote() << header;

        const int maxPly = rr.disp.size();
        for (int p = 1; p <= maxPly; ++p) {
            const QString curLbl = labelAt_(rr, p);
            // この行と「1..p まで完全一致」する仲間を抽出
            QList<int> group;
            for (int j = 0; j < m_resolvedRows.size(); ++j) {
                if (prefixEqualsUpTo_(r, j, p)) group << j;
            }

            // その仲間たちの「次手 (p+1)」を集計（存在するものだけ）
            struct NextMove { QString who; QString lbl; };
            QVector<NextMove> nexts; nexts.reserve(group.size());
            QSet<QString> uniq;

            // ★ detach 回避：QList を const 化して range-for
            for (int j : std::as_const(group)) {
                const QString lblNext = labelAt_(m_resolvedRows[j], p + 1);
                if (lblNext.isEmpty()) continue; // ここで終端は候補に出さない
                const QString who = rowNameFor_(j);
                nexts.push_back({who, lblNext});
                uniq.insert(lblNext);
            }

            const QString curOrEmpty = curLbl.isEmpty() ? QStringLiteral("<EMPTY>") : curLbl;

            if (uniq.size() > 1) {
                // 分岐あり：各ライン名と次手を「、」で列挙
                QStringList parts;
                parts.reserve(nexts.size());
                for (const auto& nm : nexts) {
                    parts << QStringLiteral("%1 %2").arg(nm.who, nm.lbl);
                }

                qDebug().noquote()
                    << QStringLiteral("%1 %2 分岐あり %3")
                           // ★ multi-arg を使用（p は number 化）
                           .arg(QString::number(p),
                                curOrEmpty,
                                parts.join(QStringLiteral("、")));
            } else {
                qDebug().noquote()
                << QStringLiteral("%1 %2 分岐なし")
                        // ★ multi-arg を使用
                        .arg(QString::number(p), curOrEmpty);
            }
        }

        // 行間の空行
        qDebug().noquote() << "";
    }
}

/*
void KifuLoadCoordinator::buildBranchCandidateDisplayPlan()
{
    m_branchDisplayPlan.clear();

    const int R = m_resolvedRows.size();
    if (R == 0) return;

    auto labelAt = [&](int row, int li)->QString {
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    auto prefixEquals = [&](int r1, int r2, int uptoLi)->bool {
        // li=0 のときは「初手より前の共通部分」は空なので常に一致とみなす
        for (int i = 0; i < uptoLi; ++i) {
            if (labelAt(r1, i) != labelAt(r2, i)) return false;
        }
        return true;
    };

    // 各行 r の各ローカル添字 li（0-based）について、
    // 「初手から li-1 まで完全一致する行」をグループ化し、
    // その li 手目に 2 種類以上の指し手があれば分岐とみなす。
    // ★ 表示は “その手（li）” に出す（＝1手先に送らない）
    for (int r = 0; r < R; ++r) {
        const int len = m_resolvedRows[r].disp.size();
        if (len == 0) continue;

        for (int li = 0; li < len; ++li) {
            // この行 r と「初手から li-1 まで一致」する行
            QVector<int> group;
            group.reserve(R);
            for (int g = 0; g < R; ++g) {
                if (li < m_resolvedRows[g].disp.size() && prefixEquals(r, g, li)) {
                    group.push_back(g);
                }
            }
            if (group.size() <= 1) continue; // 比較相手がいない

            // グループの li 手目ラベルを集計
            QHash<QString, QVector<int>> labelToRows;
            for (int g : group) {
                const QString lbl = labelAt(g, li);
                labelToRows[lbl].push_back(g);
            }
            if (labelToRows.size() <= 1) continue; // 全員同じ指し手 → 分岐ではない

            // 表示先 ply（1-based）。li は 0-based
            const int targetPly = li + 1;
            if (targetPly > m_resolvedRows[r].disp.size()) continue;

            // 見出し（この行 r の li 手目）
            const QString baseForDisplay = labelAt(r, li);

            // ===== 重複整理（自分の行 > Main(=row0) > 若い VarN）=====
            struct TmpKeep { QString lbl; int keepRow; };
            QVector<TmpKeep> keeps; keeps.reserve(labelToRows.size());

            for (auto it = labelToRows.constBegin(); it != labelToRows.constEnd(); ++it) {
                const QString lbl = it.key();
                const QVector<int>& rowsWithLbl = it.value();

                int keep = -1;

                // 1) 自分の行 r を最優先
                bool hasSelf = false;
                for (int cand : rowsWithLbl) {
                    if (cand == r) { keep = cand; hasSelf = true; break; }
                }

                // 2) 自分が無ければ Main(row=0)
                if (!hasSelf) {
                    bool hasMain = false;
                    for (int cand : rowsWithLbl) {
                        if (cand == 0) { keep = 0; hasMain = true; break; }
                    }

                    // 3) それも無ければ最小 row（VarN の若い方）
                    if (!hasMain) {
                        keep = rowsWithLbl.first();
                        for (int cand : rowsWithLbl) {
                            if (cand < keep) keep = cand;
                        }
                    }
                }

                keeps.push_back({ lbl, keep });
            }

            // 表示順: Main が先、次に row 昇順
            std::sort(keeps.begin(), keeps.end(), [](const TmpKeep& a, const TmpKeep& b){
                if (a.keepRow == 0 && b.keepRow != 0) return true;
                if (a.keepRow != 0 && b.keepRow == 0) return false;
                return a.keepRow < b.keepRow;
            });

            QVector<BranchCandidateDisplayItem> items;
            items.reserve(keeps.size());
            for (const auto& k : keeps) {
                BranchCandidateDisplayItem itx;
                itx.row      = k.keepRow;
                itx.varN     = (k.keepRow == 0 ? -1 : k.keepRow - 1);
                itx.lineName = lineNameForRow(k.keepRow); // "Main" / "VarN"
                itx.label    = k.lbl;
                items.push_back(itx);
            }

            // 保存
            BranchCandidateDisplay plan;
            plan.ply       = targetPly;
            plan.baseLabel = baseForDisplay;
            plan.items     = std::move(items);
            m_branchDisplayPlan[r].insert(targetPly, std::move(plan));
        }
    }
}
*/

void KifuLoadCoordinator::dumpBranchCandidateDisplayPlan() const
{
    if (m_resolvedRows.isEmpty()) return;

    auto labelAt = [&](int row, int ply1)->QString {
        // ply1 は 1-based
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    // 行ごとに
    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        qDebug().noquote() << (r == 0 ? "Main" : QString("Var%1").arg(r - 1));

        const int len = m_resolvedRows[r].disp.size();
        const auto itRow = m_branchDisplayPlan.constFind(r);

        for (int ply1 = 1; ply1 <= len; ++ply1) {
            const QString base = labelAt(r, ply1);
            bool has = false;
            QVector<BranchCandidateDisplayItem> items;

            if (itRow != m_branchDisplayPlan.constEnd()) {
                const auto& mp = itRow.value();
                auto itP = mp.constFind(ply1);
                if (itP != mp.constEnd()) {
                    has   = true;
                    items = itP.value().items;
                }
            }

            if (!has) {
                const QString baseOrEmpty = base.isEmpty() ? QStringLiteral("<EMPTY>") : base;
                qDebug().noquote()
                    << QStringLiteral("%1 %2 分岐候補表示なし")
                           .arg(QString::number(ply1), baseOrEmpty);
            } else {
                // "Main ▲…、Var0 ▲…、Var2 ▲…" の並びを構築
                QStringList parts;
                parts.reserve(items.size());
                for (const auto& it : std::as_const(items)) {  // ← detach 回避
                    parts << QStringLiteral("%1 %2").arg(it.lineName, it.label);
                }
                const QString baseOrEmpty = base.isEmpty() ? QStringLiteral("<EMPTY>") : base;
                qDebug().noquote()
                    << QStringLiteral("%1 %2 分岐候補表示あり %3")
                           .arg(QString::number(ply1),
                                baseOrEmpty,
                                parts.join(QStringLiteral("、")));  // ← multi-arg
            }
        }

        // 行間の空行
        qDebug().noquote() << "";
    }
}

void KifuLoadCoordinator::ensureResolvedRowsHaveFullSfen()
{
    if (m_resolvedRows.isEmpty()) return;

    qDebug() << "[SFEN] ensureResolvedRowsHaveFullSfen BEGIN";

    // Main の SFEN（VEが空なら既存の行0を使う）
    QStringList veMain = (m_varEngine ? m_varEngine->mainlineSfen() : QStringList());
    if (veMain.isEmpty() && !m_resolvedRows[0].sfen.isEmpty())
        veMain = m_resolvedRows[0].sfen;

    auto sfenFromVeForRow = [&](const ResolvedRow& rr)->QStringList {
        if (rr.varIndex < 0) {
            // 本譜
            return veMain;
        }
        if (!m_varEngine) return {};
        const int vid = m_varEngine->variationIdFromSourceIndex(rr.varIndex);
        return m_varEngine->sfenForVariationId(vid);
    };

    const int rowCount = m_resolvedRows.size();
    for (int r = 0; r < rowCount; ++r) {
        auto& rr = m_resolvedRows[r];

        const int need = rr.disp.size() + 1;     // 0..N
        const int s    = qMax(1, rr.startPly);   // 1-origin
        const int base = s - 1;                  // 直前局面の添字

        // 親行（無ければ Main=0）
        int parentRow = 0;
        if (rr.parent >= 0 && rr.parent < rowCount) {
            parentRow = rr.parent;
        }

        // 親のプレフィクス（親が空なら mainline を使う）
        const QStringList parentPrefix =
            (!m_resolvedRows[parentRow].sfen.isEmpty()
                 ? m_resolvedRows[parentRow].sfen
                 : veMain);

        // この行の VE 配列（base を先頭とする 0..M）
        const QStringList veRow = sfenFromVeForRow(rr);

        // 合成先。既存をベースにするが、0..base は必ず親で上書きする
        QStringList full = rr.sfen;

        // --- 1) 0..base を親の SFEN で「強制上書き」 -----------------------
        //     ※ ここが今回の肝：古い/別行の値が残らないようにする
        for (int i = 0; i <= base; ++i) {
            if (i >= parentPrefix.size()) break;   // 親に無ければ打ち切り
            const QString pv = parentPrefix.at(i);
            if (i < full.size()) {
                full[i] = pv;                      // 常に上書き
            } else if (!pv.isEmpty()) {
                full.append(pv);                   // 既知のものだけ追加
            } else {
                break;                             // 空は追加しない
            }
        }

        // デバッグ（境界確認）
        auto hashS = [](const QString& s){ return qHash(s); };
        auto safeAt = [&](const QStringList& a, int i)->QString{
            return (0<=i && i<a.size() ? a.at(i) : QString());
        };

        qDebug().noquote()
            << "[SFEN] row=" << r
            << " base=" << base
            << " pre(base-1)#=" << hashS(safeAt(full, base-1))
            << " pre(base)#="   << hashS(safeAt(full, base))
            << " pre(base+1)#=" << hashS(safeAt(full, base+1));


        // --- 2) VE の部分配列を base から重ねる（空は書かない／ギャップは埋めない） ---
        bool gap = false;
        if (!veRow.isEmpty()) {
            for (int j = 0; j < veRow.size(); ++j) {
                const int pos = base + j;
                const QString v = veRow.at(j);
                if (v.isEmpty()) continue;

                if (pos < full.size()) {
                    full[pos] = v;
                } else if (pos == full.size()) {
                    full.append(v);
                } else {
                    // 間の index が未充足（空で埋めない方針なので打ち切り）
                    gap = true;
                    break;
                }
            }
        } else {
            // veRow が空：壊さずスキップ（警告のみ）
            if (full.size() < need) {
                qWarning().noquote()
                << "[SFEN] SKIP(fill) row=" << r
                << " startPly=" << rr.startPly
                << " need=" << need
                << " prefix(sz=" << parentPrefix.size() << ")"
                << " veRow(sz=0)  -- keep r.sfen as-is";
            }
        }

        // --- 3) 検査＆警告 ---------------------------------------------------
        bool missing = (full.size() < need);
        if (!missing) {
            for (int i = 0; i < need; ++i) {
                if (full.at(i).isEmpty()) { missing = true; break; }
            }
        }
        if (missing || gap) {
            qWarning().noquote()
            << "[SFEN] WARN(fill) row=" << r
            << " startPly=" << rr.startPly
            << " need=" << need
            << " prefix(sz=" << parentPrefix.size() << ")"
            << " veRow(sz=" << veRow.size() << ")"
            << (gap ? " GAP" : "");
        }

        // デバッグ（境界確認：合成後）

        qDebug().noquote()
            << "[SFEN] row=" << r
            << " post(base-1)#=" << hashS(safeAt(full, base-1))
            << " post(base)#="   << hashS(safeAt(full, base))
            << " post(base+1)#=" << hashS(safeAt(full, base+1));


        rr.sfen = std::move(full);
    }

    qDebug() << "[SFEN] ensureResolvedRowsHaveFullSfen END";
}

void KifuLoadCoordinator::dumpAllRowsSfenTable() const
{
    if (m_resolvedRows.isEmpty()) return;

    auto labelAt = [&](int row, int ply1)->QString {
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        qDebug().noquote() << (r == 0 ? "Main" : QString("Var%1").arg(r - 1));

        // 0: 開始局面
        const QString s0 = (!rr.sfen.isEmpty() ? rr.sfen.first() : QStringLiteral("<SFEN MISSING>"));
        qDebug().noquote() << QStringLiteral("0 開始局面 %1").arg(s0);

        // 1..N
        for (int ply1 = 1; ply1 <= rr.disp.size(); ++ply1) {
            const QString lbl  = labelAt(r, ply1);
            QString sfen = (ply1 >= 0 && ply1 < rr.sfen.size()) ? rr.sfen.at(ply1) : QString();
            if (sfen.isEmpty()) sfen = QStringLiteral("<SFEN MISSING>");

            qDebug().noquote()
                << QStringLiteral("%1 %2 %3")
                       .arg(QString::number(ply1),
                            lbl.isEmpty() ? QStringLiteral("<EMPTY>") : lbl,
                            sfen);
        }
        qDebug().noquote() << "";
    }
}

// 各 ResolvedRow の SFEN 列から gm（ShogiMove 列）を復元する（詳細ログ付き）
void KifuLoadCoordinator::ensureResolvedRowsHaveFullGameMoves()
{
    qDebug() << "[GM] ensureResolvedRowsHaveFullGameMoves BEGIN";
    for (int i = 0; i < m_resolvedRows.size(); ++i) {
        auto& r = m_resolvedRows[i];

        const int nsfen = r.sfen.size();     // 0..N
        const int ndisp = r.disp.size();     // 1..N
        // resign など「盤が変わらない終端」は 1 手として数えないので、基本は min(ndisp, nsfen-1)
        const int want  = qMax(0, qMin(ndisp, nsfen - 1));

        const QString label = (i == 0)
                                  ? QStringLiteral("Main")
                                  : QStringLiteral("Var%1(id=%2)")
                                        .arg(r.varIndex)
                                        .arg(m_varEngine ? m_varEngine->varIdForSourceIndex(r.varIndex)
                                                         : (r.varIndex + 1));

        qDebug().noquote()
            << QString("[GM] row=%1 \"%2\" start=%3 nsfen=%4 ndisp=%5 current_gm=%6 want=%7")
                   .arg(i).arg(label).arg(r.startPly).arg(nsfen).arg(ndisp).arg(r.gm.size()).arg(want);

        // サイズが一致していればスキップ（必要に応じて強制再構築したい場合はこの if を外す）
        if (r.gm.size() == want) {
            qDebug().noquote() << QString("[GM] row=%1 keep (size match)").arg(i);
            continue;
        }

        r.gm.clear();

        for (int ply1 = 1; ply1 <= want; ++ply1) {
            const QString prev = r.sfen.at(ply1 - 1);
            const QString next = r.sfen.at(ply1);
            const QString pretty = r.disp.at(ply1 - 1).prettyMove;

            ShogiMove mv;
            const bool ok = deriveMoveFromSfenPair(prev, next, &mv); // ★ 内部で FLIP（USI向き）を採用

            if (!ok) {
                // 盤が同一 → 投了やコメントのみなど（gm へは積まない）
                qDebug().noquote()
                    << QString("[GM] row=%1 ply=%2 \"%3\" : NO-DELTA (terminal/comment)")
                           .arg(i).arg(ply1).arg(pretty);
                continue;
            }

            r.gm.push_back(mv);

            auto qcharToStr = [](QChar c)->QString { return c.isNull() ? QString(" ") : QString(c); };
            qDebug().noquote()
                << QStringLiteral("[GM] row=%1 ply=%2 \"%3\"  From:(%4,%5) To:(%6,%7) Moving:%8 Captured:%9 Promotion:%10")
                       .arg(QString::number(i),
                            QString::number(ply1),
                            pretty)
                       .arg(QString::number(mv.fromSquare.x()),
                            QString::number(mv.fromSquare.y()),
                            QString::number(mv.toSquare.x()))
                       .arg(QString::number(mv.toSquare.y()),
                            qcharToStr(mv.movingPiece),
                            qcharToStr(mv.capturedPiece))
                       .arg(mv.isPromotion ? QStringLiteral("true") : QStringLiteral("false"));
        }

        qDebug().noquote()
            << QString("[GM] row=%1 built gm.size=%2 / want=%3").arg(i).arg(r.gm.size()).arg(want);
    }
    qDebug() << "[GM] ensureResolvedRowsHaveFullGameMoves END";
}

void KifuLoadCoordinator::dumpAllLinesGameMoves() const
{
    if (m_resolvedRows.isEmpty()) return;

    // 終局判定（見た目ベース）
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

    auto lineNameFor = [](int row)->QString {
        return (row == 0) ? QStringLiteral("Main")
                          : QStringLiteral("Var%1").arg(row - 1);
    };
    auto labelAt = [&](const ResolvedRow& rr, int li)->QString {
        return (li >= 0 && li < rr.disp.size())
                 ? pickLabelForDisp(rr.disp.at(li))
                 : QString();
    };
    auto fmtPt = [](const QPoint& p)->QString {
        return QStringLiteral("(%1,%2)").arg(p.x()).arg(p.y());
    };

    // QChar/char/整数 いずれでも文字列化できる汎用ヘルパ
    auto pieceToQString = [](auto v)->QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, QChar>) {
            return v.isNull() ? QString() : QString(v);
        } else if constexpr (std::is_same_v<T, char>) {
            return v ? QString(QChar(v)) : QString();
        } else if constexpr (std::is_integral_v<T>) {
            return v ? QString(QChar(static_cast<ushort>(v))) : QString();
        } else {
            return QString();
        }
    };

    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        qDebug().noquote() << lineNameFor(r);

        // 0) 開始局面（From/To 等は出さない）
        qDebug().noquote() << "0 開始局面";

        const int M = rr.disp.size();
        for (int li = 0; li < M; ++li) {
            const QString pretty = labelAt(rr, li);
            const bool terminal  = isTerminalPretty(pretty);

            // ヘッダ（手数 + 見た目ラベル）
            const QString head = QStringLiteral("%1 %2").arg(li + 1).arg(pretty);

            if (terminal || li >= rr.gm.size()) {
                // 投了など or GM不足 → そのまま
                qDebug().noquote() << head;
                continue;
            }

            const ShogiMove& mv = rr.gm.at(li);

            // ドロップ（打つ手）は fromSquare が無効座標の可能性
            const bool hasFrom = (mv.fromSquare.x() >= 0 && mv.fromSquare.y() >= 0);
            const QString fromS = hasFrom ? fmtPt(mv.fromSquare) : QString();
            const QString toS   = fmtPt(mv.toSquare);

            const QString moving = pieceToQString(mv.movingPiece);
            const QString cap    = pieceToQString(mv.capturedPiece);
            const bool promoted  = mv.isPromotion;   // ← 正しいフィールド名

            qDebug().noquote()
                << head
                << " From:" << fromS
                << " To:" << toS
                << " Moving:" << moving
                << " Captured:" << cap
                << " Promotion:" << (promoted ? "true" : "false");
        }

        qDebug().noquote() << "";
    }
}
