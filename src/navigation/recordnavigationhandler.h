#ifndef RECORDNAVIGATIONHANDLER_H
#define RECORDNAVIGATIONHANDLER_H

/// @file recordnavigationhandler.h
/// @brief 棋譜欄行変更ハンドラクラスの定義


#include <QObject>
#include <QVector>
#include <QStringList>

#include "playmode.h"
#include "shogigamecontroller.h"

class KifuNavigationState;
class KifuBranchTree;
class KifuDisplayCoordinator;
class KifuRecordListModel;
class ShogiView;
class MatchCoordinator;
class EvaluationGraphController;
class CsaGameCoordinator;

/**
 * @brief 棋譜欄の行変更ハンドラ
 *
 * MainWindowのonRecordPaneMainRowChangedから分離されたロジック。
 * 盤面同期、手番表示、検討モード連携、分岐候補更新を担当する。
 */
class RecordNavigationHandler : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuNavigationState* navState = nullptr;          ///< ナビゲーション状態（非所有）
        KifuBranchTree* branchTree = nullptr;              ///< 分岐ツリー（非所有）
        KifuDisplayCoordinator* displayCoordinator = nullptr; ///< 棋譜表示コーディネータ（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;    ///< 棋譜リストモデル（非所有）
        ShogiView* shogiView = nullptr;                    ///< 盤面ビュー（非所有）
        EvaluationGraphController* evalGraphController = nullptr; ///< 評価値グラフ制御（非所有）
        QStringList* sfenRecord = nullptr;                  ///< SFEN記録（外部所有）
        int* activePly = nullptr;                           ///< アクティブ手数（外部所有）
        int* currentSelectedPly = nullptr;                  ///< 選択中手数（外部所有）
        int* currentMoveIndex = nullptr;                    ///< 現在の手インデックス（外部所有）
        QString* currentSfenStr = nullptr;                  ///< 現在局面SFEN文字列（外部所有）
        bool* skipBoardSyncForBranchNav = nullptr;          ///< 分岐ナビ中の盤面同期スキップフラグ（外部所有）
        CsaGameCoordinator* csaGameCoordinator = nullptr;  ///< CSA対局コーディネータ（非所有）
        PlayMode* playMode = nullptr;                       ///< 対局モード（外部所有）
        MatchCoordinator* match = nullptr;                  ///< 対局調整（非所有）
    };

    explicit RecordNavigationHandler(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    /**
     * @brief 棋譜欄の行が変更されたときの処理
     * @param row 選択された行番号
     */
    void onMainRowChanged(int row);

signals:
    /**
     * @brief 盤面とハイライトの更新が必要（→ MainWindow::onBoardSyncRequired）
     */
    void boardSyncRequired(int ply);

    /**
     * @brief 分岐ツリーからの盤面更新が必要（→ MainWindow::onBranchBoardSyncRequired）
     */
    void branchBoardSyncRequired(const QString& currentSfen, const QString& prevSfen);

    /**
     * @brief 矢印ボタンの有効化が必要（→ MainWindow::enableArrowButtons）
     */
    void enableArrowButtonsRequired();

    /**
     * @brief 手番表示の更新が必要（→ MainWindow::updateTurn）
     */
    void turnUpdateRequired();

    /**
     * @brief 定跡ウィンドウの更新が必要（→ MainWindow::updateJosekiWindow）
     */
    void josekiUpdateRequired();

    /**
     * @brief position文字列の構築が必要（検討モード用）（→ MainWindow::buildAndSendPosition）
     * @param row 行番号
     */
    void buildPositionRequired(int row);

private:
    Deps m_deps;
};

#endif // RECORDNAVIGATIONHANDLER_H
