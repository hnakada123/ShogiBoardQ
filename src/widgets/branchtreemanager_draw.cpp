/// @file branchtreemanager_draw.cpp
/// @brief BranchTreeManager のシーン構築・ノード/エッジ描画

#include "branchtreemanager.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifupresentation.h"
#include "applicationfonts.h"
#include "logcategories.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QApplication>
#include <QPalette>
#include <QRegularExpression>

#include <memory>

// ===================== ヘルパー関数 =====================

namespace {
// レイアウト定数（ノード配置・シーン範囲・ラベル位置で共有）
constexpr qreal kBaseX  = 40.0;
constexpr qreal kShiftX = 40.0;
constexpr qreal kStepY  = 56.0;
constexpr qreal kRadius = 8.0;
constexpr qreal kHeaderHeight = 22.0;   ///< 手数見出しの帯の高さ
constexpr int kTooltipCommentChars = 400;

/// 分岐ツリーのノードに表示する手の表記（KifuPresentation の表記設定に従う）
struct NodeText {
    QString canonical;   ///< 手番記号・手数を除いた棋譜の表記
    QString beforeSfen;
    QString usiMove;
};

NodeText nodeText(const KifuBranchNode* node)
{
    static const QRegularExpression kDropHeadNumber(QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
    NodeText text;
    text.canonical = node->displayText();
    text.canonical.replace(kDropHeadNumber, QString());
    text.canonical = text.canonical.trimmed();
    if (text.canonical.endsWith(QLatin1Char('+'))) {
        text.canonical.chop(1);
        text.canonical = text.canonical.trimmed();
    }
    text.beforeSfen = node->parent() ? node->parent()->sfen() : QString();
    text.usiMove = node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move());
    return text;
}

/// 手の表記に使う書体（日本語表記の指し手は日本語の書体）
QFont labelFont(const NodeText& text)
{
    QFont font(QApplication::font().family(), 10);
    if (!text.usiMove.isEmpty() && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese)
        font = ApplicationFonts::japaneseFont(font);
    return font;
}

/// 列の間隔に必要な幅（表記の幅＋余白）
qreal requiredColumnSpacing(const KifuBranchNode* node)
{
    const NodeText text = nodeText(node);
    const QFontMetrics metrics(labelFont(text));
    return metrics.horizontalAdvance(KifuPresentation::label(text.canonical, text.beforeSfen, text.usiMove)) + 48;
}
} // namespace

// ===================== ノード/エッジ描画 =====================

QPen BranchTreeManager::edgePen()
{
    return QPen(QColor(90, 90, 90), 1.0);
}

QPen BranchTreeManager::pathEdgePen()
{
    return QPen(QColor(40, 110, 210), 2.6);
}

