#ifndef ENGINEANALYSISTAB_H
#define ENGINEANALYSISTAB_H

#include <QWidget>
#include <QVector>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QPair>

class QTabWidget;
class QTableView;
class QTextBrowser;
class QPlainTextEdit;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPathItem;
class QHeaderView;

class EngineInfoWidget;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

#include "kifdisplayitem.h"

class EngineAnalysisTab : public QWidget {
    Q_OBJECT
public:
    explicit EngineAnalysisTab(QWidget* parent=nullptr);

    void setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                   UsiCommLogModel* log1, UsiCommLogModel* log2);

    // 既存コードの m_tab をそのまま使えるように
    QTabWidget* tab() const;

    // --- 分岐ツリー：MainWindow から供給する軽量行データ ---
    struct ResolvedRowLite {
        int startPly = 1;                  // 行の開始手（本譜は常に1）
        QList<KifDisplayItem> disp;        // 表示テキスト列（「▲７六歩(77)」など）
        QStringList sfen;                  // 0..N の局面列（未使用でもOK）
    };

    // ツリーの全行データをセット（row=0:本譜、row=1..:分岐）
    void setBranchTreeRows(const QVector<ResolvedRowLite>& rows);

    // ツリー上で (row, ply) をハイライト（必要なら centerOn）
    void highlightBranchTreeAt(int row, int ply, bool centerOn=false);

    // --- 互換API（MainWindowの既存呼び出しを満たす） ---
    // 2番目エンジンのビュー（情報＆思考表）の表示/非表示
    void setSecondEngineVisible(bool on);
    // 旧名エイリアス（上と同義）
    void setDualEngineVisible(bool on) { setSecondEngineVisible(on); }

    // 1/2の思考モデルを別々に差し替え
    void setEngine1ThinkingModel(ShogiEngineThinkingModel* m);
    void setEngine2ThinkingModel(ShogiEngineThinkingModel* m);

    // 旧API（プレーンテキストでコメントを設定）
    void setCommentText(const QString& text);

    // 分岐ツリー用の種類とロール
    enum BranchNodeKind { BNK_Start = 1, BNK_Main = 2, BNK_Var = 3 };
    static constexpr int BR_ROLE_KIND     = 0x200;
    static constexpr int BR_ROLE_PLY      = 0x201; // 本譜ノードの ply
    static constexpr int BR_ROLE_STARTPLY = 0x202; // 分岐の開始手
    static constexpr int BR_ROLE_BUCKET   = 0x203; // 同一開始手内での分岐Index

public slots:
    void setAnalysisVisible(bool on);
    void setCommentHtml(const QString& html);

signals:
    // ツリー上のノード（行row, 手ply）がクリックされた
    void branchNodeActivated(int row, int ply);

    // MainWindow に「何を適用したいか」を伝える
    void requestApplyStart();                         // 開始局面へ
    void requestApplyMainAtPly(int ply);             // 本譜の ply 手へ
    void requestApplyVariation(int startPly, int bucketIndex); // 分岐へ

private:
    void buildUi();

    // --- 内部：ツリー描画 ---
    void rebuildBranchTree();
    QGraphicsPathItem* addNode(int row, int ply, const QString& text);
    void addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to);

    // --- UI ---
    QTabWidget* m_tab=nullptr;
    EngineInfoWidget *m_info1=nullptr, *m_info2=nullptr;
    QTableView *m_view1=nullptr, *m_view2=nullptr;
    QPlainTextEdit* m_usiLog=nullptr;
    QTextBrowser* m_comment=nullptr;
    QGraphicsView* m_branchTree=nullptr;
    QGraphicsScene* m_scene=nullptr;

    // --- モデル参照（所有しない） ---
    ShogiEngineThinkingModel* m_model1=nullptr;
    ShogiEngineThinkingModel* m_model2=nullptr;
    UsiCommLogModel* m_log1=nullptr;
    UsiCommLogModel* m_log2=nullptr;

    // --- 分岐データ ---
    QVector<ResolvedRowLite> m_rows;   // 行0=本譜、行1..=ファイル登場順の分岐

    // クリック判定用： (row,ply) -> node item
    QMap<QPair<int,int>, QGraphicsPathItem*> m_nodeIndex;

    // item->data 用ロールキー
    static constexpr int ROLE_ROW = 0x501;
    static constexpr int ROLE_PLY = 0x502;

    // ツリークリック検出
    bool eventFilter(QObject* obj, QEvent* ev) override;
};

#endif // ENGINEANALYSISTAB_H
