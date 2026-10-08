/// @file branchtreemanager_draw.cpp
/// @brief BranchTreeManager のシーン構築・ノード/エッジ描画

#include "branchtreemanager.h"
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
#include <QFontInfo>
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

const QPen kEdgePen(QColor(90, 90, 90), 1.0);

bool isTerminalText(const QString& text)
{
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    for (const auto& kw : kTerminalKeywords) {
        if (text.contains(kw)) return true;
    }
    return false;
}
} // namespace

static void debugFontInfo(const QFont &font, const QString &context)
{
    QFontInfo info(font);
    qCDebug(lcUi) << "[FontDebug]" << context;
    qCDebug(lcUi) << "  Requested family:" << font.family();
    qCDebug(lcUi) << "  Actual family:" << info.family();
    qCDebug(lcUi) << "  Point size:" << info.pointSize();
    qCDebug(lcUi) << "  Pixel size:" << info.pixelSize();
    qCDebug(lcUi) << "  Style hint:" << font.styleHint();
    qCDebug(lcUi) << "  Exact match:" << info.exactMatch();
}

// ===================== ノード/エッジ描画 =====================

QGraphicsPathItem* BranchTreeManager::addNode(int row, int ply, const KifDisplayItem& entry)
{
    QFont LABEL_FONT(QApplication::font().family(), 10);
    if (!entry.usiMove.isEmpty() && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese)
        LABEL_FONT = ApplicationFonts::japaneseFont(LABEL_FONT);
    const QFont BADGE_FONT(QApplication::font().family(), 9);

    static bool fontDebugDone = false;
    if (!fontDebugDone) {
        debugFontInfo(LABEL_FONT, "addNode LABEL_FONT");
        fontDebugDone = true;
    }

    const qreal x = kBaseX + kShiftX + ply * m_columnSpacing;
    const qreal y = laneY(laneForRow(row));

    static const QRegularExpression kDropHeadNumber(QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
    QString labelText = entry.prettyMove;
    labelText.replace(kDropHeadNumber, QString());
    labelText = labelText.trimmed();

    if (labelText.endsWith(QLatin1Char('+'))) {
        labelText.chop(1);
        labelText = labelText.trimmed();
    }

    const QString canonical = labelText;
    labelText = KifuPresentation::label(canonical, entry.beforeSfen, entry.usiMove);
    const bool odd = entry.beforeSfen.isEmpty() ? (ply % 2) == 1
        : entry.beforeSfen.section(QLatin1Char(' '), 1, 1) == QStringLiteral("b");

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
    QString tooltip = KifuPresentation::label(canonical, entry.beforeSfen, entry.usiMove, true).toHtmlEscaped();
    const QString comment = entry.comment.trimmed();
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

    const int nodeId = registerNode(row, ply, item);
    item->setData(ROLE_NODE_ID, nodeId);

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
        // 別の段：列の間のすき間で縦に下ろし、間の段のノードに線が重ならないようにする
        const qreal xm = (fromRect.right() + toRect.left()) / 2.0;
        const qreal r = qMin<qreal>(8.0, qMax<qreal>(1.0, (toRect.left() - fromRect.right()) / 4.0));
        const qreal dir = (by > ay) ? 1.0 : -1.0;
        path.moveTo(fromRect.right(), ay);
        path.lineTo(xm - r, ay);
        path.quadTo(QPointF(xm, ay), QPointF(xm, ay + dir * r));
        path.lineTo(xm, by - dir * r);
        path.quadTo(QPointF(xm, by), QPointF(xm + r, by));
        path.lineTo(toRect.left(), by);
    }

    auto* edge = m_scene->addPath(path, kEdgePen);
    edge->setZValue(0);

    const int prevId = from->data(ROLE_NODE_ID).toInt();
    const int nextId = to  ->data(ROLE_NODE_ID).toInt();
    if (prevId > 0 && nextId > 0) {
        linkEdge(prevId, nextId);
        m_edgeInto.insert(nextId, edge);
    }
}

// ===================== シーン再構築 =====================

