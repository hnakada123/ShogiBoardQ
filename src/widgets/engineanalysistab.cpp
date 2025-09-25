#include "engineanalysistab.h"

#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QHeaderView>
#include <QFont>
#include <QMouseEvent>
#include <QtMath>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QQueue>
#include <QSet>

#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

// ===================== コンストラクタ/UI =====================

EngineAnalysisTab::EngineAnalysisTab(QWidget* parent)
    : QWidget(parent)
{
}

void EngineAnalysisTab::buildUi()
{
    // --- QTabWidget を再入安全に用意（親なしで生成して MainWindow 側で貼る） ---
    if (!m_tab) {
        m_tab = new QTabWidget(nullptr);                // 親なし → addWidget 時に reparent
        m_tab->setObjectName(QStringLiteral("analysisTabs"));
    } else {
        m_tab->clear();                                 // 同じ m_tab を再利用（重複生成しない）
    }

    // --- 思考タブ ---
    QWidget* page = new QWidget(m_tab);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(4,4,4,4);
    v->setSpacing(4);

    m_info1 = new EngineInfoWidget(page);
    m_view1 = new QTableView(page);
    m_info2 = new EngineInfoWidget(page);
    m_view2 = new QTableView(page);

    if (auto* hh = m_view1->horizontalHeader()) hh->setSectionResizeMode(4, QHeaderView::Stretch);
    if (auto* hh2 = m_view2->horizontalHeader()) hh2->setSectionResizeMode(4, QHeaderView::Stretch);

    v->addWidget(m_info1);
    v->addWidget(m_view1, 1);
    v->addWidget(m_info2);
    v->addWidget(m_view2, 1);

    m_tab->addTab(page, tr("思考"));

    // --- USI通信ログ ---
    m_usiLog = new QPlainTextEdit(m_tab);
    m_usiLog->setReadOnly(true);
    m_tab->addTab(m_usiLog, tr("USI通信ログ"));

    // --- 棋譜コメント ---
    m_comment = new QTextBrowser(m_tab);
    m_comment->setOpenExternalLinks(true);
    m_tab->addTab(m_comment, tr("棋譜コメント"));

    // --- 分岐ツリー ---
    m_branchTree = new QGraphicsView(m_tab);
    m_branchTree->setRenderHint(QPainter::Antialiasing, true);
    m_branchTree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_branchTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_branchTree->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_scene = new QGraphicsScene(m_branchTree);
    m_branchTree->setScene(m_scene);

    m_tab->addTab(m_branchTree, tr("分岐ツリー"));

    // ★ 重要：EngineAnalysisTab 自身のレイアウトに m_tab を add しない（MainWindow 側で add）
    // （ここに outer レイアウトは作らない）

    // --- 分岐ツリーのクリック検知（二重 install 防止のガード付き） ---
    if (m_branchTree && m_branchTree->viewport()) {
        QObject* vp = m_branchTree->viewport();
        if (!vp->property("branchFilterInstalled").toBool()) {
            vp->installEventFilter(this);
            vp->setProperty("branchFilterInstalled", true);
        }
    }
}

void EngineAnalysisTab::setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                                  UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_model1 = m1;
    m_model2 = m2;
    m_log1 = log1;
    m_log2 = log2;

    if (m_view1 && m1) m_view1->setModel(m1);
    if (m_view2 && m2) m_view2->setModel(m2);

    if (m_info1 && log1) m_info1->setModel(log1);
    if (m_info2 && log2) m_info2->setModel(log2);

    if (m_log1) {
        connect(m_log1, &UsiCommLogModel::usiCommLogChanged, this, [this]() {
            if (m_usiLog) m_usiLog->appendPlainText(m_log1->usiCommLog());
        });
    }
    if (m_log2) {
        connect(m_log2, &UsiCommLogModel::usiCommLogChanged, this, [this]() {
            if (m_usiLog) m_usiLog->appendPlainText(m_log2->usiCommLog());
        });
    }
}

QTabWidget* EngineAnalysisTab::tab() const { return m_tab; }

void EngineAnalysisTab::setAnalysisVisible(bool on)
{
    if (this->isVisible() != on) this->setVisible(on);
}

void EngineAnalysisTab::setCommentHtml(const QString& html)
{
    if (m_comment) m_comment->setHtml(html);
}

