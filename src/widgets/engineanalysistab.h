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
#include <QColor>

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
class QLineEdit;      // ★ 追加
class QComboBox;      // ★ 追加: USIコマンド送信先選択用
class QSpinBox;       // ★ 追加: 候補手の数用
class QTimer;         // ★ 追加: 経過時間用
class QRadioButton;   // ★ 追加: 思考時間設定用

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

    // ★ 追加: CSA通信ログ追記
    void appendCsaLog(const QString& line);
    // ★ 追加: CSA通信ログクリア
    void clearCsaLog();

    // ★ 追加: USI通信ログにステータスメッセージを追記
    void appendUsiLogStatus(const QString& message);

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

    // ★ 追加: 検討タブ用アクセサ
    EngineInfoWidget* considerationInfo() const { return m_considerationInfo; }
    QTableView*       considerationView() const { return m_considerationView; }

    // ★ 追加: 検討タブ用モデル設定
    void setConsiderationThinkingModel(ShogiEngineThinkingModel* m);

    // ★ 追加: 検討タブに切り替える
    void switchToConsiderationTab();

    // ★ 追加: 思考タブに切り替える
    void switchToThinkingTab();

    // ★ 追加: 検討タブの候補手の数を取得
    int considerationMultiPV() const;

    // ★ 追加: 検討タブの候補手の数を設定
    void setConsiderationMultiPV(int value);
    void clearThinkingViewSelection(int engineIndex);

    // ★ 追加: 検討タブの時間設定を表示
    void setConsiderationTimeLimit(bool unlimited, int byoyomiSec);

    // ★ 追加: 検討タブのエンジン名を設定
    void setConsiderationEngineName(const QString& name);

    // ★ 追加: 検討タブの経過時間タイマー制御
    void startElapsedTimer();
    void stopElapsedTimer();
    void resetElapsedTimer();

    // ★ 追加: 検討実行状態の設定（ボタン表示切替用）
    void setConsiderationRunning(bool running);

    // ★ 追加: エンジンリストを読み込み
    void loadEngineList();

    // ★ 追加: エンジン選択を取得
    int selectedEngineIndex() const;
    QString selectedEngineName() const;

    // ★ 追加: 時間設定を取得
    bool isUnlimitedTime() const;
    int byoyomiSec() const;

    // ★ 追加: 検討タブの設定を復元
    void loadConsiderationTabSettings();

    // ★ 追加: 検討タブの設定を保存
    void saveConsiderationTabSettings();

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
    void onConsiderationViewClicked(const QModelIndex& index);

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

    // ★ 追加: CSAコマンド送信シグナル
    void csaRawCommandRequested(const QString& command);

    // ★ 追加: USIコマンド送信シグナル
    // target: 0=E1, 1=E2, 2=両方
    void usiCommandRequested(int target, const QString& command);

    // ★ 追加: 検討タブの候補手の数変更シグナル
    void considerationMultiPVChanged(int value);

    // ★ 追加: 検討中止シグナル
    void stopConsiderationRequested();

    // ★ 追加: 検討開始シグナル
    void startConsiderationRequested();

    // ★ 追加: エンジン設定ボタンが押されたシグナル
    void engineSettingsRequested(int engineNumber, const QString& engineName);

    // ★ 追加: 思考時間設定が変更されたシグナル
    void considerationTimeSettingsChanged(bool unlimited, int byoyomiSec);