void BranchTreeManager::rebuildBranchTree()
{
    if (!m_scene) return;
    ++m_rebuildCount;
    m_scene->clear();
    m_nodeIndex.clear();
    m_plyLabels.clear();   // scene->clear() で削除済み
    m_headerBand = nullptr;

    clearBranchGraph();
    m_prevSelected = nullptr;
    computeLayout();

    const QFont LABEL_FONT(QApplication::font().family(), 10);
    const QFont MOVE_NO_FONT(QApplication::font().family(), 9);
    m_columnSpacing = 110.0;
    for (const auto& row : std::as_const(m_rows)) {
        for (const auto& entry : row.disp) {
            const QFontMetrics spacingMetrics(!entry.usiMove.isEmpty()
                && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese
                    ? ApplicationFonts::japaneseFont(LABEL_FONT) : LABEL_FONT);
            const QString text = KifuPresentation::label(entry.prettyMove, entry.beforeSfen, entry.usiMove);
            m_columnSpacing = qMax(m_columnSpacing, qreal(spacingMetrics.horizontalAdvance(text) + 48));
        }
    }

    static bool fontDebugDone2 = false;
    if (!fontDebugDone2) {
        debugFontInfo(LABEL_FONT, "rebuildBranchGraph LABEL_FONT");
        debugFontInfo(MOVE_NO_FONT, "rebuildBranchGraph MOVE_NO_FONT");
        fontDebugDone2 = true;
    }

    // ===== 手数見出しの帯（上端に固定して表示する） =====
    {
        QColor band = m_branchTree ? m_branchTree->palette().color(QPalette::Base) : QColor(Qt::white);
        band.setAlpha(235);
        m_headerBand = m_scene->addRect(QRectF(0, 0, 1, kHeaderHeight), Qt::NoPen, band);
        m_headerBand->setZValue(40);
    }

    // ===== 「開始局面」ノード =====
    QGraphicsPathItem* startNode = nullptr;
    {
        const qreal x = kBaseX + kShiftX;
        const qreal y = laneY(0);
        const QString label = tr("開始局面");

        const QFontMetrics fm(LABEL_FONT);
        const int  wText = fm.horizontalAdvance(label);
        const int  hText = fm.height();
        const qreal padX = 14.0, padY = 8.0;
        const qreal rectW = qMax<qreal>(84.0, wText + padX * 2);
        const qreal rectH = qMax<qreal>(26.0, hText + padY * 2);

        QPainterPath path;
        const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
        path.addRoundedRect(rect, kRadius, kRadius);

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

        // 開始局面のコメント
        if (!m_rows.isEmpty() && !m_rows.at(0).disp.isEmpty()) {
            const QString comment = m_rows.at(0).disp.at(0).comment.trimmed();
            if (!comment.isEmpty()) {
                constexpr qreal d = 8.0;
                auto* marker = new QGraphicsEllipseItem(rect.right() - d - 3.0, rect.top() + 3.0, d, d, startNode);
                marker->setBrush(QColor(60, 130, 220));
                marker->setPen(QPen(Qt::white, 1.0));
                startNode->setToolTip(QStringLiteral("<qt><p style='white-space:pre-wrap'>%1</p></qt>")
                                          .arg(comment.left(kTooltipCommentChars).toHtmlEscaped()));
            }
        }

        const int nid = registerNode(/*row*/0, /*ply*/0, startNode);
        startNode->setData(ROLE_NODE_ID, nid);
    }

    // ===== 本譜 row=0 =====
    if (!m_rows.isEmpty()) {
        const auto& main = m_rows.at(0);

        QGraphicsPathItem* prev = startNode;
        for (qsizetype i = 1; i < main.disp.size(); ++i) {
            const auto& it = main.disp.at(i);
            const int ply = static_cast<int>(i);
            QGraphicsPathItem* node = addNode(0, ply, it);
            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // ===== 分岐 row=1.. =====
    qCDebug(lcUi).noquote() << "[BTM] rebuildBranchTree: m_rows.size=" << m_rows.size()
                            << "lanes=" << m_laneCount << "compact=" << m_compactLayout;
    for (qsizetype row = 1; row < m_rows.size(); ++row) {
        if (laneForRow(static_cast<int>(row)) < 0) continue;   // 折りたたみで隠れている
        const auto& rv = m_rows.at(row);
        const int startPly = qMax(1, rv.startPly);

        const int parentRow = resolveParentRowForVariation(static_cast<int>(row));
        const int joinPly = startPly - 1;

        auto isTerminalPly = [&](int targetRow, int p) -> bool {
            if (targetRow < 0 || targetRow >= m_rows.size()) return false;
            const auto& rowData = m_rows.at(targetRow);
            if (p < 0 || p >= rowData.disp.size()) return false;
            return isTerminalText(rowData.disp.at(p).prettyMove);
        };

        QGraphicsPathItem* prev = nullptr;

        if (!isTerminalPly(parentRow, joinPly)) {
            prev = m_nodeIndex.value(qMakePair(parentRow, joinPly), nullptr);
        }
        if (!prev && !isTerminalPly(0, joinPly)) {
            prev = m_nodeIndex.value(qMakePair(0, joinPly), nullptr);
        }
        if (!prev) {
            for (int p = joinPly - 1; p >= 0; --p) {
                if (!isTerminalPly(parentRow, p)) {
                    prev = m_nodeIndex.value(qMakePair(parentRow, p), nullptr);
                    if (prev) break;
                }
                if (!prev && !isTerminalPly(0, p)) {
                    prev = m_nodeIndex.value(qMakePair(0, p), nullptr);
                    if (prev) break;
                }
            }
        }
        if (!prev) {
            prev = m_nodeIndex.value(qMakePair(0, 0), nullptr);
        }

        const int lastPly = lastVisiblePly(static_cast<int>(row));
        for (int absPly = startPly; absPly <= lastPly && absPly < rv.disp.size(); ++absPly) {
            QGraphicsPathItem* node = addNode(static_cast<int>(row), absPly, rv.disp.at(absPly));

            node->setData(BR_ROLE_STARTPLY, startPly);
            node->setData(BR_ROLE_BUCKET,   row - 1);

            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

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
    return static_cast<int>(m_rows.at(row).disp.size());
}

bool BranchTreeManager::appendNodeToRow(int row, int ply, const KifDisplayItem& item, const QString& sfen)
{
    if (!m_scene || row < 0 || row >= m_rows.size()) return false;

    ResolvedRowLite& rv = m_rows[row];
    // disp の添字は手数と一致する（disp[0] = 開始局面）。末尾に連続して追加する場合のみ扱う
    if (ply < 1 || static_cast<int>(rv.disp.size()) != ply) return false;

    // 折りたたみで隠れている行は全再構築に任せる（現在の手を見せるため展開される）
    if (laneForRow(row) < 0 || isRowCollapsed(row)) return false;

    // 直前ノードが同じ行に描画済みであること。分岐行の先頭ノード（新規ライン）は
    // 親行との接続や行の並び替えが必要なため、全再構築に任せる
    QGraphicsPathItem* prev = m_nodeIndex.value(qMakePair(row, ply - 1), nullptr);
    if (!prev) return false;

    // 詰めた配置では、伸びた先に同じ段の別の変化があれば配置し直す
    if (m_compactLayout) {
        const int lane = laneForRow(row);
        for (qsizetype other = 0; other < m_rows.size(); ++other) {
            if (other == row || laneForRow(static_cast<int>(other)) != lane) continue;
            const int otherStart = qMax(1, m_rows.at(other).startPly);
            if (otherStart > rv.startPly && otherStart <= ply + 1) return false;
        }
    }

    QFont font(QApplication::font().family(), 10);
    if (!item.usiMove.isEmpty() && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese)
        font = ApplicationFonts::japaneseFont(font);
    const QFontMetrics metrics(font);
    const QString text = KifuPresentation::label(item.prettyMove, item.beforeSfen, item.usiMove);
    if (metrics.horizontalAdvance(text) + 48 > m_columnSpacing) {
        rv.disp.append(item);
        rv.sfen.append(sfen);
        rebuildBranchTree();
        return true;
    }
    QGraphicsPathItem* node = addNode(row, ply, item);
    if (row != 0) {
        node->setData(BR_ROLE_STARTPLY, qMax(1, rv.startPly));
        node->setData(BR_ROLE_BUCKET,   row - 1);
    }
    addMoveNumberLabel(ply);
    addEdge(prev, node);

    rv.disp.append(item);
    rv.sfen.append(sfen);

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
    const int mainLen = m_rows.isEmpty() ? 0 : static_cast<int>(qMax(qsizetype(0), m_rows.at(0).disp.size() - 1));
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
