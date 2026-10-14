#ifndef BRANCHNAVIGATIONWIRING_H
#define BRANCHNAVIGATIONWIRING_H

/// @file branchnavigationwiring.h
/// @brief 分岐ナビゲーション配線クラスの定義

#include <QObject>
#include <QString>

class KifuBranchTree;
class KifuNavigationState;
class KifuNavigationController;
class KifuDisplayCoordinator;
class LiveGameSession;
class KifuRecordListModel;
class KifuBranchListModel;
class GameRecordModel;
class RecordPane;
class EngineAnalysisTab;
class CommentCoordinator;
class ShogiGameController;
class ShogiBoard;
class BranchTreeEditController;

/**
 * @brief 分岐ナビゲーション関連のUI配線を担当するクラス
 *
 * 責務:
 * - 棋譜欄のナビゲーションボタンと KifuNavigationController の接続
 * - KifuDisplayCoordinator の設定と配線
 * - 分岐ノード操作イベントの処理
 *
 * 分岐ツリー・ナビゲーション状態・コントローラ・表示コーディネータ・ライブセッションは
 * MainWindowComposer::createBranchNavigation が起動時に1回作って渡す。
 */
class BranchNavigationWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuBranchTree* branchTree = nullptr;                 ///< 分岐ツリー
        KifuNavigationState* navState = nullptr;              ///< ナビゲーション状態
        KifuNavigationController* kifuNavController = nullptr; ///< ナビゲーションコントローラ
        KifuDisplayCoordinator* displayCoordinator = nullptr; ///< 表示コーディネータ
        LiveGameSession* liveGameSession = nullptr;           ///< ライブゲームセッション
        RecordPane* recordPane = nullptr;               ///< 棋譜欄ウィジェット
        EngineAnalysisTab* analysisTab = nullptr;       ///< エンジン解析タブ
        KifuRecordListModel* kifuRecordModel = nullptr; ///< 棋譜レコードモデル
        KifuBranchListModel* kifuBranchModel = nullptr; ///< 棋譜分岐モデル
        GameRecordModel* gameRecordModel = nullptr;     ///< ゲームレコードモデル
        ShogiGameController* gameController = nullptr;  ///< ゲームコントローラ
        CommentCoordinator* commentCoordinator = nullptr; ///< コメントコーディネータ
        QString* startSfenStr = nullptr;                ///< 開始SFEN文字列
    };

    explicit BranchNavigationWiring(QObject* parent = nullptr);
    ~BranchNavigationWiring() override = default;

    void updateDeps(const Deps& deps);

    /// 分岐ナビゲーションの部品を配線する（2回目以降の呼び出しは何もしない）
    void initialize();

    /// 分岐の編集コントローラ（未初期化なら nullptr）
    BranchTreeEditController* editController() const { return m_editController; }

public slots:
    /// 分岐ツリーの構築完了を処理する（KifuBranchTreeBuilder::treeBuilt に接続）
    void onBranchTreeBuilt();

    /// 分岐ツリーのノード活性化を処理する（BranchTreeManager::branchNodeActivated に接続）
    void onBranchNodeActivated(int row, int ply);

    /// 新規対局開始時の分岐ツリーリセット
    void onBranchTreeResetForNewGame();

signals:
    /// 棋譜欄の選択シグナルを抑止した移動でも、グラフの現在手数を同期する
    void currentPlyChanged(int ply);
    /// 盤面とハイライトの更新が必要（→ MainWindow::loadBoardWithHighlights へ接続）
    void boardWithHighlightsRequired(const QString& currentSfen, const QString& prevSfen);
    /// SFEN文字列から盤面更新が必要（→ MainWindow::loadBoardFromSfen へ接続）
    void boardSfenChanged(const QString& sfen);
    /// 分岐ノード処理完了（→ MainWindow::onBranchNodeHandled へ接続）
    void branchNodeHandled(int ply, const QString& sfen,
                           int previousFileTo, int previousRankTo,
                           const QString& lastUsiMove);

private:
    /// 棋譜欄のナビゲーションボタンをコントローラへ接続する
    void connectNavigationButtons();
    /// 分岐表示コーディネーターを設定する
    void configureDisplayCoordinator();
    /// 分岐表示コーディネーターのシグナルを接続する
    void connectDisplaySignals();
    /// 分岐ツリーの右クリックメニューによる編集を配線する
    void wireBranchEditing();

    Deps m_deps;
    BranchTreeEditController* m_editController = nullptr;   ///< 分岐の編集（this が所有）
    bool m_initialized = false;                             ///< initialize() を済ませたか
};

#endif // BRANCHNAVIGATIONWIRING_H
