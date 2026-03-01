/// @file branchtreemanager_draw.cpp
/// @brief BranchTreeManager のシーン構築・ノード/エッジ描画

#include "branchtreemanager.h"
#include "logcategories.h"

#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QFontInfo>
#include <QRegularExpression>

// ===================== ヘルパー関数 =====================

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

QGraphicsPathItem* BranchTreeManager::addNode(int row, int ply, const QString& rawText)
{
    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(getJapaneseFontFamily(), 10);
    static const    QFont MOVE_NO_FONT(getJapaneseFontFamily(), 9);

    static bool fontDebugDone = false;
    if (!fontDebugDone) {
        debugFontInfo(LABEL_FONT, "addNode LABEL_FONT");
        debugFontInfo(MOVE_NO_FONT, "addNode MOVE_NO_FONT");
        fontDebugDone = true;
    }

    const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
    const qreal y = BASE_Y + row * STEP_Y;

    static const QRegularExpression kDropHeadNumber(QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
    QString labelText = rawText;
    labelText.replace(kDropHeadNumber, QString());
    labelText = labelText.trimmed();

    if (labelText.endsWith(QLatin1Char('+'))) {
        labelText.chop(1);
        labelText = labelText.trimmed();
    }

    const bool odd = (ply % 2) == 1;

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
    path.addRoundedRect(rect, RADIUS, RADIUS);

    auto* item = m_scene->addPath(path, QPen(Qt::black, 1.2));
    item->setBrush(fill);
    item->setZValue(10);
    item->setData(ROLE_ORIGINAL_BRUSH, item->brush().color().rgba());

    item->setData(ROLE_ROW, row);
    item->setData(ROLE_PLY, ply);
    item->setData(BR_ROLE_KIND, (row == 0) ? BNK_Main : BNK_Var);
    if (row == 0) item->setData(BR_ROLE_PLY, ply);

    auto* textItem = m_scene->addSimpleText(labelText, LABEL_FONT);
    const QRectF br = textItem->boundingRect();
    textItem->setParentItem(item);
    textItem->setPos(rect.center().x() - br.width() / 2.0,
                     rect.center().y() - br.height() / 2.0);

    if (row == 0) {
        const QString moveNo = tr("%1\u624b\u76ee").arg(ply);
        auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
        const QRectF nbr = noItem->boundingRect();
        noItem->setParentItem(item);
        const qreal gap = 4.0;
        noItem->setPos(rect.center().x() - nbr.width() / 2.0,
                       rect.top() - gap - nbr.height());
    }

    m_nodeIndex.insert(qMakePair(row, ply), item);

    const int nodeId = registerNode(/*vid*/row, row, ply, item);
    item->setData(ROLE_NODE_ID, nodeId);

    return item;
}

void BranchTreeManager::addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to)
{
    if (!from || !to) return;

    const QPointF a = from->sceneBoundingRect().center();
    const QPointF b = to->sceneBoundingRect().center();

    QPainterPath path(a);
    const QPointF c1(a.x() + 8, a.y());
    const QPointF c2(b.x() - 8, b.y());
    path.cubicTo(c1, c2, b);

    auto* edge = m_scene->addPath(path, QPen(QColor(90, 90, 90), 1.0));
    edge->setZValue(0);

    const int prevId = from->data(ROLE_NODE_ID).toInt();
    const int nextId = to  ->data(ROLE_NODE_ID).toInt();
    if (prevId > 0 && nextId > 0) linkEdge(prevId, nextId);
}

// ===================== シーン再構築 =====================

