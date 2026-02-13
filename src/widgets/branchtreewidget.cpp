/// @file branchtreewidget.cpp
/// @brief 分岐ツリーグラフウィジェットクラスの実装

#include "branchtreewidget.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QVBoxLayout>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRegularExpression>
#include "loggingcategory.h"

// ===================== ヘルパー関数 =====================

// クロスプラットフォーム対応の日本語フォントを取得
static QString getJapaneseFontFamily()
{
#ifdef Q_OS_WIN
    static const QStringList candidates = {
        QStringLiteral("Yu Gothic UI"),
        QStringLiteral("Meiryo UI"),
        QStringLiteral("Meiryo"),
        QStringLiteral("MS UI Gothic"),
        QStringLiteral("Noto Sans JP"),
        QStringLiteral("MS Gothic"),
    };
#elif defined(Q_OS_MAC)
    static const QStringList candidates = {
        QStringLiteral("Hiragino Sans"),
        QStringLiteral("Hiragino Kaku Gothic ProN"),
        QStringLiteral("Noto Sans JP"),
    };
#else
    static const QStringList candidates = {
        QStringLiteral("Noto Sans CJK JP"),
        QStringLiteral("Noto Sans JP"),
        QStringLiteral("IPAGothic"),
    };
#endif

    const QStringList availableFamilies = QFontDatabase::families();
    for (const QString &candidate : candidates) {
        if (availableFamilies.contains(candidate)) {
            return candidate;
        }
    }
    return QString();
}

// ===================== BranchTreeWidget =====================

BranchTreeWidget::BranchTreeWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

BranchTreeWidget::~BranchTreeWidget()
{
}

