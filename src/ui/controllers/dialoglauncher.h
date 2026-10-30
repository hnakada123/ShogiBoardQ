#ifndef DIALOGLAUNCHER_H
#define DIALOGLAUNCHER_H

/// @file dialoglauncher.h
/// @brief メニューから開くダイアログの起動

#include <QObject>
#include <QPointer>
#include <utility>


class QWidget;
class QDockWidget;
class ShogiView;
class BoardAppearanceDialog;
class CsaGameDialog;
class CsaGameSession;
class SfenCollectionDialog;
class PieceSoundPlayer;
class KifuFileController;
class EvaluationChartWidget;

/**
 * @brief メニューから開くダイアログを、必要な部品を渡して開く
 *
 * 責務:
 * - バージョン情報、エンジン設定、成り確認、持将棋判定、
 *   詰将棋の対局・生成、メニューウィンドウ、CSA通信対局、
 *   局面集ビューア、駒音・盤の外観・文字の大きさ・評価値グラフの表示の設定の各ダイアログ起動
 */
class DialogLauncher : public QObject
{
    Q_OBJECT

public:
    /// 依存（すべて非所有。MainWindowComposer が起動時に作って渡す）
    struct Deps {
        QWidget* parentWidget = nullptr;
        KifuFileController* kifuFileController = nullptr;  ///< 未保存の棋譜の確認

        ShogiView* shogiView = nullptr;
        CsaGameSession* csaSession = nullptr;              ///< 通信対局を始める
        PieceSoundPlayer* pieceSoundPlayer = nullptr;
        QDockWidget* menuWindowDock = nullptr;
        EvaluationChartWidget* evalChart = nullptr;        ///< 表示設定を変える評価値グラフ
    };

    explicit DialogLauncher(const Deps& deps, QObject* parent = nullptr);
    ~DialogLauncher() override = default;


    /// 成りダイアログで成り・不成を選ばせる（成るなら true）。盤の操作（BoardInteractionController）の成りの選び方に使う
    bool choosePromotion();

public slots:
    void displayVersionInformation();
    void openProjectWebsite();
    void displayEngineSettingsDialog();
    void displayJishogiScoreDialog();
    void displayTsumeshogiGeneratorDialog();
    void displayTsumePlayDialog();
    void displayMenuWindow();
    void displayCsaGameDialog();
    void displaySfenCollectionViewer();
    void displayPieceSoundSettingsDialog();
    void displayBoardAppearanceDialog();
    void displayFontSettingsDialog();
    /// 評価値グラフの「表示設定…」（決めた設定をグラフに渡す）
    void displayEvaluationChartSettings();

signals:
    void sfenCollectionPositionSelected(const QString& sfen);
    /// エンジン登録ダイアログを閉じた（登録エンジンの一覧が変わったかもしれない）
    void engineListChanged();

private:
    Deps m_deps;
    // Lifecycle: Created once on first open (parent=parentWidget), reused afterwards
    QPointer<CsaGameDialog> m_csaGameDialog;
    // Lifecycle: Created on open, deleted on close (WA_DeleteOnClose)
    QPointer<SfenCollectionDialog> m_sfenCollectionDialog;
    // 盤の外観の設定は開いたまま盤を見比べられるようにモードレスで1つだけ開く（閉じたら破棄）
    QPointer<BoardAppearanceDialog> m_appearanceDialog;
};

#endif // DIALOGLAUNCHER_H