void BranchTreeManager::rebuildBranchTree()
{
    if (!m_scene) return;
    m_scene->clear();
    m_nodeIndex.clear();

    clearBranchGraph();
    m_prevSelected = nullptr;

    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(getJapaneseFontFamily(), 10);
    static const    QFont MOVE_NO_FONT(getJapaneseFontFamily(), 9);

    static bool fontDebugDone2 = false;
    if (!fontDebugDone2) {
        debugFontInfo(LABEL_FONT, "rebuildBranchGraph LABEL_FONT");
        debugFontInfo(MOVE_NO_FONT, "rebuildBranchGraph MOVE_NO_FONT");
        fontDebugDone2 = true;
    }

    // ===== 「開始局面」ノード =====
    QGraphicsPathItem* startNode = nullptr;
    {
        const qreal x = BASE_X + SHIFT_X;
        const qreal y = BASE_Y + 0 * STEP_Y;
        const QString label = tr("\u958b\u59cb\u5c40\u9762");

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

        const int nid = registerNode(/*vid*/0, /*row*/0, /*ply*/0, startNode);
        startNode->setData(ROLE_NODE_ID, nid);
    }

    // ===== 本譜 row=0 =====
    if (!m_rows.isEmpty()) {
        const auto& main = m_rows.at(0);

        QGraphicsPathItem* prev = startNode;
        for (qsizetype i = 1; i < main.disp.size(); ++i) {
            const auto& it = main.disp.at(i);
            const int ply = static_cast<int>(i);
            QGraphicsPathItem* node = addNode(0, ply, it.prettyMove);
            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // ===== 分岐 row=1.. =====
    qCDebug(lcUi).noquote() << "[BTM] rebuildBranchTree: m_rows.size=" << m_rows.size();
    for (qsizetype row = 1; row < m_rows.size(); ++row) {
        const auto& rv = m_rows.at(row);
        const int startPly = qMax(1, rv.startPly);
        qCDebug(lcUi).noquote() << "[BTM] rebuildBranchTree: row=" << row
                           << " startPly=" << startPly
                           << " rv.disp.size=" << rv.disp.size()
                           << " rv.parent=" << rv.parent;

        const int parentRow = resolveParentRowForVariation(static_cast<int>(row));
        const int joinPly = startPly - 1;

        static const QStringList kTerminalKeywords = {
            QStringLiteral("\u6295\u4e86"), QStringLiteral("\u4e2d\u65ad"), QStringLiteral("\u6301\u5c06\u68cb"),
            QStringLiteral("\u5343\u65e5\u624b"), QStringLiteral("\u5207\u308c\u8ca0\u3051"),
            QStringLiteral("\u53cd\u5247\u52dd\u3061"), QStringLiteral("\u53cd\u5247\u8ca0\u3051"),
            QStringLiteral("\u5165\u7389\u52dd\u3061"), QStringLiteral("\u4e0d\u6226\u52dd"),
            QStringLiteral("\u4e0d\u6226\u6557"), QStringLiteral("\u8a70\u307f"), QStringLiteral("\u4e0d\u8a70"),
        };
        auto isTerminalPly = [&](int targetRow, int p) -> bool {
            if (targetRow < 0 || targetRow >= m_rows.size()) return false;
            const auto& rowData = m_rows.at(targetRow);
            if (p < 0 || p >= rowData.disp.size()) return false;
            const QString& text = rowData.disp.at(p).prettyMove;
            for (const auto& kw : kTerminalKeywords) {
                if (text.contains(kw)) return true;
            }
            return false;
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

        const int cut   = startPly;
        const qsizetype total = rv.disp.size();
        const int take  = (cut < total) ? static_cast<int>(total - cut) : 0;
        qCDebug(lcUi).noquote() << "[BTM] rebuildBranchTree: row=" << row
                           << " cut=" << cut << " total=" << total << " take=" << take;
        if (take <= 0) {
            qCDebug(lcUi).noquote() << "[BTM] rebuildBranchTree: SKIPPING row=" << row << " (no moves to draw)";
            continue;
        }

        for (int i = 0; i < take; ++i) {
            const auto& it = rv.disp.at(cut + i);
            const int absPly = startPly + i;

            QGraphicsPathItem* node = addNode(static_cast<int>(row), absPly, it.prettyMove);

            node->setData(BR_ROLE_STARTPLY, startPly);
            node->setData(BR_ROLE_BUCKET,   row - 1);

            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // ===== 手数ラベル補完 =====
    int maxAbsPly = 0;
    {
        const auto keys = m_nodeIndex.keys();
        for (const auto& k : keys) {
            const int ply = k.second;
            if (ply > maxAbsPly) maxAbsPly = ply;
        }
    }

    if (maxAbsPly > 0) {
        const QFontMetrics fmLabel(LABEL_FONT);
        const QFontMetrics fmMoveNo(MOVE_NO_FONT);
        Q_UNUSED(fmMoveNo);
        const int hText = fmLabel.height();
        const qreal padY = 6.0;
        const qreal rectH = qMax<qreal>(24.0, hText + padY * 2);
        const qreal gap   = 4.0;
        const qreal baselineY = BASE_Y + 0 * STEP_Y;
        const qreal topY = (baselineY - rectH / 2.0) - gap;

        for (int ply = 1; ply <= maxAbsPly; ++ply) {
            if (m_nodeIndex.contains(qMakePair(0, ply))) continue;

            const QString moveNo = tr("%1\u624b\u76ee").arg(ply);
            auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
            const QRectF nbr = noItem->boundingRect();

            const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
            noItem->setZValue(15);
            noItem->setPos(x - nbr.width() / 2.0, topY - nbr.height());
        }
    }

    // ===== シーン境界 =====
    const int mainLen = m_rows.isEmpty() ? 0 : static_cast<int>(qMax(qsizetype(0), m_rows.at(0).disp.size() - 1));
    const int spanLen = qMax(mainLen, maxAbsPly);
    const qreal width  = (BASE_X + SHIFT_X) + STEP_X * qMax(40, spanLen + 6) + 40.0;
    const qreal height = 30 + STEP_Y * static_cast<qreal>(qMax(qsizetype(2), m_rows.size() + 1));
    m_scene->setSceneRect(QRectF(0, 0, width, height));

    highlightBranchTreeAt(0, 0, /*centerOn=*/false);
}
