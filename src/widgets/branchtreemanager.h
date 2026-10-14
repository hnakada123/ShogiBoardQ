#ifndef BRANCHTREEMANAGER_H
#define BRANCHTREEMANAGER_H

/// @file branchtreemanager.h
/// @brief EngineAnalysisTab から分離した分岐ツリー管理クラスの定義

#include <QObject>
#include <QList>
#include <QMap>
#include <QPair>
#include <QHash>
#include <QPointer>
#include <QSet>

#include "branchtreeitemroles.h"

class KifuBranchNode;
class KifuBranchTree;
class QGraphicsItem;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QKeyEvent;
class QMenu;
class QPoint;
class QPen;

/**
 * @brief 分岐ツリーのグラフ構築・ハイライト・クリック検出を担うマネージャ
 *
 * EngineAnalysisTab から分岐ツリー描画の責務を分離したクラス。
 * QGraphicsView は外部（createBranchTreePage）で作成し、setView() で受け取る。
 *
 * 棋譜の分岐構造は KifuBranchTree を読むだけで、自分では手順や局面を持たない（読み取り専用の観察者）。
 * 各ノードの図形には KifuBranchNode の ID（ROLE_NODE_ID）を持たせ、手のつながり・表示内容・
 * 強調する経路はツリーから引く。手持ちの状態は描画用の配置（段・列幅）と折りたたみだけ。
 *
 * 行（row）は KifuBranchTree のライン番号（allLines() の添字）で、ナビゲーションとの受け渡しに使う。
 * 手は、その手を初めて含むライン（変化の先頭以降）の行に描く。
 * 画面上の段（lane）は描画用で、詰めて表示する場合は変化を親の手順のすぐ下の空いた段に置く。
 */
class BranchTreeManager : public QObject
{
    Q_OBJECT

public:
    explicit BranchTreeManager(QObject* parent = nullptr);
    ~BranchTreeManager() override;

    // --- 定数 ---
    enum BranchNodeKind { BNK_Start = 1, BNK_Main = 2, BNK_Var = 3 };
    static constexpr int BR_ROLE_KIND     = 0x200;
    static constexpr int BR_ROLE_PLY      = 0x201;
    static constexpr int BR_ROLE_STARTPLY = 0x202;
    static constexpr int BR_ROLE_BUCKET   = 0x203;

    static constexpr int ROLE_ROW            = BranchTreeItemRoles::Row;
    static constexpr int ROLE_PLY            = BranchTreeItemRoles::Ply;
    static constexpr int ROLE_ORIGINAL_BRUSH = 0x503;
    static constexpr int ROLE_NODE_ID        = BranchTreeItemRoles::NodeId;

    static constexpr int kMinZoomPercent = 50;
    static constexpr int kMaxZoomPercent = 200;

    // --- QGraphicsView 受け取り ---
    void setView(QGraphicsView* view);

    // --- 公開API ---
    /**
     * @brief 表示する棋譜ツリーを設定し、表示をツリーに合わせる（tree=nullptr なら開始局面だけ）
     *
     * 前回合わせたときから手順の末尾に1手増えただけなら、その手だけを描き足す
     * （盤上で手を指すたびに全体を作り直すと、手数に比例して着手が遅くなる）。
     * それ以外（表記・コメントの変更や分岐の追加など）は全体を作り直す。
     */
    void setBranchTree(KifuBranchTree* tree);
    void highlightBranchTreeAt(int row, int ply, bool centerOn = false);

    /// 描画済みの行（ライン）数
    int rowCount() const { return static_cast<int>(m_rows.size()); }

    /// 指定行の手順の長さ（開始局面を含むので「描画済み最終手数 + 1」）
    int rowDispCount(int row) const;

    /**
     * @brief ツリーの手順の末尾に加わった1ノードを差分描画する（ライブ対局用）
     * @param node 追加されたノード（葉で、直前の手が同じ行の末尾に描画済みであること）
     * @return 追加できた場合 true。行が無い・直前ノードが未描画（新規ラインの先頭など）・
     *         詰めた配置で他の変化と重なる場合は false を返し、呼び出し側は
     *         setBranchTree() による全再構築にフォールバックする。
     */
    bool appendNode(const KifuBranchNode* node);

    /// 全再構築（rebuildBranchTree）を実行した回数（テスト・計測用）
    int rebuildCount() const { return m_rebuildCount; }
    int lastHighlightedRow() const { return m_lastHighlightedRow; }
    int lastHighlightedPly() const { return m_lastHighlightedPly; }
    bool hasHighlightedNode() const { return m_lastHighlightedRow >= 0 && m_lastHighlightedPly >= 0; }

    /// 行 row の手数 ply に描いた手の KifuBranchNode ID。描いていなければ -1
    int  nodeIdFor(int row, int ply) const;

    void setBranchTreeClickEnabled(bool enabled) { m_branchTreeClickEnabled = enabled; }
    bool isBranchTreeClickEnabled() const { return m_branchTreeClickEnabled; }

    // --- 表示設定 ---

    /// 変化を空いた段に詰めて表示するか（false なら1変化1段）
    bool isCompactLayout() const { return m_compactLayout; }
    void setCompactLayout(bool compact);

    /// 表示倍率（%）
    int zoomPercent() const { return m_zoomPercent; }
    void setZoomPercent(int percent);