QGraphicsPathItem* BranchTreeManager::addNode(int row, int ply, const KifuBranchNode* node)
{
    const NodeText text = nodeText(node);
    const QFont LABEL_FONT = labelFont(text);
    const QFont BADGE_FONT(QApplication::font().family(), 9);

    const qreal x = kBaseX + kShiftX + ply * m_columnSpacing;
    const qreal y = laneY(laneForRow(row));

    const QString labelText = KifuPresentation::label(text.canonical, text.beforeSfen, text.usiMove);
    const bool odd = text.beforeSfen.isEmpty() ? (ply % 2) == 1
        : text.beforeSfen.section(QLatin1Char(' '), 1, 1) == QStringLiteral("b");

    const QColor mainOdd (196, 230, 255);
    const QColor mainEven(255, 223, 196);
    const QColor fill = odd ? mainOdd : mainEven;

    const QFontMetrics fm(LABEL_FONT);
    const int  wText = fm.horizontalAdvance(labelText);
    const int  hText = fm.height();
    const qreal padX = 12.0, padY = 6.0;
    const qreal rectW = qMax<qreal>(70.0, wText + padX * 2);
    const qreal rectH = qMax<qreal>(24.0, hText + padY * 2);

    QPainterPath path;
    const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
    path.addRoundedRect(rect, kRadius, kRadius);

    auto* item = m_scene->addPath(path, QPen(Qt::black, 1.2));
    item->setBrush(fill);
    item->setZValue(10);
    item->setData(ROLE_ORIGINAL_BRUSH, item->brush().color().rgba());

    // ツールチップ：手の表記（読み上げ形式）と、あればコメント
    QString tooltip = KifuPresentation::label(text.canonical, text.beforeSfen, text.usiMove, true).toHtmlEscaped();
    const QString comment = node->comment().trimmed();
    if (!comment.isEmpty()) {
        QString shown = comment.left(kTooltipCommentChars);
        if (comment.size() > kTooltipCommentChars) shown += QStringLiteral("…");
        tooltip += QStringLiteral("<hr/><p style='white-space:pre-wrap'>%1</p>").arg(shown.toHtmlEscaped());
    }
    item->setToolTip(QStringLiteral("<qt>%1</qt>").arg(tooltip));

    item->setData(ROLE_ROW, row);
    item->setData(ROLE_PLY, ply);
    item->setData(BR_ROLE_KIND, (row == 0) ? BNK_Main : BNK_Var);
    if (row == 0) item->setData(BR_ROLE_PLY, ply);

    auto* textItem = m_scene->addSimpleText(labelText, LABEL_FONT);
    const QRectF br = textItem->boundingRect();
    textItem->setParentItem(item);
    textItem->setPos(rect.center().x() - br.width() / 2.0,
                     rect.center().y() - br.height() / 2.0);

    // コメントのある手は右上に印を付ける
    if (!comment.isEmpty()) {
        constexpr qreal d = 8.0;
        auto* marker = new QGraphicsEllipseItem(rect.right() - d - 3.0, rect.top() + 3.0, d, d, item);
        marker->setBrush(QColor(60, 130, 220));
        marker->setPen(QPen(Qt::white, 1.0));
    }

    // 折りたたんだ変化の先頭には、隠れている手の数を表示する
    if (row >= 1 && isRowCollapsed(row) && ply == qMax(1, m_rows.at(row).startPly)) {
        QPen dashed(Qt::black, 1.2);
        dashed.setStyle(Qt::DashLine);
        item->setPen(dashed);
        auto* badge = m_scene->addSimpleText(QStringLiteral("▸ +%1").arg(hiddenNodeCount(row)), BADGE_FONT);
        badge->setParentItem(item);
        badge->setBrush(m_branchTree ? m_branchTree->palette().color(QPalette::Text) : QColor(Qt::black));
        const QRectF bbr = badge->boundingRect();
        badge->setPos(rect.right() + 6.0, rect.center().y() - bbr.height() / 2.0);
        item->setToolTip(item->toolTip() + tr("<p>（ダブルクリックで展開）</p>"));
    }

    m_nodeIndex.insert(qMakePair(row, ply), item);
    m_itemByNodeId.insert(node->nodeId(), item);
    item->setData(ROLE_NODE_ID, node->nodeId());

    return item;
}

