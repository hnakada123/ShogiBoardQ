#ifndef BRANCHTREEMANAGER_H
#define BRANCHTREEMANAGER_H

/// @file branchtreemanager.h
/// @brief EngineAnalysisTab から分離した分岐ツリー管理クラスの定義

#include <QObject>
#include <QVector>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QPair>
#include <QHash>
#include <QSet>

#include "kifdisplayitem.h"

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPathItem;

/**
 * @brief 分岐ツリーのグラフ構築・ハイライト・クリック検出を担うマネージャ
 *
 * EngineAnalysisTab から分岐ツリー描画の責務を分離したクラス。
 * QGraphicsView は外部（createBranchTreePage）で作成し、setView() で受け取る。
 */
class BranchTreeManager : public QObject
{
    Q_OBJECT

public:
    explicit BranchTreeManager(QObject* parent = nullptr);
    ~BranchTreeManager() override;

    // --- 軽量行データ（旧 EngineAnalysisTab::ResolvedRowLite） ---
    struct ResolvedRowLite {
        int startPly = 1;
        int parent   = -1;
        QList<KifDisplayItem> disp;
        QStringList sfen;
    };

    // --- グラフノード ---
    struct BranchGraphNode {
        int id   = -1;
        int vid  = -1;
        int row  = -1;
        int ply  =  0;
        QGraphicsPathItem* item = nullptr;
    };

    // --- 定数 ---
    enum BranchNodeKind { BNK_Start = 1, BNK_Main = 2, BNK_Var = 3 };
    static constexpr int BR_ROLE_KIND     = 0x200;
    static constexpr int BR_ROLE_PLY      = 0x201;
    static constexpr int BR_ROLE_STARTPLY = 0x202;
    static constexpr int BR_ROLE_BUCKET   = 0x203;

    static constexpr int ROLE_ROW            = 0x501;
    static constexpr int ROLE_PLY            = 0x502;
    static constexpr int ROLE_ORIGINAL_BRUSH = 0x503;
    static constexpr int ROLE_NODE_ID        = 0x504;

    // --- QGraphicsView 受け取り ---
    void setView(QGraphicsView* view);

    // --- 公開API ---
    void setBranchTreeRows(const QVector<ResolvedRowLite>& rows);
    void highlightBranchTreeAt(int row, int ply, bool centerOn = false);
    int lastHighlightedRow() const { return m_lastHighlightedRow; }
    int lastHighlightedPly() const { return m_lastHighlightedPly; }
    bool hasHighlightedNode() const { return m_lastHighlightedRow >= 0 && m_lastHighlightedPly >= 0; }

    void clearBranchGraph();
    int  registerNode(int vid, int row, int ply, QGraphicsPathItem* item);
    void linkEdge(int prevId, int nextId);
    int  nodeIdFor(int row, int ply) const;

    void setBranchTreeClickEnabled(bool enabled) { m_branchTreeClickEnabled = enabled; }
    bool isBranchTreeClickEnabled() const { return m_branchTreeClickEnabled; }

signals:
    void branchNodeActivated(int row, int ply);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // --- 描画 ---
    void rebuildBranchTree();
    QGraphicsPathItem* addNode(int row, int ply, const QString& text);
    void addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to);
    int  resolveParentRowForVariation(int row) const;
    int  graphFallbackToPly(int row, int targetPly) const;
    void highlightNodeId(int nodeId, bool centerOn);

    // --- UI ---
    QGraphicsView*  m_branchTree = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QWidget*        m_branchTreeViewport = nullptr;

    // --- データ ---
    QVector<ResolvedRowLite> m_rows;
    QMap<QPair<int,int>, QGraphicsPathItem*> m_nodeIndex;
    QGraphicsPathItem* m_prevSelected = nullptr;
    bool m_branchTreeClickEnabled = true;

    // --- グラフ ---
    QHash<QPair<int,int>, int> m_nodeIdByRowPly;
    QHash<int, BranchGraphNode> m_nodesById;
    QHash<int, QVector<int>>    m_prevIds;
    QHash<int, QVector<int>>    m_nextIds;
    QHash<int, int>             m_rowEntryNode;
    int m_nextNodeId = 1;
    int m_lastHighlightedRow = -1;
    int m_lastHighlightedPly = -1;
};

#endif // BRANCHTREEMANAGER_H
