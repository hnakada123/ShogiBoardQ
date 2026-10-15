#ifndef DIALOGLAUNCHER_H
#define DIALOGLAUNCHER_H

/// @file dialoglauncher.h
/// @brief メニューから開くダイアログの起動

#include <QObject>
#include <functional>
#include <QPointer>
#include <utility>


class QWidget;
class QDockWidget;
class DialogCoordinator;
class ShogiGameController;
class ShogiView;
class AppActivityState;
class MatchCoordinator;
class JishogiScoreDialogController;
class NyugyokuDeclarationHandler;
class CsaGameWiring;
class CsaGameDialog;
class CsaGameCoordinator;
class BoardSetupController;
class EngineAnalysisTab;
class EvaluationChartWidget;
class SfenCollectionDialog;
class PieceSoundPlayer;
class KifuFileController;

/**
 * @brief メニューから開くダイアログを、必要な部品を渡して開く
 *
 * 責務:
 * - バージョン情報、エンジン設定、成り確認、持将棋判定、入玉宣言、
 *   詰将棋探索/生成、メニューウィンドウ、CSA通信対局、
 *   棋譜解析、局面集ビューア、駒音の設定の各ダイアログ起動
 */
class DialogLauncher : public QObject
{
    Q_OBJECT

public:
    /// 依存（すべて非所有。MainWindowComposer が起動時に作って渡す）
    struct Deps {
        QWidget* parentWidget = nullptr;
        KifuFileController* kifuFileController = nullptr;  ///< 未保存の棋譜の確認
        const AppActivityState* activity = nullptr;              ///< 通信対局中か・対局中か

        DialogCoordinator* dialogCoordinator = nullptr;
        ShogiGameController* gameController = nullptr;
        MatchCoordinator* match = nullptr;
        ShogiView* shogiView = nullptr;
        JishogiScoreDialogController* jishogiController = nullptr;
        NyugyokuDeclarationHandler* nyugyokuHandler = nullptr;
        CsaGameWiring* csaGameWiring = nullptr;            ///< 通信対局の司令塔は最初の CSA 対局で作られる
        BoardSetupController* boardSetupController = nullptr;
        EngineAnalysisTab* analysisTab = nullptr;
        EvaluationChartWidget* evalChart = nullptr;
        PieceSoundPlayer* pieceSoundPlayer = nullptr;
        /// 対局者を下手・上手と呼ぶか（駒落ちで、対局情報の見出しが下手・上手のとき）
        std::function<bool()> usesHandicapNames;
        QDockWidget* menuWindowDock = nullptr;
    };

    explicit DialogLauncher(const Deps& deps, QObject* parent = nullptr);
    ~DialogLauncher() override = default;


    /// 成りダイアログで成り・不成を選ばせる（成るなら true）。ShogiGameController の成りの選び方に使う
    bool choosePromotion();

public slots:
    void displayVersionInformation();
    void openProjectWebsite();
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

#endif // DIALOGLAUNCHER_H