void BranchTreeManager::addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to)
{
    if (!from || !to) return;

    const QRectF fromRect = from->sceneBoundingRect();
    const QRectF toRect = to->sceneBoundingRect();
    const qreal ay = fromRect.center().y();
    const qreal by = toRect.center().y();

    QPainterPath path;
    if (qFuzzyCompare(ay, by)) {
        // 同じ段：中心どうしを結ぶ
        const QPointF a = fromRect.center();
        const QPointF b = toRect.center();
        path.moveTo(a);
        path.cubicTo(QPointF(a.x() + 8, a.y()), QPointF(b.x() - 8, b.y()), b);
    } else {
        // 別の段：列の間のすき間で縦に下ろし、間の段のノードに線が重ならないようにする。
        // 縦線は列の中心どうしの中間に置き、同じ手から出る変化が1本の縦線を共有するようにする
        // （ノードの幅は表記で変わるため、ノードの端を基準にすると縦線が並んでしまう）
        const qreal xm = (fromRect.center().x() + toRect.center().x()) / 2.0;
        const qreal r = qMin<qreal>(8.0, qMax<qreal>(1.0, (toRect.left() - fromRect.right()) / 4.0));
        const qreal dir = (by > ay) ? 1.0 : -1.0;
        path.moveTo(fromRect.right(), ay);
        path.lineTo(xm - r, ay);
        path.quadTo(QPointF(xm, ay), QPointF(xm, ay + dir * r));
        path.lineTo(xm, by - dir * r);
        path.quadTo(QPointF(xm, by), QPointF(xm + r, by));
        path.lineTo(toRect.left(), by);
    }

    auto* edge = m_scene->addPath(path, edgePen());
    edge->setZValue(0);

    m_edgeInto.insert(to->data(ROLE_NODE_ID).toInt(), edge);
}

// ===================== シーン再構築 =====================

