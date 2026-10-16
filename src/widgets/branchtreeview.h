#ifndef BRANCHTREEVIEW_H
#define BRANCHTREEVIEW_H

/// @file branchtreeview.h
/// @brief 分岐ツリーの図（ドックの中身。描画・強調・キーとマウスの操作）

#include <QGraphicsView>
#include <QHash>
#include <QList>
#include <QPointer>

#include "branchtreelayout.h"

class BranchNodeItem;
class KifuBranchNode;
class KifuBranchTree;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QMenu;

/**
 * @brief 分岐ツリーの図
 *
 * 棋譜の分岐構造は KifuBranchTree を読むだけで、自分では手順や局面を持たない。ツリーの変更の知らせ
 * （nodeAppended など）を受けて描き直す。手順の末尾に1手足しただけならその手だけを描き足す
 * （盤上で手を指すたびに全体を作り直すと、手数に比例して着手が遅くなる）。
 *
 * 外とのやり取りは KifuBranchNode の ID だけで行う（表示中の手・選んだ手・右クリックした手）。
 * 図の中の行（変化）は KifuBranchTree のライン番号で、手はその手を初めて含むラインの行に描く。
 * 画面上の段（lane）は描画用で、詰めて表示する場合は変化を親の手順のすぐ下の空いた段に置く。
 */
class BranchTreeView : public QGraphicsView
{
    Q_OBJECT

public:
    static constexpr int kMinZoomPercent = 50;
    static constexpr int kMaxZoomPercent = 200;

    explicit BranchTreeView(QWidget* parent = nullptr);
    ~BranchTreeView() override;

    /// 描くツリー（nullptr なら開始局面だけ）。変更の知らせを受けて描き直す
    void setTree(KifuBranchTree* tree);
    KifuBranchTree* tree() const;

    /**
     * @brief 表示中の手を目立たせる（折りたたんだ変化の中なら展開する）
     *
     * まだ描いていない手（ツリーの変更の知らせより先に届いたとき）は、描いた時点で目立たせる。
     */
    void setCurrentNode(int nodeId, bool center = false);
    /// 目立たせている手の ID（描いていなければ -1）
    int currentNodeId() const;

    /// 手の表記を描き直す（表記の設定・言語を変えたとき）
    void refreshLabels();

    /// 選んで移れるか（対局中などは選べない）
    void setClickEnabled(bool enabled) { m_clickEnabled = enabled; }
    bool isClickEnabled() const { return m_clickEnabled; }

    // --- 表示設定 ---

    /// 変化を空いた段に詰めて表示するか（false なら1変化1段）
    bool isCompactLayout() const { return m_compactLayout; }
    void setCompactLayout(bool compact);

    /// 表示倍率（%）
    int zoomPercent() const { return m_zoomPercent; }
    void setZoomPercent(int percent);

    /// nodeId を描いた変化を折りたたむ。先頭の手だけを残し、続きの手とその先の変化を隠す
    void setCollapsed(int nodeId, bool collapsed);
    bool isCollapsed(int nodeId) const;
    /// 折りたためる変化の手か（本譜以外で2手以上あるか、先の変化を持つ）
    bool isCollapsible(int nodeId) const;
    void expandAll();

    // --- 描いた結果（テスト・自動化用） ---

    /// 手を描いているか（折りたたみで隠れていれば false）
    bool isDrawn(int nodeId) const { return m_items.contains(nodeId); }
    /// 手を描いた画面上の段（描いていなければ -1）
    int laneOf(int nodeId) const;
    /// 全体を作り直した回数
    int rebuildCount() const { return m_rebuildCount; }

public slots:
    /// 手のコメントを描き直す（KifuDocument::commentChanged に接続する）
    void refreshNode(int nodeId);

signals:
    /// 手を選んだ（クリック・キー操作）
    void nodeActivated(int nodeId);

    /**
     * @brief 手の右クリックメニューを表示する直前に通知する（同期接続）
     *
     * 受信側は menu に操作を追加できる。表示設定の項目はこの後に追加される。
     */
    void nodeContextMenuAboutToShow(int nodeId, QMenu* menu);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    // --- ツリーの変更 ---
    void onNodeAppended(int parentId, int nodeId);
    void onStructureChanged();

    /// ツリーの全ラインから行の配置を作り直す
    void syncRowsFromTree();
    /// 手順の末尾に加わった手を描き足す。描き足せなければ（新しい変化など）false
    bool appendNode(const KifuBranchNode* node);

    // --- 描画 ---
    void rebuild();
    BranchNodeItem* addNode(int row, const KifuBranchNode* node);
    void addEdge(const BranchNodeItem* from, const BranchNodeItem* to);
    void addMoveNumberLabel(int ply);
    void updateSceneRect();
    void updateStickyHeader();
    void applyZoom();
    qreal laneY(int lane) const;
    int maxDrawnPly() const;

    // --- 強調 ---
    void applyCurrent(bool center);
    void highlightPathTo(const KifuBranchNode* node);
    void scrollToNode(const BranchNodeItem* item, bool center);
    void scrollToCurrentNode();

    // --- 操作 ---
    BranchNodeItem* nodeItemAt(const QPoint& viewportPos) const;
    /// キーで移る先の手（移らなければ nullptr）
    const BranchNodeItem* keyTarget(int key) const;
    void activate(int nodeId);
    void showContextMenu(const QPoint& viewportPos, const QPoint& globalPos);
    /// 手を描いた行（描いていない手は、その手を含む最初のライン）。無ければ -1
    int rowOf(int nodeId) const { return m_rowOf.value(nodeId, -1); }

    QGraphicsScene* m_scene = nullptr;
    QGraphicsRectItem* m_headerBand = nullptr;   ///< 手数見出しの背景（上端に固定）

    // --- データ（ツリーは非所有。表示はツリーを読むだけ） ---
    QPointer<KifuBranchTree> m_tree;
    BranchTreeLayout m_layout;                   ///< 行の配置・折りたたみ・段
    QHash<int, int> m_rowOf;                     ///< KifuBranchNode ID → 描く行
    QHash<int, BranchNodeItem*> m_items;         ///< KifuBranchNode ID → 描いた手
    QHash<int, QGraphicsPathItem*> m_edgeInto;   ///< KifuBranchNode ID → その手へ入る枝
    QHash<int, QGraphicsSimpleTextItem*> m_plyLabels;   ///< 「n手目」の見出し（手数 → 見出し）
    QList<QGraphicsPathItem*> m_highlightedEdges;
    qreal m_columnSpacing = 110.0;
    int m_currentNodeId = -1;                    ///< 表示中の手（描く前に届くことがある）
    BranchNodeItem* m_currentItem = nullptr;     ///< 目立たせている手
    bool m_clickEnabled = true;
    bool m_compactLayout = true;
    int m_zoomPercent = 100;
    int m_rebuildCount = 0;
};

#endif // BRANCHTREEVIEW_H
