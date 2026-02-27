#ifndef ENGINEANALYSISTAB_H
#define ENGINEANALYSISTAB_H

/// @file engineanalysistab.h
/// @brief エンジン解析タブクラスの定義


#include <QWidget>
#include <QVector>

#include "branchtreemanager.h"

class QTabWidget;
class QTableView;

class EngineInfoWidget;
class ShogiEngineThinkingModel;
class UsiCommLogModel;
class CommentEditorPanel;
class ConsiderationTabManager;
class UsiLogPanel;
class CsaLogPanel;
class EngineAnalysisPresenter;

class EngineAnalysisTab : public QWidget
{
    Q_OBJECT

public:
    explicit EngineAnalysisTab(QWidget* parent=nullptr);
    ~EngineAnalysisTab() override;

    void buildUi();

    // 独立したドック用にページを作成
    // 各メソッドは独立したQWidgetを作成して返す（呼び出し側が親を設定）
    QWidget* createThinkingPage(QWidget* parent);
    QWidget* createConsiderationPage(QWidget* parent);
    QWidget* createUsiLogPage(QWidget* parent);
    QWidget* createCsaLogPage(QWidget* parent);
    QWidget* createCommentPage(QWidget* parent);
    QWidget* createBranchTreePage(QWidget* parent);

    void setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                   UsiCommLogModel* log1, UsiCommLogModel* log2);

    // 既存コードの m_tab をそのまま使えるように（ドックモードでは使用しない）
    QTabWidget* tab() const;

    // --- 分岐ツリー：BranchTreeManager への型エイリアス ---
    using ResolvedRowLite = BranchTreeManager::ResolvedRowLite;

    // ツリーの全行データをセット（row=0:本譜、row=1..:分岐）（BranchTreeManager へ委譲）
    void setBranchTreeRows(const QVector<ResolvedRowLite>& rows);

    // ツリー上で (row, ply) をハイライト（必要なら centerOn）（BranchTreeManager へ委譲）
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

    // CSA通信ログ追記
    void appendCsaLog(const QString& line);
    // CSA通信ログクリア
    void clearCsaLog();
    // USI通信ログクリア
    void clearUsiLog();

    // USI通信ログにステータスメッセージを追記
    void appendUsiLogStatus(const QString& message);

    // 追加（HvE/EvE の配線で使う）
    EngineInfoWidget* info1() const { return m_info1; }
    EngineInfoWidget* info2() const { return m_info2; }
    QTableView*       view1() const { return m_view1; }
    QTableView*       view2() const { return m_view2; }

    // --- サブコンポーネントアクセサ ---
    BranchTreeManager* branchTreeManager();
    CommentEditorPanel* commentEditor();
    ConsiderationTabManager* considerationTabManager();

    // 検討タブ用アクセサ（ConsiderationTabManager へ委譲）
    EngineInfoWidget* considerationInfo() const;
    QTableView*       considerationView() const;

    // 検討タブ用モデル設定（ConsiderationTabManager へ委譲）
    void setConsiderationThinkingModel(ShogiEngineThinkingModel* m);

    // 検討タブに切り替える
    void switchToConsiderationTab();

    // 思考タブに切り替える
    void switchToThinkingTab();

    // 検討タブの候補手の数を取得/設定（ConsiderationTabManager へ委譲）
    int considerationMultiPV() const;
    void setConsiderationMultiPV(int value);
    void clearThinkingViewSelection(int engineIndex);

    // 検討タブの時間設定を表示（ConsiderationTabManager へ委譲）
    void setConsiderationTimeLimit(bool unlimited, int byoyomiSec);

    // 検討タブのエンジン名を設定（ConsiderationTabManager へ委譲）
    void setConsiderationEngineName(const QString& name);

    // 検討タブの経過時間タイマー制御（ConsiderationTabManager へ委譲）
    void startElapsedTimer();
    void stopElapsedTimer();
    void resetElapsedTimer();

    // 検討実行状態の設定（ボタン表示切替用）（ConsiderationTabManager へ委譲）
    void setConsiderationRunning(bool running);

    // エンジンリストを読み込み（ConsiderationTabManager へ委譲）
    void loadEngineList();

    // エンジン選択を取得（ConsiderationTabManager へ委譲）
    int selectedEngineIndex() const;
    QString selectedEngineName() const;

    // 時間設定を取得（ConsiderationTabManager へ委譲）
    bool isUnlimitedTime() const;
    int byoyomiSec() const;

    // 検討タブの設定を復元/保存（ConsiderationTabManager へ委譲）
    void loadConsiderationTabSettings();
    void saveConsiderationTabSettings();

    // 矢印表示チェックボックスの状態を取得（ConsiderationTabManager へ委譲）
    bool isShowArrowsChecked() const;

    // 未保存の編集があるかチェック（CommentEditorPanel へ委譲）
    bool hasUnsavedComment() const;

    // 未保存編集の警告ダイアログを表示（CommentEditorPanel へ委譲）
    bool confirmDiscardUnsavedComment();

    // 編集状態をクリア（CommentEditorPanel へ委譲）
    void clearCommentDirty();

    // 分岐ツリーのクリック有効/無効を設定（対局中は無効にする）（BranchTreeManager へ委譲）
    void setBranchTreeClickEnabled(bool enabled);
    bool isBranchTreeClickEnabled() const;