void BranchTreeWidget::buildUi()
{
    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setRenderHint(QPainter::TextAntialiasing, true);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->viewport()->installEventFilter(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    setLayout(layout);
}

void BranchTreeWidget::setTree(KifuBranchTree* tree)
{
    m_tree = tree;
    m_linesCache.clear();
    if (m_tree != nullptr) {
        m_linesCache = m_tree->allLines();
    }
    rebuild();
}

void BranchTreeWidget::highlightNode(int lineIndex, int ply)
{
    const int row = calculateRowForLine(lineIndex);
    QGraphicsPathItem* item = findNodeAt(row, ply);

    // フォールバック: 指定ノードが見つからない場合、同じplyの本譜ノードを探す
    if (item == nullptr && row > 0) {
        item = findNodeAt(0, ply);
    }

    // さらにフォールバック: 開始局面
    if (item == nullptr) {
        item = findNodeAt(0, 0);
    }

    if (item == nullptr) {
        return;
    }

    // 前回のハイライトを元に戻す
    if (m_prevHighlighted != nullptr && m_prevHighlighted != item) {
        const QRgb origColor = m_prevHighlighted->data(ROLE_ORIGINAL_BRUSH).toUInt();
        m_prevHighlighted->setBrush(QColor::fromRgba(origColor));
    }

    // 新しいノードをハイライト（黄色）
    item->setBrush(QColor(255, 255, 0));  // #FFFF00
    m_prevHighlighted = item;

    // センタリング
    m_view->centerOn(item);
}

void BranchTreeWidget::clearHighlight()
{
    if (m_prevHighlighted != nullptr) {
        const QRgb origColor = m_prevHighlighted->data(ROLE_ORIGINAL_BRUSH).toUInt();
        m_prevHighlighted->setBrush(QColor::fromRgba(origColor));
        m_prevHighlighted = nullptr;
    }
}

void BranchTreeWidget::rebuild()
{
    if (m_scene == nullptr) {
        return;
    }

    m_scene->clear();
    m_nodeIndex.clear();
    m_prevHighlighted = nullptr;

    // ラインキャッシュを更新
    if (m_tree != nullptr) {
        m_linesCache = m_tree->allLines();
    } else {
        m_linesCache.clear();
    }

    // 開始局面を描画
    drawStartNode();

    // 本譜を描画
    drawMainLine();

    // 分岐ラインを描画
    drawBranchLines();

    // 最大手数を計算
    int maxPly = 0;
    const auto keys = m_nodeIndex.keys();
    for (const auto& k : keys) {
        if (k.second > maxPly) {
            maxPly = k.second;
        }
    }

    // 手数ラベルを補完
    drawMoveNumberLabels(maxPly);

    // シーン境界を設定
    const int mainLen = m_linesCache.isEmpty() ? 0
        : static_cast<int>(m_linesCache.at(0).nodes.size());
    const int spanLen = qMax(mainLen, maxPly);
    const qreal width = (BASE_X + SHIFT_X) + STEP_X * qMax(40, spanLen + 6) + 40.0;
    const qreal height = 30 + STEP_Y * qMax(2.0, static_cast<qreal>(m_linesCache.size() + 1));
    m_scene->setSceneRect(QRectF(0, 0, width, height));

    // 初期状態で開始局面をハイライト
    highlightNode(0, 0);
}

void BranchTreeWidget::drawStartNode()
{
    static const QFont LABEL_FONT(getJapaneseFontFamily(), 10);

    const qreal x = BASE_X + SHIFT_X;
    const qreal y = BASE_Y;
    const QString label = tr("開始局面");

    const QFontMetrics fm(LABEL_FONT);
    const int wText = fm.horizontalAdvance(label);
    const int hText = fm.height();
    const qreal padX = 14.0, padY = 8.0;
    const qreal rectW = qMax<qreal>(84.0, wText + padX * 2);
    const qreal rectH = qMax<qreal>(26.0, hText + padY * 2);

    QPainterPath path;
    const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
    path.addRoundedRect(rect, RADIUS, RADIUS);

    auto* startNode = m_scene->addPath(path, QPen(Qt::black, 1.4));
    startNode->setBrush(QColor(235, 235, 235));
    startNode->setZValue(10);

    startNode->setData(ROLE_ROW, 0);
    startNode->setData(ROLE_PLY, 0);
    startNode->setData(ROLE_LINE_INDEX, 0);
    startNode->setData(ROLE_ORIGINAL_BRUSH, startNode->brush().color().rgba());
    m_nodeIndex.insert(qMakePair(0, 0), startNode);

    auto* t = m_scene->addSimpleText(label, LABEL_FONT);
    const QRectF br = t->boundingRect();
    t->setParentItem(startNode);
    t->setPos(rect.center().x() - br.width() / 2.0,
              rect.center().y() - br.height() / 2.0);
}

void BranchTreeWidget::drawMainLine()
{
    if (m_linesCache.isEmpty()) {
        return;
    }

    const BranchLine& mainLine = m_linesCache.at(0);
    QGraphicsPathItem* prev = findNodeAt(0, 0);  // 開始局面

    for (KifuBranchNode* node : std::as_const(mainLine.nodes)) {
        if (node->ply() == 0) {
            continue;  // 開始局面はスキップ
        }

        QGraphicsPathItem* item = addNode(0, node->ply(), node->displayText(), true);
        item->setData(ROLE_LINE_INDEX, 0);

        if (prev != nullptr) {
            addEdge(prev, item);
        }
        prev = item;
    }
}

void BranchTreeWidget::drawBranchLines()
{
    if (m_linesCache.size() <= 1) {
        return;
    }

    for (int lineIdx = 1; lineIdx < m_linesCache.size(); ++lineIdx) {
        const BranchLine& line = m_linesCache.at(lineIdx);
        const int row = lineIdx;  // 分岐ラインはそのまま行番号に

        // 分岐点を探す
        QGraphicsPathItem* prev = nullptr;
        if (line.branchPoint != nullptr) {
            const int branchPly = line.branchPoint->ply();
            // 親行を探す（分岐点のlineIndex）
            const int parentRow = calculateRowForLine(line.branchPoint->lineIndex());
            prev = findNodeAt(parentRow, branchPly);

            // フォールバック: 本譜のノードを探す
            if (prev == nullptr) {
                prev = findNodeAt(0, branchPly);
            }
        }

        // それでも見つからなければ開始局面
        if (prev == nullptr) {
            prev = findNodeAt(0, 0);
        }

        // 分岐開始手以降のノードを描画
        const int startPly = line.branchPly;
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->ply() < startPly) {
                continue;  // 分岐開始前のノードはスキップ
            }

            QGraphicsPathItem* item = addNode(row, node->ply(), node->displayText(), false);
            item->setData(ROLE_LINE_INDEX, lineIdx);

            if (prev != nullptr) {
                addEdge(prev, item);
            }
            prev = item;
        }
    }
}

