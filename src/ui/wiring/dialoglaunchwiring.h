#ifndef DIALOGLAUNCHWIRING_H
#define DIALOGLAUNCHWIRING_H

/// @file dialoglaunchwiring.h
/// @brief ダイアログ起動配線クラスの定義

#include <QObject>
#include <QPointer>
#include <functional>

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
class PlayerInfoWiring;
class AnalysisResultsPresenter;
class Usi;
class EngineAnalysisTab;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class KifuRecordListModel;
class KifuLoadCoordinator;
class EvaluationChartWidget;
class KifuAnalysisListModel;
class GameInfoPaneController;
class SfenCollectionDialog;
class KifuDisplay;

/**
 * @brief ダイアログ起動関連のUI配線を担当するクラス
 *
 * 責務:
 * - バージョン情報、エンジン設定、成り確認、持将棋判定、入玉宣言、
 *   詰将棋探索/生成、メニューウィンドウ、CSA通信対局、
 *   棋譜解析、局面集ビューアの各ダイアログ起動
 */
class DialogLaunchWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;
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
        std::function<PlayerInfoWiring*()> getPlayerInfoWiring;
        std::function<AnalysisResultsPresenter*()> getAnalysisPresenter;
        std::function<Usi*()> getUsi1;
        std::function<EngineAnalysisTab*()> getAnalysisTab;
        std::function<UsiCommLogModel*()> getLineEditModel1;
        std::function<ShogiEngineThinkingModel*()> getModelThinking1;
        std::function<KifuRecordListModel*()> getKifuRecordModel;
        std::function<KifuLoadCoordinator*()> getKifuLoadCoordinator;
        std::function<EvaluationChartWidget*()> getEvalChart;
        std::function<QStringList*()> getSfenRecord;

        // 値型メンバーへのポインタ
        QList<KifuDisplay*>* moveRecords = nullptr;
        QStringList* gameUsiMoves = nullptr;
        int* activePly = nullptr;

        // メソッドが生成/代入するオブジェクト (ダブルポインタ)
        CsaGameDialog** csaGameDialog = nullptr;
        CsaGameCoordinator** csaGameCoordinator = nullptr;
        GameInfoPaneController** gameInfoController = nullptr;
        KifuAnalysisListModel** analysisModel = nullptr;

        // メニューウィンドウドック
        QDockWidget** menuWindowDock = nullptr;
        std::function<void()> createMenuWindowDock;

        // 局面集ダイアログ
        QPointer<SfenCollectionDialog>* sfenCollectionDialog = nullptr;

        // CSA通信対局のエンジン評価値グラフ用
        std::function<CsaGameCoordinator*()> getCsaGameCoordinator;
    };

    explicit DialogLaunchWiring(const Deps& deps, QObject* parent = nullptr);
    ~DialogLaunchWiring() override = default;

    void updateDeps(const Deps& deps);

public slots:
    void displayVersionInformation();
    void displayEngineSettingsDialog();
    void displayPromotionDialog();
    void displayJishogiScoreDialog();
    void handleNyugyokuDeclaration();
    void displayTsumeShogiSearchDialog();
    void displayTsumeshogiGeneratorDialog();
    void displayMenuWindow();
    void displayCsaGameDialog();
    void displayKifuAnalysisDialog();
    void displaySfenCollectionViewer();

signals:
    void sfenCollectionPositionSelected(const QString& sfen);

private slots:
    void onCsaEngineScoreUpdatedInternal(int scoreCp, int ply);

private:
    Deps m_deps;
};

#endif // DIALOGLAUNCHWIRING_H