void BranchTreeManager::rebuildBranchTree()
{
    if (!m_scene) return;
    ++m_rebuildCount;
    m_scene->clear();
    m_nodeIndex.clear();
    m_itemByNodeId.clear();
    m_edgeInto.clear();
    m_highlightedEdges.clear();
    m_plyLabels.clear();   // scene->clear() で削除済み
    m_headerBand = nullptr;
    m_prevSelected = nullptr;
    m_lastHighlightedRow = -1;
    m_lastHighlightedPly = -1;

    // 折りたたみの操作などで作り直すときも、その時点のツリーを描く
    syncRowsFromTree();
    rememberTreeState();
    computeLayout();
    const QList<BranchLine> lines = m_tree ? m_tree->allLines() : QList<BranchLine>();

    const QFont LABEL_FONT(QApplication::font().family(), 10);
    m_columnSpacing = 110.0;
    for (qsizetype row = 0; row < lines.size(); ++row) {
        const QList<KifuBranchNode*>& nodes = lines.at(row).nodes;
        for (qsizetype ply = qMax(1, m_rows.at(row).startPly); ply < nodes.size(); ++ply)
            m_columnSpacing = qMax(m_columnSpacing, requiredColumnSpacing(nodes.at(ply)));
    }

    // ===== 手数見出しの帯（上端に固定して表示する） =====
    {
        // 下に隠れたノードが透けないよう不透明にする
        const QColor band = m_branchTree ? m_branchTree->palette().color(QPalette::Base) : QColor(Qt::white);
        m_headerBand = m_scene->addRect(QRectF(0, 0, 1, kHeaderHeight), Qt::NoPen, band);
        m_headerBand->setZValue(40);
    }

    // ===== 「開始局面」ノード =====
    {
        const KifuBranchNode* root = m_tree ? m_tree->root() : nullptr;
        const qreal x = kBaseX + kShiftX;
        const qreal y = laneY(0);
        const QString label = tr("開始局面");

        const QFontMetrics fm(LABEL_FONT);
        const qreal padX = 14.0, padY = 8.0;
        const qreal rectW = qMax<qreal>(84.0, fm.horizontalAdvance(label) + padX * 2);
        const qreal rectH = qMax<qreal>(26.0, fm.height() + padY * 2);

        QPainterPath path;
        const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
        path.addRoundedRect(rect, kRadius, kRadius);

        auto* startNode = m_scene->addPath(path, QPen(Qt::black, 1.4));
        startNode->setBrush(QColor(235, 235, 235));
        startNode->setZValue(10);
        startNode->setData(ROLE_ROW, 0);
        startNode->setData(ROLE_PLY, 0);
        startNode->setData(BR_ROLE_KIND, BNK_Start);
        startNode->setData(ROLE_ORIGINAL_BRUSH, startNode->brush().color().rgba());
        startNode->setData(ROLE_NODE_ID, root ? root->nodeId() : 0);
        m_nodeIndex.insert(qMakePair(0, 0), startNode);
        if (root) m_itemByNodeId.insert(root->nodeId(), startNode);

        auto* t = m_scene->addSimpleText(label, LABEL_FONT);
        const QRectF br = t->boundingRect();
        t->setParentItem(startNode);
        t->setPos(rect.center().x() - br.width() / 2.0, rect.center().y() - br.height() / 2.0);

        // 開始局面のコメント
        const QString comment = root ? root->comment().trimmed() : QString();
        if (!comment.isEmpty()) {
            constexpr qreal d = 8.0;
            auto* marker = new QGraphicsEllipseItem(rect.right() - d - 3.0, rect.top() + 3.0, d, d, startNode);
            marker->setBrush(QColor(60, 130, 220));
            marker->setPen(QPen(Qt::white, 1.0));
            startNode->setToolTip(QStringLiteral("<qt><p style='white-space:pre-wrap'>%1</p></qt>")
                                      .arg(comment.left(kTooltipCommentChars).toHtmlEscaped()));
        }
    }

    // ===== 各行（0=本譜、1..=変化）：変化の先頭から描き、分岐元の手の図形とつなぐ =====
    for (qsizetype row = 0; row < lines.size(); ++row) {
        const int r = static_cast<int>(row);
        if (laneForRow(r) < 0) continue;   // 折りたたみで隠れている
        const QList<KifuBranchNode*>& nodes = lines.at(row).nodes;
        const int startPly = qMax(1, m_rows.at(row).startPly);
        const int lastPly = lastVisiblePly(r);
        for (int ply = startPly; ply <= lastPly && ply < nodes.size(); ++ply) {
            const KifuBranchNode* node = nodes.at(ply);
            // 直前の手（分岐の先頭なら分岐元の手）。終局の手からは線を出さず、その前の手につなぐ
            const KifuBranchNode* prevNode = node->parent();
            while (prevNode && prevNode->isTerminal() && prevNode->parent()) prevNode = prevNode->parent();
            QGraphicsPathItem* prev = itemForNode(prevNode);
            if (!prev) prev = m_nodeIndex.value(qMakePair(0, 0), nullptr);

            QGraphicsPathItem* item = addNode(r, ply, node);
            if (row > 0) {
                item->setData(BR_ROLE_STARTPLY, startPly);
                item->setData(BR_ROLE_BUCKET, r - 1);
            }
            addEdge(prev, item);
        }
    }
    qCDebug(lcUi).noquote() << "[BTM] rebuildBranchTree: rows=" << m_rows.size()
                            << "lanes=" << m_laneCount << "compact=" << m_compactLayout;

    // ===== 手数の見出し =====
    const int maxAbsPly = maxDrawnPly();
    for (int ply = 1; ply <= maxAbsPly; ++ply) {
        addMoveNumberLabel(ply);
    }

    // ===== シーン境界 =====
    updateSceneRect();

    highlightBranchTreeAt(0, 0, /*centerOn=*/false);
}

// ===================== 差分更新（ライブ対局用） =====================

int BranchTreeManager::rowDispCount(int row) const
{
    if (row < 0 || row >= m_rows.size()) return 0;
    return m_rows.at(row).length;
}