private:
    // ★ 追加: 分岐ツリーのクリック有効フラグ（対局中は無効にする）
    bool m_branchTreeClickEnabled = true;

    // --- 内部：ツリー描画 ---
    void rebuildBranchTree();
    QGraphicsPathItem* addNode(int row, int ply, const QString& text);
    void addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to);

    // ---- 追加：フォールバック探索とハイライト実体（実装は .cpp） ----
    int  graphFallbackToPly(int row, int targetPly) const;
    void highlightNodeId(int nodeId, bool centerOn);

    // --- UI ---
    QTabWidget* m_tab=nullptr;
    EngineInfoWidget *m_info1=nullptr, *m_info2=nullptr;
    QTableView *m_view1=nullptr, *m_view2=nullptr;
    QPlainTextEdit* m_usiLog=nullptr;
    QTextEdit* m_comment=nullptr;
    QGraphicsView* m_branchTree=nullptr;
    QGraphicsScene* m_scene=nullptr;

    // ★ 追加: 検討タブ用UI
    EngineInfoWidget* m_considerationInfo=nullptr;
    QTableView* m_considerationView=nullptr;
    ShogiEngineThinkingModel* m_considerationModel=nullptr;
    int m_considerationTabIndex=-1;  // 検討タブのインデックス
    QWidget* m_considerationToolbar=nullptr;  // 検討タブ用ツールバー
    QToolButton* m_btnConsiderationFontDecrease=nullptr;  // 検討タブフォントサイズ減少ボタン
    QToolButton* m_btnConsiderationFontIncrease=nullptr;  // 検討タブフォントサイズ増加ボタン
    QComboBox* m_engineComboBox=nullptr;      // エンジン選択コンボボックス
    QPushButton* m_btnEngineSettings=nullptr; // エンジン設定ボタン
    QRadioButton* m_unlimitedTimeRadioButton=nullptr;      // 時間無制限ラジオボタン
    QRadioButton* m_considerationTimeRadioButton=nullptr;  // 検討時間ラジオボタン
    QSpinBox* m_byoyomiSecSpinBox=nullptr;    // 検討時間スピンボックス
    QLabel* m_byoyomiSecUnitLabel=nullptr;    // 「秒まで」ラベル
    QLabel* m_elapsedTimeLabel=nullptr;       // 経過時間ラベル
    QTimer* m_elapsedTimer=nullptr;           // 経過時間更新タイマー
    int m_elapsedSeconds=0;                   // 経過秒数
    int m_considerationTimeLimitSec=0;        // 検討時間制限（秒、0=無制限）
    QLabel* m_multiPVLabel=nullptr;           // 「候補手の数」ラベル
    QComboBox* m_multiPVComboBox=nullptr;     // 候補手の数コンボボックス
    QToolButton* m_btnStopConsideration=nullptr;  // 検討中止ボタン
    int m_considerationFontSize=10;           // 検討タブフォントサイズ

    // ★ 追加: USI通信ログ編集用UI
    QWidget* m_usiLogContainer=nullptr;
    QWidget* m_usiLogToolbar=nullptr;
    QLabel* m_usiLogEngine1Label=nullptr;   // ★ 追加: エンジン1名ラベル
    QLabel* m_usiLogEngine2Label=nullptr;   // ★ 追加: エンジン2名ラベル
    QToolButton* m_btnUsiLogFontIncrease=nullptr;
    QToolButton* m_btnUsiLogFontDecrease=nullptr;
    int m_usiLogFontSize=10;

    // ★ 追加: USI通信ログコマンド入力UI
    QWidget* m_usiCommandBar=nullptr;
    QComboBox* m_usiTargetCombo=nullptr;
    QLineEdit* m_usiCommandInput=nullptr;

    // ★ 追加: CSA通信ログ用UI
    QWidget* m_csaLogContainer=nullptr;
    QWidget* m_csaLogToolbar=nullptr;
    QPlainTextEdit* m_csaLog=nullptr;
    QToolButton* m_btnCsaLogFontIncrease=nullptr;
    QToolButton* m_btnCsaLogFontDecrease=nullptr;
    int m_csaLogFontSize=10;

    // ★ 追加: CSA通信ログコマンド入力UI
    QWidget* m_csaCommandBar=nullptr;
    QPushButton* m_btnCsaSendToServer=nullptr;
    QLineEdit* m_csaCommandInput=nullptr;

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
    void appendColoredUsiLog(const QString& logLine, const QColor& lineColor);  // ★ 追加: 色付きログ追加
    void buildUsiCommandBar();       // ★ 追加: USIコマンドバー構築
    void onUsiCommandEntered();      // ★ 追加: USIコマンド入力処理
    void onEngine1NameChanged();     // ★ 追加: エンジン1名変更時
    void onEngine2NameChanged();     // ★ 追加: エンジン2名変更時
    void buildCsaLogToolbar();       // ★ 追加: CSA通信ログツールバー構築
    void updateCsaLogFontSize(int delta);  // ★ 追加: CSA通信ログフォントサイズ変更
    void onCsaLogFontIncrease();     // ★ 追加
    void onCsaLogFontDecrease();     // ★ 追加
    void buildCsaCommandBar();       // ★ 追加: CSAコマンド入力バー構築
    void onCsaCommandEntered();      // ★ 追加: CSAコマンド入力処理
    void updateThinkingFontSize(int delta);  // ★ 追加: 思考タブフォントサイズ変更
    void onThinkingFontIncrease();   // ★ 追加
    void onThinkingFontDecrease();   // ★ 追加
    void updateConsiderationFontSize(int delta);  // ★ 追加: 検討タブフォントサイズ変更
    void onConsiderationFontIncrease();  // ★ 追加
    void onConsiderationFontDecrease();  // ★ 追加
    void onMultiPVComboBoxChanged(int index);  // ★ 追加: コンボボックス値変更
    void onElapsedTimerTick();           // ★ 追加: 経過時間更新
    void onEngineSettingsClicked();      // ★ 追加: エンジン設定ボタンクリック
    void onTimeSettingChanged();         // ★ 追加: 時間設定変更
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
    void onEngineInfoColumnWidthChanged();  // ★ 追加: 列幅変更時の保存
    void onThinkingViewColumnWidthChanged(int viewIndex);  // ★ 追加: 思考タブ下段の列幅変更時の保存

