#ifndef ENGINEANALYSISTAB_H
#define ENGINEANALYSISTAB_H

#include <QWidget>
#include <QVector>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QPair>
#include <QHash>
#include <QSet>
#include <QAbstractItemModel>

class QTabWidget;
class QTableView;
class QTextBrowser;
class QTextEdit;
class QPlainTextEdit;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPathItem;
class QHeaderView;
class QToolButton;
class QPushButton;
class QLabel;  // ★ 追加

class EngineInfoWidget;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

#include "kifdisplayitem.h"

class EngineAnalysisTab : public QWidget
{
    Q_OBJECT

public:
    explicit EngineAnalysisTab(QWidget* parent=nullptr);

    void buildUi();

    void setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                   UsiCommLogModel* log1, UsiCommLogModel* log2);

    // 既存コードの m_tab をそのまま使えるように
    QTabWidget* tab() const;

    // --- 分岐ツリー：MainWindow から供給する軽量行データ ---
    struct ResolvedRowLite {
        int startPly = 1;                  // 行の開始手（本譜は常に1）
        int parent   = -1;                 // ★ 追加: 親行のインデックス（-1=なし/本譜）
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

    // item->data 用ロールキー
    static constexpr int ROLE_ROW = 0x501;
    static constexpr int ROLE_PLY = 0x502;
    static constexpr int ROLE_ORIGINAL_BRUSH = 0x503;
    static constexpr int ROLE_NODE_ID = 0x504;     // ★ 追加：グラフノードID

    // ===== 追加：分岐ツリー・グラフAPI（初期構築時に使用） =====
    // 再構築の先頭で呼ぶ（内部グラフをクリア）
    void clearBranchGraph();

    // 矩形を生成した直後に呼ぶ：ノード登録して nodeId を返す
    // ※戻り値は各矩形に setData(ROLE_NODE_ID, id) して保持してください
    int registerNode(int vid, int row, int ply, QGraphicsPathItem* item);

    // 罫線を引く直前に、接続する2つのノード id で呼ぶ
    void linkEdge(int prevId, int nextId);

    // アクセサ： (row,ply) → nodeId（無ければ -1）
    int nodeIdFor(int row, int ply) const {
        return m_nodeIdByRowPly.value(qMakePair(row, ply), -1);
    }

    // ★ 追加（HvE/EvE の配線で使う）
    EngineInfoWidget* info1() const { return m_info1; }
    EngineInfoWidget* info2() const { return m_info2; }
    QTableView*       view1() const { return m_view1; }
    QTableView*       view2() const { return m_view2; }

public slots:
    void setAnalysisVisible(bool on);
    void setCommentHtml(const QString& html);
    // ★ 追加: 現在の手数インデックスを設定/取得
    void setCurrentMoveIndex(int index);
    int currentMoveIndex() const { return m_currentMoveIndex; }

private slots:
    // ★ 追加: コメント編集用スロット
    void onFontIncrease();
    void onFontDecrease();
    void onUpdateCommentClicked();
    // ★ 追加: 読み筋テーブルのクリック処理
    void onView1Clicked(const QModelIndex& index);
    void onView2Clicked(const QModelIndex& index);

signals:
    // ツリー上のノード（行row, 手ply）がクリックされた
    void branchNodeActivated(int row, int ply);

    // MainWindow に「何を適用したいか」を伝える
    void requestApplyStart();                         // 開始局面へ
    void requestApplyMainAtPly(int ply);             // 本譜の ply 手へ

    // ★ 追加: コメント更新シグナル
    void commentUpdated(int moveIndex, const QString& newComment);

    // ★ 追加: 読み筋がクリックされた時のシグナル
    // engineIndex: 0=エンジン1, 1=エンジン2
    // row: クリックされた行のインデックス
    void pvRowClicked(int engineIndex, int row);

private:
    // --- 内部：ツリー描画 ---
    void rebuildBranchTree();
    QGraphicsPathItem* addNode(int row, int ply, const QString& text);
    void addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to);

    // ---- 追加：フォールバック探索とハイライト実体（実装は .cpp） ----
    int  graphFallbackToPly_(int row, int targetPly) const;
    void highlightNodeId_(int nodeId, bool centerOn);

    // --- UI ---
    QTabWidget* m_tab=nullptr;
    EngineInfoWidget *m_info1=nullptr, *m_info2=nullptr;
    QTableView *m_view1=nullptr, *m_view2=nullptr;
    QPlainTextEdit* m_usiLog=nullptr;
    QTextEdit* m_comment=nullptr;
    QGraphicsView* m_branchTree=nullptr;
    QGraphicsScene* m_scene=nullptr;

    // ★ 追加: USI通信ログ編集用UI
    QWidget* m_usiLogContainer=nullptr;
    QWidget* m_usiLogToolbar=nullptr;
    QToolButton* m_btnUsiLogFontIncrease=nullptr;
    QToolButton* m_btnUsiLogFontDecrease=nullptr;
    int m_usiLogFontSize=10;

    // ★ 追加: 思考タブフォントサイズ
    int m_thinkingFontSize=10;

    // ★ 追加: コメント編集用UI
    QWidget* m_commentToolbar=nullptr;
    QToolButton* m_btnFontIncrease=nullptr;
    QToolButton* m_btnFontDecrease=nullptr;
    QToolButton* m_btnCommentUndo=nullptr;   // undoボタン
    QToolButton* m_btnCommentRedo=nullptr;   // redoボタン
    QToolButton* m_btnCommentCut=nullptr;    // 切り取りボタン
    QToolButton* m_btnCommentCopy=nullptr;   // コピーボタン
    QToolButton* m_btnCommentPaste=nullptr;  // 貼り付けボタン
    QPushButton* m_btnUpdateComment=nullptr;
    QLabel* m_editingLabel=nullptr;  // 「修正中」ラベル
    int m_currentFontSize=10;
    int m_currentMoveIndex=-1;
    QString m_originalComment;       // 元のコメント（変更検知用）
    bool m_isCommentDirty=false;     // コメントが変更されたか

    void buildUsiLogToolbar();       // ★ 追加: USI通信ログツールバー構築
    void updateUsiLogFontSize(int delta);  // ★ 追加: USI通信ログフォントサイズ変更
    void onUsiLogFontIncrease();     // ★ 追加
    void onUsiLogFontDecrease();     // ★ 追加
    void updateThinkingFontSize(int delta);  // ★ 追加: 思考タブフォントサイズ変更
    void onThinkingFontIncrease();   // ★ 追加
    void onThinkingFontDecrease();   // ★ 追加
    void buildCommentToolbar();
    void updateCommentFontSize(int delta);
    QString convertUrlsToLinks(const QString& text);
    void updateEditingIndicator();   // 「修正中」表示の更新
    void onCommentTextChanged();     // コメント変更時の処理
    void onCommentUndo();            // コメントのundo
    void onCommentRedo();            // コメントのredo
    void onCommentCut();             // 切り取り
    void onCommentCopy();            // コピー
    void onCommentPaste();           // 貼り付け