bool BranchTreeManager::appendNode(const KifuBranchNode* node)
{
    if (!m_scene || !m_tree || !node || !node->parent() || node->childCount() != 0) return false;
    const int ply = node->ply();
    // 直前の手が、ある行の末尾に描いてあること。分岐行の先頭ノード（新規ライン）は
    // 親行との接続や行の並び替えが必要なため、全再構築に任せる
    QGraphicsPathItem* prev = itemForNode(node->parent());
    if (!prev || prev->data(ROLE_PLY).toInt() != ply - 1) return false;
    const int row = prev->data(ROLE_ROW).toInt();
    if (row < 0 || row >= m_rows.size()) return false;
    RowInfo& rv = m_rows[row];
    if (rv.length != ply || nodeAt(row, ply) != node) return false;

    // 折りたたみで隠れている行は全再構築に任せる（現在の手を見せるため展開される）
    if (laneForRow(row) < 0 || isRowCollapsed(row)) return false;

    // 詰めた配置では、伸びた先に同じ段の別の変化があれば配置し直す
    if (m_compactLayout) {
        const int lane = laneForRow(row);
        for (qsizetype other = 0; other < m_rows.size(); ++other) {
            if (other == row || laneForRow(static_cast<int>(other)) != lane) continue;
            const int otherStart = qMax(1, m_rows.at(other).startPly);
            if (otherStart > rv.startPly && otherStart <= ply + 1) return false;
        }
    }

    // 表記が列の幅に収まらなければ、列の幅を広げて全体を作り直す
    if (requiredColumnSpacing(node) > m_columnSpacing) {
        rebuildBranchTree();
        return true;
    }
    QGraphicsPathItem* item = addNode(row, ply, node);
    if (row != 0) {
        item->setData(BR_ROLE_STARTPLY, qMax(1, rv.startPly));
        item->setData(BR_ROLE_BUCKET, row - 1);
    }
    addMoveNumberLabel(ply);
    addEdge(prev, item);
    ++rv.length;
    rememberTreeState();

    updateSceneRect();
    return true;
}

int BranchTreeManager::maxDrawnPly() const
{
    int maxAbsPly = 0;
    for (auto it = m_nodeIndex.cbegin(); it != m_nodeIndex.cend(); ++it) {
        maxAbsPly = qMax(maxAbsPly, it.key().second);
    }
    return maxAbsPly;
}

void BranchTreeManager::addMoveNumberLabel(int ply)
{
    const QFont MOVE_NO_FONT(QApplication::font().family(), 9);

    if (!m_scene || !m_headerBand || ply < 1) return;
    if (m_plyLabels.contains(ply)) return;

    const QString moveNo = tr("%1手目").arg(ply);
    auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
    noItem->setParentItem(m_headerBand);
    noItem->setBrush(m_branchTree ? m_branchTree->palette().color(QPalette::Text) : QColor(Qt::black));
    const QRectF nbr = noItem->boundingRect();
    const qreal x = kBaseX + kShiftX + ply * m_columnSpacing;
    noItem->setPos(x - nbr.width() / 2.0, kHeaderHeight - nbr.height() - 1.0);
    m_plyLabels.insert(ply, noItem);
}

void BranchTreeManager::removeMoveNumberLabel(int ply)
{
    QGraphicsSimpleTextItem* item = m_plyLabels.take(ply);
    if (item && m_scene) {
        // シーンから外した時点で所有権はこちらに移るので unique_ptr で解放する
        m_scene->removeItem(item);
        const std::unique_ptr<QGraphicsSimpleTextItem> owner(item);
    }
}

void BranchTreeManager::updateSceneRect()
{
    if (!m_scene) return;
    const int mainLen = m_rows.isEmpty() ? 0 : qMax(0, m_rows.at(0).length - 1);
    const int spanLen = qMax(mainLen, maxDrawnPly());
    const qreal width  = (kBaseX + kShiftX) + m_columnSpacing * qMax(40, spanLen + 6) + 40.0;
    const qreal height = 30 + kStepY * static_cast<qreal>(qMax(2, m_laneCount + 1));
    m_scene->setSceneRect(QRectF(0, 0, width, height));
    if (m_headerBand) m_headerBand->setRect(QRectF(0, 0, width, kHeaderHeight));
    updateStickyHeader();
}

void BranchTreeManager::updateStickyHeader()
{
    if (!m_branchTree || !m_headerBand) return;
    // 見えている範囲の上端に帯を置く（スクロールしても手数が読める）
    const qreal visibleTop = m_branchTree->mapToScene(QPoint(0, 0)).y();
    m_headerBand->setY(qMax<qreal>(0.0, visibleTop));
}
