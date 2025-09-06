#ifndef KIFUVARIATIONGRAPHWIDGET_H
#define KIFUVARIATIONGRAPHWIDGET_H

#include <QWidget>
#include <QVector>
#include <QStringList>
#include <QMap>

class KifuVariationGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KifuVariationGraphWidget(QWidget* parent = nullptr);

    // 本譜：各手の表示テキスト（例：「▲７六歩(77)」）。index=1..N を想定し、index=0は空でもOK
    void setMainline(const QStringList& prettyMoves);  // prettyMoves[ply]

    // 分岐：1本の変化を「開始手目＋その変化で進むテキスト列」として渡す
    struct VariationPath {
        int startPly = 0;              // この手目から分岐
        int bucketIndex = 0;           // m_variationsByPly[startPly] 内のインデックス
        QStringList labels;            // 変化の表示（v.disp.*.prettyMove）
    };
    void setVariationPaths(const QVector<VariationPath>& paths);

    // 選択表示のため（本譜のどの手を選んでいるか）
    void setCurrentPly(int ply);

signals:
    void mainlineNodeClicked(int ply);
    void variationNodeClicked(int startPly, int bucketIndex);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    QSize sizeHint() const override { return {900, 220}; }

private:
    struct Node {
        QRectF rect;
        int ply = 0;
        bool isMain = true;
        int startPly = 0;
        int bucketIndex = -1; // 分岐なら有効
    };

    // 描画パラメータ
    int m_colW = 120;     // 列幅（手数1列）
    int m_rowH = 28;      // ノード高さ
    int m_topMargin = 26; // 列タイトル(「1手目」など)分
    int m_leftMargin = 40;
    int m_vgap = 10;      // ノード縦間
    int m_hgap = 12;      // ノード横間
    int m_corner = 6;

    QStringList m_mainPretty;            // index=ply
    QVector<VariationPath> m_paths;
    QMap<int,int> m_laneUsedAtStart;     // startPlyごとの使用レーン数
    int m_currentPly = 0;

    QVector<Node> m_nodes;

    void rebuildGeometry();              // m_nodes 再構築
    int  allocLaneForStartPly(int startPly); // startPlyごとの下段レーンを割り当て
};

#endif // KIFUVARIATIONGRAPHWIDGET_H
