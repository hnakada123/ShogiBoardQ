#ifndef DIALOGLAUNCHWIRING_H
#define DIALOGLAUNCHWIRING_H

/// @file dialoglaunchwiring.h
/// @brief ダイアログ起動配線クラスの定義

#include <QObject>
#include <QPointer>
#include <functional>
#include <utility>

#include "playmode.h"

class QWidget;
class QDockWidget;
class DialogCoordinator;
class ShogiGameController;
class ShogiView;
class MatchCoordinator;
class JishogiScoreDialogController;
class NyugyokuDeclarationHandler;
class CsaGameWiring;
class CsaGameDialog;
class CsaGameCoordinator;
class BoardSetupController;
class AnalysisResultsPresenter;
class EngineAnalysisTab;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class KifuRecordListModel;
class KifuNavigationState;
class EvaluationChartWidget;
class KifuAnalysisListModel;
class GameInfoPaneController;
class SfenCollectionDialog;
class PieceSoundPlayer;

/**
 * @brief ダイアログ起動関連のUI配線を担当するクラス
 *
 * 責務:
 * - バージョン情報、エンジン設定、成り確認、持将棋判定、入玉宣言、
 *   詰将棋探索/生成、メニューウィンドウ、CSA通信対局、
 *   棋譜解析、局面集ビューア、駒音の設定の各ダイアログ起動
 */
class DialogLaunchWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;
        std::function<bool()> confirmDiscardUnsavedKifu;
        PlayMode* playMode = nullptr;

        // 遅延初期化ゲッター (ensure + return)
        std::function<DialogCoordinator*()> getDialogCoordinator;
        std::function<ShogiGameController*()> getGameController;
        std::function<MatchCoordinator*()> getMatch;
        std::function<ShogiView*()> getShogiView;
        std::function<JishogiScoreDialogController*()> getJishogiController;
        std::function<NyugyokuDeclarationHandler*()> getNyugyokuHandler;
        std::function<CsaGameWiring*()> getCsaGameWiring;
        std::function<BoardSetupController*()> getBoardSetupController;
        std::function<GameInfoPaneController*()> getGameInfoController;
        std::function<AnalysisResultsPresenter*()> getAnalysisPresenter;
        std::function<EngineAnalysisTab*()> getAnalysisTab;
        std::function<UsiCommLogModel*()> getLineEditModel1;
        std::function<ShogiEngineThinkingModel*()> getModelThinking1;
        std::function<KifuRecordListModel*()> getKifuRecordModel;
        std::function<KifuNavigationState*()> getNavState;
        std::function<EvaluationChartWidget*()> getEvalChart;
        std::function<PieceSoundPlayer*()> getPieceSoundPlayer;
        std::function<KifuAnalysisListModel*()> getAnalysisModel;
        std::function<QDockWidget*()> getMenuWindowDock;

        // CSA通信対局の司令塔（最初の CSA 対局を始めるまでは nullptr）
        std::function<CsaGameCoordinator*()> getCsaGameCoordinator;

        // CSA通信対局の開始を試みた後（司令塔ができた後）の依存更新コールバック
        std::function<void()> onCsaGameCoordinatorUpdated;
    };

    explicit DialogLaunchWiring(Deps deps, QObject* parent = nullptr);
    ~DialogLaunchWiring() override = default;

    void updateDeps(Deps deps);

    /// 成りダイアログで成り・不成を選ばせる（成るなら true）。ShogiGameController の成りの選び方に使う
    bool choosePromotion();

public slots:
    void displayVersionInformation();
    void displayEngineSettingsDialog();
    void displayJishogiScoreDialog();
    void handleNyugyokuDeclaration();
    void displayTsumeShogiSearchDialog();
    void displayTsumeshogiGeneratorDialog();
    void displayTsumePlayDialog();
    void displayMenuWindow();
    void displayCsaGameDialog();
    void displayKifuAnalysisDialog();
    void displaySfenCollectionViewer();
    void displayPieceSoundSettingsDialog();

signals:
    void sfenCollectionPositionSelected(const QString& sfen);

private slots:
    void onCsaEngineScoreUpdatedInternal(int scoreCp, int ply, const QString& mate);

private:
    Deps m_deps;
    // Lifecycle: Created once on first open (parent=parentWidget), reused afterwards
    QPointer<CsaGameDialog> m_csaGameDialog;
    // Lifecycle: Created on open, deleted on close (WA_DeleteOnClose)
    QPointer<SfenCollectionDialog> m_sfenCollectionDialog;
};

#endif // DIALOGLAUNCHWIRING_H