public slots:
    void setAnalysisVisible(bool on);
    void setCommentHtml(const QString& html);
    // 現在の手数インデックスを設定/取得
    void setCurrentMoveIndex(int index);
    int currentMoveIndex() const;

signals:
    // ツリー上のノード（行row, 手ply）がクリックされた
    void branchNodeActivated(int row, int ply);

    // MainWindow に「何を適用したいか」を伝える
    void requestApplyStart();                         // 開始局面へ
    void requestApplyMainAtPly(int ply);             // 本譜の ply 手へ

    // コメント更新シグナル
    void commentUpdated(int moveIndex, const QString& newComment);

    // 読み筋がクリックされた時のシグナル
    // engineIndex: 0=エンジン1, 1=エンジン2, 2=検討タブ
    // row: クリックされた行のインデックス
    void pvRowClicked(int engineIndex, int row);

    // CSAコマンド送信シグナル
    void csaRawCommandRequested(const QString& command);

    // USIコマンド送信シグナル
    // target: 0=E1, 1=E2, 2=両方
    void usiCommandRequested(int target, const QString& command);

    // 検討タブの候補手の数変更シグナル
    void considerationMultiPVChanged(int value);

    // 検討中止シグナル
    void stopConsiderationRequested();

    // 検討開始シグナル
    void startConsiderationRequested();

    // エンジン設定ボタンが押されたシグナル
    void engineSettingsRequested(int engineNumber, const QString& engineName);

    // 思考時間設定が変更されたシグナル
    void considerationTimeSettingsChanged(bool unlimited, int byoyomiSec);

    // 矢印表示設定が変更されたシグナル
    void showArrowsChanged(bool checked);

    // 検討中にエンジン選択が変更されたシグナル
    void considerationEngineChanged(int index, const QString& name);

private:
    // --- UI ---
    QTabWidget* m_tab = nullptr;
    EngineInfoWidget* m_info1 = nullptr;
    EngineInfoWidget* m_info2 = nullptr;
    QTableView* m_view1 = nullptr;
    QTableView* m_view2 = nullptr;

    // コメント編集パネル（CommentEditorPanel に委譲）
    CommentEditorPanel* m_commentEditor = nullptr;
    void ensureCommentEditor();

    // 分岐ツリー管理（BranchTreeManager に委譲）
    BranchTreeManager* m_branchTreeManager = nullptr;
    void ensureBranchTreeManager();

    // 検討タブ管理（ConsiderationTabManager に委譲）
    ConsiderationTabManager* m_considerationTabManager = nullptr;
    int m_considerationTabIndex = -1;
    void ensureConsiderationTabManager();

    // USI通信ログパネル（UsiLogPanel に委譲）
    UsiLogPanel* m_usiLogPanel = nullptr;
    void ensureUsiLogPanel();

    // CSA通信ログパネル（CsaLogPanel に委譲）
    CsaLogPanel* m_csaLogPanel = nullptr;
    void ensureCsaLogPanel();

    // 思考ビュープレゼンタ（EngineAnalysisPresenter に委譲）
    EngineAnalysisPresenter* m_presenter = nullptr;
    void ensurePresenter();

    // 思考ページ構築ヘルパ
    QWidget* buildThinkingPageContent(QWidget* parent);
};

#endif // ENGINEANALYSISTAB_H
