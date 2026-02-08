#ifndef BRANCHTREEWIDGET_H
#define BRANCHTREEWIDGET_H

/// @file branchtreewidget.h
/// @brief 分岐ツリーグラフウィジェットクラスの定義


#include <QWidget>
#include <QHash>
#include <QPair>
#include <QVector>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPathItem;
class KifuBranchTree;
class KifuBranchNode;
struct BranchLine;

/**
 * @brief 分岐ツリーを描画するウィジェット
 *
 * KifuBranchTreeのデータをグラフィカルに表示する。
 * EngineAnalysisTabから分離された独立ウィジェット。
 */
class BranchTreeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BranchTreeWidget(QWidget* parent = nullptr);
    ~BranchTreeWidget() override;

    /**
     * @brief ツリーデータを設定
     */
    void setTree(KifuBranchTree* tree);

    /**
     * @brief ツリーを取得
     */
    KifuBranchTree* tree() const { return m_tree; }

    /**
     * @brief 指定ノードをハイライト（黄色）
     * @param lineIndex ラインインデックス（0=本譜）
     * @param ply 手数
     */
    void highlightNode(int lineIndex, int ply);

    /**
     * @brief ハイライトをクリア
     */
    void clearHighlight();

    /**
     * @brief クリック有効/無効を設定
     */
    void setClickEnabled(bool enabled) { m_clickEnabled = enabled; }

    /**
     * @brief クリックが有効かどうか
     */
    bool isClickEnabled() const { return m_clickEnabled; }

    /**
     * @brief ビューを取得（外部からの操作用）
     */
    QGraphicsView* view() const { return m_view; }

    /**
     * @brief シーンを取得
     */
    QGraphicsScene* scene() const { return m_scene; }

public slots:
    /**
     * @brief ツリーを再描画
     */
    void rebuild();

signals:
    /**
     * @brief ノードがクリックされた
     * @param lineIndex ラインインデックス
     * @param ply 手数
     */
    void nodeClicked(int lineIndex, int ply);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // 描画メソッド
    void buildUi();
    QGraphicsPathItem* addNode(int row, int ply, const QString& text, bool isMainLine);
    void addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to);
    void drawStartNode();
    void drawMainLine();
    void drawBranchLines();
    void drawMoveNumberLabels(int maxPly);

    // ノード検索
    QGraphicsPathItem* findNodeAt(int row, int ply) const;
    int calculateRowForLine(int lineIndex) const;

    // データ
    KifuBranchTree* m_tree = nullptr;
    bool m_clickEnabled = true;

    // UI
    QGraphicsView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;

    // ノードインデックス: (row, ply) -> item
    QHash<QPair<int, int>, QGraphicsPathItem*> m_nodeIndex;

    // ライン情報キャッシュ
    QVector<BranchLine> m_linesCache;

    // 前回ハイライトしたノード
    QGraphicsPathItem* m_prevHighlighted = nullptr;

    // データロール
    static constexpr int ROLE_ROW = 0x501;
    static constexpr int ROLE_PLY = 0x502;
    static constexpr int ROLE_LINE_INDEX = 0x503;
    static constexpr int ROLE_ORIGINAL_BRUSH = 0x504;

    // レイアウト定数
    static constexpr qreal STEP_X = 110.0;
    static constexpr qreal BASE_X = 40.0;
    static constexpr qreal SHIFT_X = 40.0;
    static constexpr qreal BASE_Y = 40.0;
    static constexpr qreal STEP_Y = 56.0;
    static constexpr qreal RADIUS = 8.0;
};

#endif // BRANCHTREEWIDGET_H