// ===================== 分岐ツリー・データ設定 =====================

void EngineAnalysisTab::setBranchTreeRows(const QVector<ResolvedRowLite>& rows)
{
    m_rows = rows;
    rebuildBranchTree();
}

// ===================== ノード/エッジ描画（＋登録） =====================

// ノード（指し手札）を描く：row=0(本譜)/1..(分岐), ply=手数(1始まり), rawText=指し手
QGraphicsPathItem* EngineAnalysisTab::addNode(int row, int ply, const QString& rawText)
{
    // レイアウト
    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;   // ← 現在使っているオフセットと揃える
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(QStringLiteral("Noto Sans JP"), 10);
    static const    QFont MOVE_NO_FONT(QStringLiteral("Noto Sans JP"), 9);

    const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
    const qreal y = BASE_Y + row * STEP_Y;

    // 先頭の手数数字（全角/半角）を除去
    static const QRegularExpression kDropHeadNumber(QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
    QString labelText = rawText;
    labelText.replace(kDropHeadNumber, QString());
    labelText = labelText.trimmed();

    const bool odd = (ply % 2) == 1; // 奇数=先手、偶数=後手

    // ★ 分岐も本譜と同じ配色に統一
    const QColor mainOdd (196, 230, 255); // 先手=水色
    const QColor mainEven(255, 223, 196); // 後手=ピーチ
    const QColor fill = odd ? mainOdd : mainEven;

    // 札サイズ
    const QFontMetrics fm(LABEL_FONT);
    const int  wText = fm.horizontalAdvance(labelText);
    const int  hText = fm.height();
    const qreal padX = 12.0, padY = 6.0;
    const qreal rectW = qMax<qreal>(70.0, wText + padX * 2);
    const qreal rectH = qMax<qreal>(24.0, hText + padY * 2);

    // 角丸札
    QPainterPath path;
    const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
    path.addRoundedRect(rect, RADIUS, RADIUS);

    auto* item = m_scene->addPath(path, QPen(Qt::black, 1.2));
    item->setBrush(fill);
    item->setZValue(10);
    item->setData(ROLE_ORIGINAL_BRUSH, item->brush().color().rgba());

    // メタ
    item->setData(ROLE_ROW, row);
    item->setData(ROLE_PLY, ply);
    item->setData(BR_ROLE_KIND, (row == 0) ? BNK_Main : BNK_Var);
    if (row == 0) item->setData(BR_ROLE_PLY, ply); // 既存クリック処理互換

    // 指し手ラベル（中央）
    auto* textItem = m_scene->addSimpleText(labelText, LABEL_FONT);
    const QRectF br = textItem->boundingRect();
    textItem->setParentItem(item);
    textItem->setPos(rect.center().x() - br.width() / 2.0,
                     rect.center().y() - br.height() / 2.0);

    // ★ 「n手目」ラベルは本譜の上だけに表示（分岐 row!=0 では表示しない）
    if (row == 0) {
        const QString moveNo = QString::number(ply) + QStringLiteral("手目");
        auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
        const QRectF nbr = noItem->boundingRect();
        noItem->setParentItem(item);
        const qreal gap = 4.0;
        noItem->setPos(rect.center().x() - nbr.width() / 2.0,
                       rect.top() - gap - nbr.height());
    }

    // クリック解決用（従来）
    m_nodeIndex.insert(qMakePair(row, ply), item);

    // ★ グラフ登録（vid はここでは row と同義で十分）
    const int nodeId = registerNode(/*vid*/row, row, ply, item);
    item->setData(ROLE_NODE_ID, nodeId);

    return item;
}

void EngineAnalysisTab::addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to)
{
    if (!from || !to) return;

    // ノードの中心（シーン座標）
    const QPointF a = from->sceneBoundingRect().center();
    const QPointF b = to->sceneBoundingRect().center();

    // 緩やかなベジェ
    QPainterPath path(a);
    const QPointF c1(a.x() + 8, a.y());
    const QPointF c2(b.x() - 8, b.y());
    path.cubicTo(c1, c2, b);

    auto* edge = m_scene->addPath(path, QPen(QColor(90, 90, 90), 1.0));
    edge->setZValue(0); // ← 線は常に背面（長方形の中に罫線が見えなくなる）

    // ★ グラフ接続
    const int prevId = from->data(ROLE_NODE_ID).toInt();
    const int nextId = to  ->data(ROLE_NODE_ID).toInt();
    if (prevId > 0 && nextId > 0) linkEdge(prevId, nextId);
}

