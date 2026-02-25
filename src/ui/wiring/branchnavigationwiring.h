#ifndef BRANCHNAVIGATIONWIRING_H
#define BRANCHNAVIGATIONWIRING_H

/// @file branchnavigationwiring.h
/// @brief 分岐ナビゲーション配線クラスの定義

#include <QObject>
#include <QString>
#include <functional>

class KifuBranchTree;
class KifuNavigationState;
class KifuNavigationController;
class KifuDisplayCoordinator;
class BranchTreeWidget;
class LiveGameSession;
class KifuRecordListModel;
class KifuBranchListModel;
class GameRecordModel;
class RecordPane;
class EngineAnalysisTab;
class CommentCoordinator;
class ShogiGameController;
class ShogiBoard;

/**
 * @brief 分岐ナビゲーション関連のUI配線を担当するクラス
 *
 * 責務:
 * - 分岐ツリー/ナビゲーション状態/コントローラの生成
 * - KifuDisplayCoordinatorの生成と配線
 * - 分岐ノード操作イベントの処理
 */
class BranchNavigationWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuBranchTree** branchTree = nullptr;                ///< 分岐ツリー（ポインタのポインタ）
        KifuNavigationState** navState = nullptr;             ///< ナビゲーション状態
        KifuNavigationController** kifuNavController = nullptr; ///< ナビゲーションコントローラ
        KifuDisplayCoordinator** displayCoordinator = nullptr; ///< 表示コーディネータ
        BranchTreeWidget** branchTreeWidget = nullptr;        ///< 分岐ツリーウィジェット
        LiveGameSession** liveGameSession = nullptr;          ///< ライブゲームセッション
        RecordPane* recordPane = nullptr;               ///< 棋譜欄ウィジェット
        EngineAnalysisTab* analysisTab = nullptr;       ///< エンジン解析タブ
        KifuRecordListModel* kifuRecordModel = nullptr; ///< 棋譜レコードモデル
        KifuBranchListModel* kifuBranchModel = nullptr; ///< 棋譜分岐モデル
        GameRecordModel* gameRecordModel = nullptr;     ///< ゲームレコードモデル
        ShogiGameController* gameController = nullptr;  ///< ゲームコントローラ
        CommentCoordinator* commentCoordinator = nullptr; ///< コメントコーディネータ
        QString* startSfenStr = nullptr;                ///< 開始SFEN文字列
        std::function<void()> ensureCommentCoordinator; ///< CommentCoordinator遅延初期化コールバック
    };

    explicit BranchNavigationWiring(QObject* parent = nullptr);
    ~BranchNavigationWiring() override = default;

    void updateDeps(const Deps& deps);

    /// 分岐ナビゲーション関連クラスを初期化する（モデル生成 + シグナル配線）
    void initialize();

public slots:
    /// 分岐ツリーの構築完了を処理する（KifuBranchTreeBuilder::treeBuilt に接続）
    void onBranchTreeBuilt();

    /// 分岐ツリーのノード活性化を処理する（BranchTreeWidget::nodeActivated に接続）
    void onBranchNodeActivated(int row, int ply);

    /// 新規対局開始時の分岐ツリーリセット
    void onBranchTreeResetForNewGame();

signals:
    /// 盤面とハイライトの更新が必要（→ MainWindow::loadBoardWithHighlights へ接続）
    void boardWithHighlightsRequired(const QString& currentSfen, const QString& prevSfen);
    /// SFEN文字列から盤面更新が必要（→ MainWindow::loadBoardFromSfen へ接続）
    void boardSfenChanged(const QString& sfen);
    /// 分岐ノード処理完了（→ MainWindow::onBranchNodeHandled へ接続）
    void branchNodeHandled(int ply, const QString& sfen,
                           int previousFileTo, int previousRankTo,
                           const QString& lastUsiMove);

private:
    /// 分岐ナビゲーションのモデルを生成する
    void createModels();
    /// 分岐ナビゲーションのシグナルを接続する
    void wireSignals();
    /// 分岐表示コーディネーターを設定する
    void configureDisplayCoordinator();
    /// 分岐表示コーディネーターのシグナルを接続する
    void connectDisplaySignals();

    Deps m_deps;
};

#endif // BRANCHNAVIGATIONWIRING_H