    /// 行（変化）を折りたたむ。先頭の手だけを残し、続きの手とその先の変化を隠す
    void setRowCollapsed(int row, bool collapsed);
    bool isRowCollapsed(int row) const;
    /// 折りたためる行か（本譜以外で2手以上あるか、先の変化を持つ）
    bool isRowCollapsible(int row) const;
    void expandAll();

    /// 画面上の段（テスト・自動化用）。行が描画されていなければ -1
    int laneForRow(int row) const;

signals:
    void branchNodeActivated(int row, int ply);

    /**
     * @brief ノードの右クリックメニューを表示する直前に通知する（同期接続）
     *
     * 受信側は menu に操作を追加できる。表示設定の項目はこの後に追加される。
     */
    void nodeContextMenuAboutToShow(int row, int ply, QMenu* menu);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    /// 行（ライン）の配置情報。手順そのものはツリーから引く
    struct RowInfo {
        int startPly = 1;      ///< この行に描く最初の手数（変化の先頭）
        int parent   = -1;     ///< 分岐元の行
        int headNodeId = -1;   ///< 変化の先頭の手の KifuBranchNode ID（折りたたみ状態の保持に使う）
        int length = 0;        ///< 開始局面を含む手順の長さ
    };

    /// ツリーの全ラインから行の配置情報を作り直す
    void syncRowsFromTree();
    /// 前回合わせたときから葉が1つ増えただけなら、その手を描き足して true を返す
    bool appendSingleMove();
    /// 描画に合わせたツリーの状態を覚える（次の差分判定に使う）
    void rememberTreeState();
    /// 行 row の手数 ply のノード（ツリーから引く）
    const KifuBranchNode* nodeAt(int row, int ply) const;
    /// 描画済みのノードの図形
    QGraphicsPathItem* itemForNode(const KifuBranchNode* node) const;

    qreal m_columnSpacing = 110.0;
    // --- 描画 ---
    /// 枝の線（通常）
    static QPen edgePen();
    /// 枝の線（選んでいる手順）
    static QPen pathEdgePen();
    void rebuildBranchTree();
    void computeLayout();
    qreal laneY(int lane) const;
    QGraphicsPathItem* addNode(int row, int ply, const KifuBranchNode* node);
    void addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to);
    int  resolveParentRowForVariation(int row) const;
    void highlightItem(QGraphicsPathItem* item, bool centerOn);
    void highlightPathTo(const KifuBranchNode* node);
    void scrollToNode(QGraphicsPathItem* item, bool centerOn);   ///< 表示中のときだけノードを見える位置へスクロールする
    void scrollToCurrentNode();   ///< 強調中のノードを見える位置へスクロールする（表示・リサイズ後）
    int  maxDrawnPly() const;
    void addMoveNumberLabel(int ply);      ///< 「n手目」の見出しを追加する
    void removeMoveNumberLabel(int ply);
    void updateSceneRect();
    void updateStickyHeader();             ///< 縦にスクロールしても手数の見出しを上端に表示する
    void onVerticalScrolled(int value);
    void applyZoom();

    // --- 折りたたみ ---
    bool isRowHidden(int row) const;       ///< 祖先の変化が折りたたまれていて描画しない行
    int  lastVisiblePly(int row) const;    ///< 行の描画する最後の手数
    int  hiddenNodeCount(int row) const;   ///< 折りたたみで隠れている手の数
    bool revealNode(int row, int ply);     ///< 指定ノードが隠れていれば折りたたみを解除する。解除したら true

    // --- 操作 ---
    bool handleKeyPress(QKeyEvent* event);
    void showContextMenu(const QPoint& viewportPos, const QPoint& globalPos);
    QGraphicsItem* nodeItemAt(const QPoint& viewportPos) const;

    // --- UI ---
    QGraphicsView*  m_branchTree = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QWidget*        m_branchTreeViewport = nullptr;
    QGraphicsRectItem* m_headerBand = nullptr;   ///< 手数見出しの背景（上端に固定）

    // --- データ（ツリーは非所有。表示はツリーを読むだけ） ---
    QPointer<KifuBranchTree> m_tree;
    quint64 m_syncedRevision = 0;                 ///< 描画に合わせたときの KifuBranchTree::revision()
    int m_syncedNodeCount = -1;                   ///< 描画に合わせたときのノード数
    QList<RowInfo> m_rows;
    QList<int> m_rowLane;                         ///< 行 → 段（隠れている行は -1）
    int m_laneCount = 1;
    QMap<QPair<int,int>, QGraphicsPathItem*> m_nodeIndex;   ///< 描いた (行, 手数) → 図形
    QHash<int, QGraphicsPathItem*> m_itemByNodeId;          ///< KifuBranchNode ID → 図形
    QHash<int, QGraphicsSimpleTextItem*> m_plyLabels;   ///< 「n手目」の見出し（ply → item）
    QGraphicsPathItem* m_prevSelected = nullptr;
    bool m_branchTreeClickEnabled = true;
    int m_rebuildCount = 0;
    bool m_compactLayout = true;
    int m_zoomPercent = 100;
    QSet<int> m_collapsedHeads;                   ///< 折りたたんだ変化の先頭ノードID（KifuBranchNode）

    // --- 強調 ---
    QHash<int, QGraphicsPathItem*> m_edgeInto;   ///< KifuBranchNode ID → そのノードへ入る辺
    QList<QGraphicsPathItem*> m_highlightedEdges;
    int m_lastHighlightedRow = -1;
    int m_lastHighlightedPly = -1;
};

#endif // BRANCHTREEMANAGER_H
