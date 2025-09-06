#include "kifuvariationgraphwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <algorithm>

KifuVariationGraphWidget::KifuVariationGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::white);
    setPalette(pal);
}

void KifuVariationGraphWidget::setMainline(const QStringList& prettyMoves)
{
    m_mainPretty = prettyMoves; // prettyMoves[ply]
    rebuildGeometry();
    update();
}

void KifuVariationGraphWidget::setVariationPaths(const QVector<VariationPath>& paths)
{
    m_paths = paths;
    rebuildGeometry();
    update();
}

void KifuVariationGraphWidget::setCurrentPly(int ply)
{
    m_currentPly = std::max(0, ply);
    update();
}

int KifuVariationGraphWidget::allocLaneForStartPly(int startPly)
{
    int used = m_laneUsedAtStart.value(startPly, 0);
    m_laneUsedAtStart[startPly] = used + 1;
    return used; // 0始まり
}

void KifuVariationGraphWidget::rebuildGeometry()
{
    m_nodes.clear();
    m_laneUsedAtStart.clear();

    // 列数＝本譜の手数（=最後のインデックス）
    int lastPly = 0;
    for (int i = m_mainPretty.size() - 1; i >= 0; --i) {
        if (!m_mainPretty.value(i).isEmpty()) { lastPly = i; break; }
    }
    if (lastPly < 0) lastPly = 0;

    // 本譜ノード（行=0段）
    for (int ply = 1; ply <= lastPly; ++ply) {
        const int colX = m_leftMargin + (ply - 1) * m_colW;
        const int y = m_topMargin;
        QRectF rc(colX, y, m_colW - m_hgap, m_rowH);
        m_nodes.push_back(Node{rc, ply, true, 0, -1});
    }

    // 分岐ノード（各変化ごとに、startPly以降に横へ並べる。縦はstartPlyごとのレーン割当）
    for (const auto& path : m_paths) {
        if (path.startPly <= 0 || path.labels.isEmpty()) continue;

        int lane = allocLaneForStartPly(path.startPly); // 0,1,2,...
        // 下段のYは mainline行の下に laneごとに積む
        int baseY = m_topMargin + m_rowH + m_vgap + lane * (m_rowH + m_vgap);

        // 変化の i 手目は全体の手数でいうと (startPly + i - 1) 列に描く
        for (int i = 0; i < path.labels.size(); ++i) {
            int ply = path.startPly + i;
            const int colX = m_leftMargin + (ply - 1) * m_colW;
            QRectF rc(colX, baseY, m_colW - m_hgap, m_rowH);
            m_nodes.push_back(Node{rc, ply, false, path.startPly, path.bucketIndex});
        }
    }

    // サイズ目安を更新
    int lanesMax = 0;
    for (auto it = m_laneUsedAtStart.constBegin(); it != m_laneUsedAtStart.constEnd(); ++it)
        lanesMax = std::max(lanesMax, it.value());

    int height = m_topMargin + m_rowH + (lanesMax>0 ? (m_vgap + lanesMax * (m_rowH + m_vgap)) : 0) + 16;
    int width  = m_leftMargin + std::max(1, lastPly) * m_colW + 16;
    setMinimumSize(width, height);
}

void KifuVariationGraphWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFontMetrics fm(font());

    // 列タイトル（1手目、2手目...）
    // 最大列 = 本譜終端 と 変化の末尾 から算出
    int maxPly = 0;
    for (const auto& n : m_nodes) maxPly = std::max(maxPly, n.ply);
    for (int ply = 1; ply <= maxPly; ++ply) {
        const int x = m_leftMargin + (ply - 1) * m_colW;
        QString cap = QString::number(ply) + tr("手目");
        p.setPen(QColor(90,90,90));
        p.drawText(QRect(x, 0, m_colW, m_topMargin-6), Qt::AlignHCenter|Qt::AlignVCenter, cap);
    }

    // エッジ（線）
    auto centerOf = [](const QRectF& r){ return QPointF(r.center().x(), r.center().y()); };
    p.setPen(QPen(QColor(90,90,90,140), 1.0));

    // 本譜：隣接ノードを直線で
    QVector<const Node*> mainNodes;
    for (const auto& n : m_nodes) if (n.isMain) mainNodes.push_back(&n);
    std::sort(mainNodes.begin(), mainNodes.end(), [](auto a, auto b){ return a->ply < b->ply; });
    for (int i = 1; i < mainNodes.size(); ++i) {
        p.drawLine(centerOf(mainNodes[i-1]->rect), centerOf(mainNodes[i]->rect));
    }

    // 分岐：その変化の連続ノードを直線で。先頭は対応する本譜ノードとも結ぶ
    // 先に startPly/bucketIndex ごとに集約
    struct Key { int s; int b; };
    auto keyLess = [](const Key& a, const Key& b){ return a.s<b.s || (a.s==b.s && a.b<b.b); };
    QMap<QPair<int,int>, QVector<const Node*>> paths;
    for (const auto& n : m_nodes) {
        if (n.isMain) continue;
        paths[{n.startPly, n.bucketIndex}].push_back(&n);
    }
    // 本譜ノードを ply->Node* で拾う
    QMap<int,const Node*> mainByPly;
    for (const auto* n : mainNodes) mainByPly[n->ply] = n;

    for (auto it = paths.begin(); it != paths.end(); ++it) {
        auto vec = it.value();
        std::sort(vec.begin(), vec.end(), [](auto a, auto b){ return a->ply < b->ply; });
        // startPly の本譜ノードと、変化最初のノードを結ぶ
        if (!vec.isEmpty()) {
            const int sp = it.key().first;
            if (mainByPly.contains(sp)) {
                p.drawLine(centerOf(mainByPly[sp]->rect), centerOf(vec.first()->rect));
            }
        }
        for (int i = 1; i < vec.size(); ++i) {
            p.drawLine(centerOf(vec[i-1]->rect), centerOf(vec[i]->rect));
        }
    }

    // ノード本体
    for (const auto& n : m_nodes) {
        const bool isSel = (n.isMain && n.ply==m_currentPly);
        const QColor fill = n.isMain
                                ? (isSel ? QColor(180,220,255) : QColor(210,232,246))
                                : QColor(235,244,250);
        const QColor frame = n.isMain ? QColor(60,120,170) : QColor(120,150,170);

        p.setBrush(fill);
        p.setPen(QPen(frame, 1.0));
        p.drawRoundedRect(n.rect, m_corner, m_corner);

        QString label;
        if (n.isMain) {
            label = m_mainPretty.value(n.ply);
        } else {
            // 変化のこの列のラベルは setVariationPaths 側で列に合わせて入れているので、
            // 再検索する必要なく、概ね列幅に入るように切り詰め
            // ここでは簡便に ply から求め直す
            // ただし描画だけのため、やや冗長だがOK
            for (const auto& path : m_paths) {
                if (path.startPly == n.startPly && path.bucketIndex == n.bucketIndex) {
                    int idx = n.ply - path.startPly;
                    if (idx>=0 && idx<path.labels.size()) label = path.labels[idx];
                    break;
                }
            }
        }
        if (label.isEmpty()) continue;

        // 文字を若干短く省略
        const int pad = 6;
        QString el = label;
        el.replace(QChar::Nbsp, QLatin1Char(' '));
        while (el.size() > 2 && fm.horizontalAdvance(el) > n.rect.width() - pad*2) {
            el.chop(1);
        }
        p.setPen(QColor(40,40,40));
        p.drawText(n.rect.adjusted(pad,0,-pad,0), Qt::AlignCenter, el);
    }
}

void KifuVariationGraphWidget::mousePressEvent(QMouseEvent* e)
{
    const QPointF pt = e->position();
    for (const auto& n : m_nodes) {
        if (n.rect.contains(pt)) {
            if (n.isMain) {
                emit mainlineNodeClicked(n.ply);
            } else {
                emit variationNodeClicked(n.startPly, n.bucketIndex);
            }
            break;
        }
    }
}
