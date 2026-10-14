#ifndef DIALOGLAUNCHWIRING_H
#define DIALOGLAUNCHWIRING_H

/// @file dialoglaunchwiring.h
/// @brief ダイアログ起動配線クラスの定義

#include <QObject>
#include <QPointer>
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
class KifuFileController;

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
    /// 依存（すべて非所有。MainWindowComposer が起動時に作って渡す）
    struct Deps {
        QWidget* parentWidget = nullptr;
        KifuFileController* kifuFileController = nullptr;  ///< 未保存の棋譜の確認
        PlayMode* playMode = nullptr;                      ///< AppState::playMode

        DialogCoordinator* dialogCoordinator = nullptr;
        ShogiGameController* gameController = nullptr;
        MatchCoordinator* match = nullptr;
        ShogiView* shogiView = nullptr;
        JishogiScoreDialogController* jishogiController = nullptr;
        NyugyokuDeclarationHandler* nyugyokuHandler = nullptr;
        CsaGameWiring* csaGameWiring = nullptr;            ///< 通信対局の司令塔は最初の CSA 対局で作られる
        BoardSetupController* boardSetupController = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        AnalysisResultsPresenter* analysisPresenter = nullptr;
        EngineAnalysisTab* analysisTab = nullptr;
        UsiCommLogModel* lineEditModel1 = nullptr;
        ShogiEngineThinkingModel* modelThinking1 = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        KifuNavigationState* navState = nullptr;
        EvaluationChartWidget* evalChart = nullptr;
        PieceSoundPlayer* pieceSoundPlayer = nullptr;
        KifuAnalysisListModel* analysisModel = nullptr;
        QDockWidget* menuWindowDock = nullptr;
    };

    explicit DialogLaunchWiring(const Deps& deps, QObject* parent = nullptr);
    ~DialogLaunchWiring() override = default;


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