public:
    // ★ 追加: 未保存の編集があるかチェック
    bool hasUnsavedComment() const { return m_isCommentDirty; }

    // ★ 追加: 未保存編集の警告ダイアログを表示（移動を続けるならtrue）
    bool confirmDiscardUnsavedComment();

    // ★ 追加: 編集状態をクリア（コメント更新後に呼ぶ）
    void clearCommentDirty();

    // ★ 追加: 分岐ツリーのクリック有効/無効を設定（対局中は無効にする）
    void setBranchTreeClickEnabled(bool enabled) { m_branchTreeClickEnabled = enabled; }
    bool isBranchTreeClickEnabled() const { return m_branchTreeClickEnabled; }

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

    // ★ 追加: 思考タブ下段の列幅が設定から読み込まれたか
    bool m_thinkingView1WidthsLoaded = false;
    bool m_thinkingView2WidthsLoaded = false;

    // ツリークリック検出
    bool eventFilter(QObject* obj, QEvent* ev) override;

    int resolveParentRowForVariation(int row) const;

    // すでに Q_OBJECT が付いている前提
private slots:
    void onModel1Reset();
    void onModel2Reset();
    void onLog1Changed();
    void onLog2Changed();
    void onView1SectionResized(int logicalIndex, int oldSize, int newSize);  // ★ 追加
    void onView2SectionResized(int logicalIndex, int oldSize, int newSize);  // ★ 追加

private:
    void reapplyViewTuning(QTableView* v, QAbstractItemModel* m);

    // 既に導入済みのヘルパ（前回案）
private:
    void setupThinkingViewHeader(QTableView* v);  // ★ 変更: ヘッダ基本設定
    void applyThinkingViewColumnWidths(QTableView* v, int viewIndex);  // ★ 追加: 列幅適用
    void applyNumericFormattingTo(QTableView* view, QAbstractItemModel* model);
    static int findColumnByHeader(QAbstractItemModel* model, const QString& title);
};

#endif // ENGINEANALYSISTAB_H
