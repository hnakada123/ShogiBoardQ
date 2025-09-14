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

QGraphicsPathItem* EngineAnalysisTab::addNode(int row, int ply, const QString& text)
{
    // 配置：x=手数方向, y=行方向
    const qreal r = 8.0;
    const qreal x = 40.0 + ply * 20.0;
    const qreal y = 30.0 + row * 50.0;

    QPainterPath path;
    path.addEllipse(QPointF(x, y), r, r);
    auto* item = m_scene->addPath(path, QPen(Qt::black, 1.0));
    item->setBrush(Qt::white);

    // ラベル（小さめの手数表示）
    auto* label = m_scene->addSimpleText(text, QFont(QStringLiteral("Noto Sans"), 8));
    QRectF br = label->boundingRect();
    label->setPos(x - br.width()/2.0, y - r - br.height() - 2.0);

    // クリック用メタ
    item->setData(ROLE_ROW, row);
    item->setData(ROLE_PLY, ply);

    m_nodeIndex.insert(qMakePair(row, ply), item);
    return item;
}

void EngineAnalysisTab::addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to)
{
    if (!from || !to) return;
    QPainterPath path;
    QPointF a = from->path().boundingRect().center();
    QPointF b = to->path().boundingRect().center();
    path.moveTo(a);
    // 緩やかなベジェ
    QPointF c1(a.x() + 8, a.y());
    QPointF c2(b.x() - 8, b.y());
    path.cubicTo(c1, c2, b);
    m_scene->addPath(path, QPen(QColor(80,160,80), 1.2));
}

void EngineAnalysisTab::rebuildBranchTree()
{
    if (!m_scene) return;
    m_scene->clear();
    m_nodeIndex.clear();

    // 本譜（row=0）
    if (!m_rows.isEmpty()) {
        const auto& main = m_rows.at(0);
        QGraphicsPathItem* prev = nullptr;
        int ply = 0;
        for (const auto& it : main.disp) {
            Q_UNUSED(it);
            ++ply;
            auto* node = addNode(0, ply, QString::number(ply));
            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // 分岐（row=1..）
    for (int row = 1; row < m_rows.size(); ++row) {
        const auto& rv = m_rows.at(row);

        // 接続元：本譜 startPly のノード
        const int basePly = qMax(1, qMin(rv.startPly, m_rows.value(0).disp.size()));
        QGraphicsPathItem* prev = m_nodeIndex.value(qMakePair(0, basePly), nullptr);

        int local = 0;
        for (const auto& it : rv.disp) {
            Q_UNUSED(it);
            ++local;
            const int ply = rv.startPly - 1 + local;
            auto* node = addNode(row, ply, QString::number(ply));
            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // シーン境界
    const int mainLen = m_rows.isEmpty() ? 0 : m_rows.at(0).disp.size();
    m_scene->setSceneRect(QRectF(0, 0, 40 + 20 * qMax(40, mainLen + 10),
                                 30 + 50 * qMax(2, m_rows.size() + 1)));
}

void EngineAnalysisTab::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    auto it = m_nodeIndex.find(qMakePair(row, ply));
    if (it == m_nodeIndex.end()) return;

    // 一旦リセット
    for (auto* n : m_nodeIndex) if (n) n->setBrush(Qt::white);

    // ハイライト
    QGraphicsPathItem* node = it.value();
    node->setBrush(QColor(255, 240, 160));
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
