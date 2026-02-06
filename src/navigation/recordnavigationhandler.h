#ifndef RECORDNAVIGATIONHANDLER_H
#define RECORDNAVIGATIONHANDLER_H

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
        // ナビゲーション状態
        KifuNavigationState* navState = nullptr;
        KifuBranchTree* branchTree = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;

        // モデル
        KifuRecordListModel* kifuRecordModel = nullptr;

        // ビュー
        ShogiView* shogiView = nullptr;

        // 評価値グラフ
        EvaluationGraphController* evalGraphController = nullptr;

        // SFEN記録
        QStringList* sfenRecord = nullptr;

        // ply追跡変数（外部所有）
        int* activePly = nullptr;
        int* currentSelectedPly = nullptr;
        int* currentMoveIndex = nullptr;

        // SFEN文字列（外部所有）
        QString* currentSfenStr = nullptr;

        // スキップフラグ（外部所有）
        bool* skipBoardSyncForBranchNav = nullptr;

        // CSA対局コーディネータ
        CsaGameCoordinator* csaGameCoordinator = nullptr;

        // 検討モード関連
        PlayMode* playMode = nullptr;
        MatchCoordinator* match = nullptr;
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
     * @brief 盤面とハイライトの更新が必要
     */
    void boardSyncRequired(int ply);

    /**
     * @brief 分岐ツリーからの盤面更新が必要
     */
    void branchBoardSyncRequired(const QString& currentSfen, const QString& prevSfen);

    /**
     * @brief 矢印ボタンの有効化が必要
     */
    void enableArrowButtonsRequired();

    /**
     * @brief 手番表示の更新が必要
     */
    void turnUpdateRequired();

    /**
     * @brief 定跡ウィンドウの更新が必要
     */
    void josekiUpdateRequired();

    /**
     * @brief position文字列の構築が必要（検討モード用）
     * @param row 行番号
     */
    void buildPositionRequired(int row);

private:
    Deps m_deps;
};

#endif // RECORDNAVIGATIONHANDLER_H