void BranchTreeWidget::drawMoveNumberLabels(int maxPly)
{
    static const QFont LABEL_FONT(getJapaneseFontFamily(), 10);
    static const QFont MOVE_NO_FONT(getJapaneseFontFamily(), 9);

    if (maxPly <= 0) {
        return;
    }

    const QFontMetrics fmLabel(LABEL_FONT);
    const int hText = fmLabel.height();
    const qreal padY = 6.0;
    const qreal rectH = qMax<qreal>(24.0, hText + padY * 2);
    const qreal gap = 4.0;
    const qreal baselineY = BASE_Y;
    const qreal topY = (baselineY - rectH / 2.0) - gap;

    for (int ply = 1; ply <= maxPly; ++ply) {
        // 本譜ノードがあれば既にラベル付き
        if (m_nodeIndex.contains(qMakePair(0, ply))) {
            continue;
        }

        const QString moveNo = tr("%1手目").arg(ply);
        auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
        const QRectF nbr = noItem->boundingRect();

        const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
        noItem->setZValue(15);
        noItem->setPos(x - nbr.width() / 2.0, topY - nbr.height());
    }
}

QGraphicsPathItem* BranchTreeWidget::addNode(int row, int ply, const QString& rawText, bool isMainLine)
{
    static const QFont LABEL_FONT(getJapaneseFontFamily(), 10);
    static const QFont MOVE_NO_FONT(getJapaneseFontFamily(), 9);

    const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
    const qreal y = BASE_Y + row * STEP_Y;

    // 先頭の手数数字を除去
    static const QRegularExpression kDropHeadNumber(QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
    QString labelText = rawText;
    labelText.replace(kDropHeadNumber, QString());
    labelText = labelText.trimmed();

    // 分岐マーク '+' を除去
    if (labelText.endsWith(QLatin1Char('+'))) {
        labelText.chop(1);
        labelText = labelText.trimmed();
    }

    const bool odd = (ply % 2) == 1;
    const QColor mainOdd(196, 230, 255);   // 先手=水色
    const QColor mainEven(255, 223, 196);  // 後手=ピーチ
    const QColor fill = odd ? mainOdd : mainEven;

    // 札サイズ
    const QFontMetrics fm(LABEL_FONT);
    const int wText = fm.horizontalAdvance(labelText);
    const int hText = fm.height();
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

    // 指し手ラベル（中央）
    auto* textItem = m_scene->addSimpleText(labelText, LABEL_FONT);
    const QRectF br = textItem->boundingRect();
    textItem->setParentItem(item);
    textItem->setPos(rect.center().x() - br.width() / 2.0,
                     rect.center().y() - br.height() / 2.0);

    // 本譜の上には「n手目」ラベルを表示
    if (isMainLine) {
        const QString moveNo = tr("%1手目").arg(ply);
        auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
        const QRectF nbr = noItem->boundingRect();
        noItem->setParentItem(item);
        const qreal gap = 4.0;
        noItem->setPos(rect.center().x() - nbr.width() / 2.0,
                       rect.top() - nbr.height() - gap);
    }

    m_nodeIndex.insert(qMakePair(row, ply), item);
    return item;
}

void BranchTreeWidget::addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to)
{
    if (from == nullptr || to == nullptr) {
        return;
    }

    const QPointF a = from->sceneBoundingRect().center();
    const QPointF b = to->sceneBoundingRect().center();

    QPainterPath path(a);
    const QPointF c1(a.x() + 8, a.y());
    const QPointF c2(b.x() - 8, b.y());
    path.cubicTo(c1, c2, b);

    auto* edge = m_scene->addPath(path, QPen(QColor(90, 90, 90), 1.0));
    edge->setZValue(0);
}

QGraphicsPathItem* BranchTreeWidget::findNodeAt(int row, int ply) const
{
    return m_nodeIndex.value(qMakePair(row, ply), nullptr);
}

int BranchTreeWidget::calculateRowForLine(int lineIndex) const
{
    // lineIndex=0 は本譜で row=0
    // lineIndex=1以降は分岐でそのまま行番号に
    return lineIndex;
}

bool BranchTreeWidget::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_view->viewport() && ev->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::LeftButton && m_clickEnabled) {
            const QPointF scenePos = m_view->mapToScene(me->pos());
            QGraphicsItem* clicked = m_scene->itemAt(scenePos, QTransform());

            // クリックされたアイテムがパスアイテムでなければ親を探す
            while (clicked != nullptr && clicked->type() != QGraphicsPathItem::Type) {
                clicked = clicked->parentItem();
            }

            if (clicked != nullptr) {
                auto* pathItem = static_cast<QGraphicsPathItem*>(clicked);
                const int row = pathItem->data(ROLE_ROW).toInt();
                const int ply = pathItem->data(ROLE_PLY).toInt();
                const int lineIndex = pathItem->data(ROLE_LINE_INDEX).toInt();

                qCDebug(lcUi) << "[BranchTreeWidget] nodeClicked: lineIndex=" << lineIndex
                         << " ply=" << ply << " row=" << row;

                emit nodeClicked(lineIndex, ply);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}
