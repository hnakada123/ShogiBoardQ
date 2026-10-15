#ifndef ENGINEANALYSISTAB_H
#define ENGINEANALYSISTAB_H

/// @file engineanalysistab.h
/// @brief エンジン解析タブクラスの定義

#include <QWidget>
#include <QList>
#include <QPointer>

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
class QCheckBox;
class QLabel;

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

    // 表示する棋譜ツリーを設定（row=0:本譜、row=1..:分岐）（BranchTreeManager へ委譲）
    void setBranchTree(KifuBranchTree* tree);

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
    void setCsaConnected(bool connected);
    // USI通信ログクリア
    void clearUsiLog();

    // USI通信ログにステータスメッセージを追記
    void appendUsiLogStatus(const QString& message);

    // 追加（HvE/EvE の配線で使う）
    EngineInfoWidget* info1() const { return m_info1; }
    EngineInfoWidget* info2() const { return m_info2; }
    // --- サブコンポーネントアクセサ ---
    BranchTreeManager* branchTreeManager();
    CommentEditorPanel* commentEditor();
    ConsiderationTabManager* considerationTabManager();

    // 検討タブに切り替える
    void switchToConsiderationTab();

    // 思考タブに切り替える
    void switchToThinkingTab();

    // 思考・検討の表の選択を外す（読み筋ダイアログを閉じたとき）
    void clearThinkingViewSelection(int engineIndex);

    // 検討開始ボタンの有効/無効（対局状態ポリシーから使用）
    void setConsiderationStartEnabled(bool enabled);

    // 未保存の編集があるかチェック（CommentEditorPanel へ委譲）
    bool hasUnsavedComment() const;

    // 未保存編集の警告ダイアログを表示（CommentEditorPanel へ委譲）
    bool confirmDiscardUnsavedComment();

    // 編集状態をクリア（CommentEditorPanel へ委譲）
    void clearCommentDirty();

    // 分岐ツリーのクリック有効/無効を設定（対局中は無効にする）（BranchTreeManager へ委譲）
    void setBranchTreeClickEnabled(bool enabled);
    bool isBranchTreeClickEnabled() const;

    // 現在の手数インデックスを取得
    int currentMoveIndex() const;

public slots:
    void setAnalysisVisible(bool on);
    /// 候補手の矢印の表示状態（検討中は対局の矢印の設定を使わない。先読みの説明を添える）
    void setArrowDisplayState(bool considerationActive, const QString& ponderDescription);
    void setCommentHtml(const QString& html);
    // 現在の手数インデックスを設定
    void setCurrentMoveIndex(int index);

signals:
    /// 対局の矢印・先読みの矢印の表示設定が変わった（矢印を描き直す）
    void arrowSettingsChanged();

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

    bool m_arrowConsideration = false;   ///< 検討中か（検討の矢印は検討タブで設定する）
    QString m_ponderDescription;         ///< 先読みの矢印の説明
    QCheckBox* m_matchArrows = nullptr;
    QCheckBox* m_ponderArrows = nullptr;
    QLabel* m_arrowDescription = nullptr;

private slots:
    void onMatchArrowsToggled(bool checked);
    void onPonderArrowsToggled(bool checked);
    void updateArrowControls();

private:
    // 思考ページ構築ヘルパ
    QWidget* buildThinkingPageContent(QWidget* parent);
};

#endif // ENGINEANALYSISTAB_H
