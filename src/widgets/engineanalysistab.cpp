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

#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

EngineAnalysisTab::EngineAnalysisTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void EngineAnalysisTab::buildUi()
{
    m_tab = new QTabWidget(this);

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
    m_branchTree->viewport()->installEventFilter(this);

    m_tab->addTab(m_branchTree, tr("分岐ツリー"));

    // 外側
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->addWidget(m_tab);

    // 分岐ツリータブ（m_branchTree）を生成済みならクリック検知を仕込む
    if (m_branchTree && m_branchTree->viewport())
        m_branchTree->viewport()->installEventFilter(this);
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

void EngineAnalysisTab::setBranchTreeRows(const QVector<ResolvedRowLite>& rows)
{
    m_rows = rows;
    rebuildBranchTree();
}

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

    m_nodeIndex.insert(qMakePair(row, ply), item);
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
}

void EngineAnalysisTab::rebuildBranchTree()
{
    if (!m_scene) return;
    m_scene->clear();
    m_nodeIndex.clear();

    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;   // ← addNode と同じ値
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(QStringLiteral("Noto Sans JP"), 10);

    QGraphicsPathItem* startNode = nullptr;

    // ===== 本譜 row=0 =====
    if (!m_rows.isEmpty()) {
        const auto& main = m_rows.at(0);

        // 「開始局面」（ply=0）— 全体シフトを適用
        {
            const qreal x = BASE_X + SHIFT_X;     // ← ここも右寄せ
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
        }

        // 本譜の手札（addNode 内で SHIFT_X が効く）
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
        const int basePly = qMax(0, rv.startPly - 1);
        QGraphicsPathItem* prev = m_nodeIndex.value(qMakePair(0, basePly), nullptr);

        int local = 0;
        for (const auto& it : rv.disp) {
            ++local;
            const int ply = rv.startPly - 1 + local;

            auto* node = addNode(row, ply, it.prettyMove);
            node->setData(BR_ROLE_STARTPLY, rv.startPly);
            node->setData(BR_ROLE_BUCKET,    row - 1);

            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // シーン境界：シフトぶん余白を足す
    const int mainLen = m_rows.isEmpty() ? 0 : m_rows.at(0).disp.size();
    const qreal width  = (BASE_X + SHIFT_X) + STEP_X * qMax(40, mainLen + 3) + 40.0;
    const qreal height = 30 + STEP_Y * qMax(2, m_rows.size() + 1);
    m_scene->setSceneRect(QRectF(0, 0, width, height));
}

void EngineAnalysisTab::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    auto it = m_nodeIndex.find(qMakePair(row, ply));
    if (it == m_nodeIndex.end()) return;

    static QGraphicsPathItem* s_prevSelected = nullptr;
    if (s_prevSelected) {
        const auto argb = s_prevSelected->data(ROLE_ORIGINAL_BRUSH).toUInt();
        s_prevSelected->setBrush(QColor::fromRgba(argb));
        s_prevSelected->setPen(QPen(Qt::black, 1.2));
        s_prevSelected->setZValue(10);
        s_prevSelected = nullptr;
    }

    QGraphicsPathItem* node = it.value();
    node->setBrush(QColor(255, 235, 80));   // ← はっきりした黄色
    node->setPen(QPen(Qt::black, 1.8));
    node->setZValue(20);
    s_prevSelected = node;

    if (centerOn && m_branchTree) m_branchTree->centerOn(node);
}

bool EngineAnalysisTab::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_branchTree && obj == m_branchTree->viewport()
        && (ev->type() == QEvent::MouseButtonRelease /* または MouseButtonPress */))
    {
        auto* me = static_cast<QMouseEvent*>(ev);
        const QPointF scenePt = m_branchTree->mapToScene(me->pos());

        QGraphicsItem* hit =
            m_branchTree->scene() ? m_branchTree->scene()->itemAt(scenePt, QTransform()) : nullptr;

        // 子(Text)に当たることがあるので親まで遡る
        while (hit && !hit->data(BR_ROLE_KIND).isValid())
            hit = hit->parentItem();
        if (!hit) return false;

        const int kind = hit->data(BR_ROLE_KIND).toInt();
        switch (kind) {
        case BNK_Start:
            emit requestApplyStart();
            return true;

        case BNK_Main: {
            const int ply = hit->data(BR_ROLE_PLY).toInt();
            emit requestApplyMainAtPly(ply);
            return true;
        }

        case BNK_Var: {
            const int sp  = hit->data(BR_ROLE_STARTPLY).toInt();
            const int bix = hit->data(BR_ROLE_BUCKET).toInt();
            emit requestApplyVariation(sp, bix);
            return true;
        }

        default:
            break;
        }
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