// --- 追加ヘルパ：row(>=1) の分岐元となる「親行」を決める ---
int EngineAnalysisTab::resolveParentRowForVariation_(int row) const
{
    Q_ASSERT(row >= 1 && row < m_rows.size());
    const int b = m_rows.at(row).startPly;   // この変化の開始手数（1-origin）

    for (int prev = row - 1; prev >= 1; --prev) {
        const int a = m_rows.at(prev).startPly;
        if (b > a) return prev;              // 見つけたらその変化行を親に
    }
    return 0;                                 // どれにも入らなければ本譜
}

// ===================== シーン再構築 =====================

void EngineAnalysisTab::rebuildBranchTree()
{
    if (!m_scene) return;
    m_scene->clear();
    m_nodeIndex.clear();

    // ★ グラフもクリア
    clearBranchGraph();
    m_prevSelected = nullptr;

    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;   // addNode() と同じ
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(QStringLiteral("Noto Sans JP"), 10);

    QGraphicsPathItem* startNode = nullptr;

    // ===== 本譜 row=0 =====
    if (!m_rows.isEmpty()) {
        const auto& main = m_rows.at(0);

        // 「開始局面」（ply=0）
        {
            const qreal x = BASE_X + SHIFT_X;
            const qreal y = BASE_Y + 0 * STEP_Y;
            const QString label = QStringLiteral("開始局面");

            const QFontMetrics fm(LABEL_FONT);
            const int  wText = fm.horizontalAdvance(label);
            const int  hText = fm.height();
            const qreal padX = 14.0, padY = 8.0;
            const qreal rectW = qMax<qreal>(84.0, wText + padX * 2);
            const qreal rectH = qMax<qreal>(26.0, hText + padY * 2);

            QPainterPath path;
            const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
            path.addRoundedRect(rect, RADIUS, RADIUS);

            startNode = m_scene->addPath(path, QPen(Qt::black, 1.4));
            startNode->setBrush(QColor(235, 235, 235));
            startNode->setZValue(10);

            startNode->setData(ROLE_ROW, 0);
            startNode->setData(ROLE_PLY, 0);
            startNode->setData(BR_ROLE_KIND, BNK_Start);
            startNode->setData(ROLE_ORIGINAL_BRUSH, startNode->brush().color().rgba());
            m_nodeIndex.insert(qMakePair(0, 0), startNode);

            auto* t = m_scene->addSimpleText(label, LABEL_FONT);
            const QRectF br = t->boundingRect();
            t->setParentItem(startNode);
            t->setPos(rect.center().x() - br.width() / 2.0,
                      rect.center().y() - br.height() / 2.0);

            // ★ 開始局面もノード登録
            const int nid = registerNode(/*vid*/0, /*row*/0, /*ply*/0, startNode);
            startNode->setData(ROLE_NODE_ID, nid);
        }

        // 本譜のノード（ply=1..）
        QGraphicsPathItem* prev = startNode;
        int ply = 0;
        for (const auto& it : main.disp) {
            ++ply;
            auto* node = addNode(0, ply, it.prettyMove);
            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // ===== 分岐 row=1.. =====
    for (int row = 1; row < m_rows.size(); ++row) {
        const auto& rv = m_rows.at(row);
        const int startPly = qMax(1, rv.startPly);      // 1-origin

        // 1) 親行を決定
        const int parentRow = resolveParentRowForVariation_(row);

        // 2) 親と繋ぐ“合流手”は (startPly - 1) 手目のノード
        const int joinPly = startPly - 1;

        // 親の joinPly ノードを取得。無ければ本譜→開始局面へフォールバック。
        QGraphicsPathItem* prev =
            m_nodeIndex.value(qMakePair(parentRow, joinPly),
                              m_nodeIndex.value(qMakePair(0, joinPly),
                                                m_nodeIndex.value(qMakePair(0, 0), nullptr)));

        // 3) 分岐の手リストを「開始手以降だけ」にスライス
        const int cut = qMax(0, startPly - 1);              // 0-origin index
        const int total = rv.disp.size();
        const int take = (cut < total) ? (total - cut) : 0;
        if (take <= 0) continue;                            // 描くもの無し

        // 4) 切り出した手だけを絶対手数で並べる（absPly = startPly + i）
        for (int i = 0; i < take; ++i) {
            const auto& it = rv.disp.at(cut + i);
            const int absPly = startPly + i;

            auto* node = addNode(row, absPly, it.prettyMove);

            // クリック用メタ
            node->setData(BR_ROLE_STARTPLY, startPly);
            node->setData(BR_ROLE_BUCKET,    row - 1);

            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // シーン境界
    const int mainLen = m_rows.isEmpty() ? 0 : m_rows.at(0).disp.size();
    const qreal width  = (BASE_X + SHIFT_X) + STEP_X * qMax(40, mainLen + 6) + 40.0;
    const qreal height = 30 + STEP_Y * qMax(2, m_rows.size() + 1);
    m_scene->setSceneRect(QRectF(0, 0, width, height));
}

// ===================== ハイライト（フォールバック対応） =====================

void EngineAnalysisTab::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    // まず (row,ply) 直指定
    auto it = m_nodeIndex.find(qMakePair(row, ply));
    if (it != m_nodeIndex.end()) {
        highlightNodeId_(it.value()->data(ROLE_NODE_ID).toInt(), centerOn);
        return;
    }

    // 無ければグラフでフォールバック（分岐開始前は親行へ、あるいは next/prev を辿る）
    const int nid = graphFallbackToPly_(row, ply);
    if (nid > 0) {
        highlightNodeId_(nid, centerOn);
    }
}

void EngineAnalysisTab::highlightNodeId_(int nodeId, bool centerOn)
{
    if (nodeId <= 0) return;
    const auto node = m_nodesById.value(nodeId);
    QGraphicsPathItem* item = node.item;
    if (!item) return;

    // 直前ハイライト復元
    if (m_prevSelected) {
        const auto argb = m_prevSelected->data(ROLE_ORIGINAL_BRUSH).toUInt();
        m_prevSelected->setBrush(QColor::fromRgba(argb));
        m_prevSelected->setPen(QPen(Qt::black, 1.2));
        m_prevSelected->setZValue(10);
        m_prevSelected = nullptr;
    }

    // 黄色へ
    item->setBrush(QColor(255, 235, 80));
    item->setPen(QPen(Qt::black, 1.8));
    item->setZValue(20);
    m_prevSelected = item;

    if (centerOn && m_branchTree) m_branchTree->centerOn(item);
}

// ===================== クリック検出 =====================
bool EngineAnalysisTab::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_branchTree && obj == m_branchTree->viewport()
        && ev->type() == QEvent::MouseButtonRelease)
    {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (!(me->button() & Qt::LeftButton)) return QWidget::eventFilter(obj, ev);

        const QPointF scenePt = m_branchTree->mapToScene(me->pos());
        QGraphicsItem* hit =
            m_branchTree->scene() ? m_branchTree->scene()->itemAt(scenePt, QTransform()) : nullptr;

        // 子(Text)に当たることがあるので親まで遡る
        while (hit && !hit->data(BR_ROLE_KIND).isValid())
            hit = hit->parentItem();
        if (!hit) return false;

        // クリックされたノードの絶対(row, ply)
        const int row = hit->data(ROLE_ROW).toInt();  // 0=Main, 1..=VarN
        const int ply = hit->data(ROLE_PLY).toInt();  // 0=開始局面, 1..=手数

        // 即時の視覚フィードバック（黄色）※任意だが体感向上
        highlightBranchTreeAt(row, ply, /*centerOn=*/false);

        // 新API：MainWindow 側で (row, ply) をそのまま適用
        emit branchNodeActivated(row, ply);
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

// ===== 互換API 実装 =====
void EngineAnalysisTab::setSecondEngineVisible(bool on)
{
    if (m_info2)  m_info2->setVisible(on);
    if (m_view2)  m_view2->setVisible(on);
}

void EngineAnalysisTab::setEngine1ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model1 = m;
    if (m_view1) m_view1->setModel(m);
    if (m_view1 && m_view1->horizontalHeader())
        m_view1->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisTab::setEngine2ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model2 = m;
    if (m_view2) m_view2->setModel(m);
    if (m_view2 && m_view2->horizontalHeader())
        m_view2->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisTab::setCommentText(const QString& text)
{
    // 旧コード互換：プレーンテキストで設定（HTML解釈させたくない想定）
    if (m_comment) m_comment->setPlainText(text);
}

// ===================== グラフAPI 実装 =====================

void EngineAnalysisTab::clearBranchGraph()
{
    m_nodeIdByRowPly.clear();
    m_nodesById.clear();
    m_prevIds.clear();
    m_nextIds.clear();
    m_rowEntryNode.clear();
    m_nextNodeId = 1;
}

int EngineAnalysisTab::registerNode(int vid, int row, int ply, QGraphicsPathItem* item)
{
    if (!item) return -1;
    const int id = m_nextNodeId++;

    BranchGraphNode n;
    n.id   = id;
    n.vid  = vid;
    n.row  = row;
    n.ply  = ply;
    n.item = item;

    m_nodesById.insert(id, n);
    m_nodeIdByRowPly.insert(qMakePair(row, ply), id);

    // 行のエントリノード（最初に登録されたもの）を覚えておくと探索が楽
    if (!m_rowEntryNode.contains(row))
        m_rowEntryNode.insert(row, id);

    return id;
}

void EngineAnalysisTab::linkEdge(int prevId, int nextId)
{
    if (prevId <= 0 || nextId <= 0) return;
    m_nextIds[prevId].push_back(nextId);
    m_prevIds[nextId].push_back(prevId);
}

// ===================== フォールバック探索 =====================

int EngineAnalysisTab::graphFallbackToPly_(int row, int targetPly) const
{
    // 1) まず (row, ply) にノードがあるならそれ
    const int direct = nodeIdFor(row, targetPly);
    if (direct > 0) return direct;

    // 2) 分岐行の「開始手より前」なら親行へ委譲する
    if (row >= 1 && row < m_rows.size()) {
        const int startPly = qMax(1, m_rows.at(row).startPly);
        if (targetPly < startPly) {
            const int parentRow = resolveParentRowForVariation_(row);
            return graphFallbackToPly_(parentRow, targetPly);
        }
    }

    // 3) 同じ行内で近いノードから next を辿って同一 ply を探す
    //    まず「targetPly 以下で最も近い既存 ply」を見つける
    int seedId = -1;
    for (int p = targetPly; p >= 0; --p) {
        seedId = nodeIdFor(row, p);
        if (seedId > 0) break;
    }
    if (seedId <= 0) {
        // 行に何も無ければ、行の入口（例えば開始局面や最初の手）から辿る
        seedId = m_rowEntryNode.value(row, -1);
    }

    if (seedId > 0) {
        // BFSで next を辿り、targetPly と一致する ply を持つノードを探す
        QQueue<int> q;
        QSet<int> seen;
        q.enqueue(seedId);
        seen.insert(seedId);

        while (!q.isEmpty()) {
            const int cur = q.dequeue();
            const auto node = m_nodesById.value(cur);
            if (node.ply == targetPly) return cur;

            const auto nexts = m_nextIds.value(cur);
            for (int nx : nexts) {
                if (!seen.contains(nx)) {
                    seen.insert(nx);
                    q.enqueue(nx);
                }
            }
        }
    }

    // 4) それでも見つからない場合、親行へ委譲してみる（最終手段）
    if (row >= 1 && row < m_rows.size()) {
        const int parentRow = resolveParentRowForVariation_(row);
        if (parentRow != row) {
            const int viaParent = graphFallbackToPly_(parentRow, targetPly);
            if (viaParent > 0) return viaParent;
        }
    }

    // 5) 本譜 row=0 の seed からも探索してみる
    {
        int seed0 = nodeIdFor(0, targetPly);
        if (seed0 <= 0) seed0 = m_rowEntryNode.value(0, -1);
        if (seed0 > 0) {
            QQueue<int> q;
            QSet<int> seen;
            q.enqueue(seed0);
            seen.insert(seed0);
            while (!q.isEmpty()) {
                const int cur = q.dequeue();
                const auto node = m_nodesById.value(cur);
                if (node.ply == targetPly) return cur;
                const auto nexts = m_nextIds.value(cur);
                for (int nx : nexts) if (!seen.contains(nx)) { seen.insert(nx); q.enqueue(nx); }
            }
        }
    }

    return -1;
}