public:
    // ★ 追加: 未保存の編集があるかチェック
    bool hasUnsavedComment() const { return m_isCommentDirty; }
    
    // ★ 追加: 未保存編集の警告ダイアログを表示（移動を続けるならtrue）
    bool confirmDiscardUnsavedComment();
    
    // ★ 追加: 編集状態をクリア（コメント更新後に呼ぶ）
    void clearCommentDirty();

    // --- モデル参照（所有しない） ---
    ShogiEngineThinkingModel* m_model1=nullptr;
    ShogiEngineThinkingModel* m_model2=nullptr;
    UsiCommLogModel* m_log1=nullptr;
    UsiCommLogModel* m_log2=nullptr;

    // --- 分岐データ ---
    QVector<ResolvedRowLite> m_rows;   // 行0=本譜、行1..=ファイル登場順の分岐

    // クリック判定用： (row,ply) -> node item（既存）
    QMap<QPair<int,int>, QGraphicsPathItem*> m_nodeIndex;

    // ==== 追加：罫線フォールバック用のグラフ ====
    struct BranchGraphNode {
        int id   = -1;
        int vid  = -1;   // 0=Main / 1..=VarN
        int row  = -1;   // ResolvedRow の行インデックス
        int ply  =  0;   // グローバル手数（1-based。初期局面は 0）
        QGraphicsPathItem* item = nullptr;
    };

    // (row,ply) -> nodeId
    QHash<QPair<int,int>, int> m_nodeIdByRowPly;
    // nodeId -> ノード
    QHash<int, BranchGraphNode> m_nodesById;
    // nodeId の前後リンク集合
    QHash<int, QVector<int>> m_prevIds;
    QHash<int, QVector<int>> m_nextIds;
    // 行ごとの入口ノード（分岐開始ノード等）
    QHash<int, int> m_rowEntryNode;
    // 連番発行
    int m_nextNodeId = 1;
    // 直前に黄色にした item
    QGraphicsPathItem* m_prevSelected = nullptr;

    // ツリークリック検出
    bool eventFilter(QObject* obj, QEvent* ev) override;

    int resolveParentRowForVariation_(int row) const;

    // すでに Q_OBJECT が付いている前提
private slots:
    void onModel1Reset_();
    void onModel2Reset_();
    void onLog1Changed_();
    void onLog2Changed_();

private:
    void reapplyViewTuning_(QTableView* v, QAbstractItemModel* m);

    // 既に導入済みのヘルパ（前回案）
private:
    void tuneColumnsForThinkingView_(QTableView* v);
    void applyNumericFormattingTo_(QTableView* view, QAbstractItemModel* model);
    static int findColumnByHeader_(QAbstractItemModel* model, const QString& title);
};

#endif // ENGINEANALYSISTAB_H
