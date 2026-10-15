#ifndef RECORDNAVIGATIONHANDLER_H
#define RECORDNAVIGATIONHANDLER_H

/// @file recordnavigationhandler.h
/// @brief 棋譜欄行変更ハンドラクラスの定義


#include <QObject>

class KifuNavigationState;
class KifuDisplayCoordinator;
class LiveGameSession;

/**
 * @brief 棋譜欄の行変更ハンドラ
 *
 * MainWindowのonRecordPaneMainRowChangedから分離されたロジック。
 * 対局中の行変更を除き、表示中のラインのノードへ移動要求を送る。
 */
class RecordNavigationHandler : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuNavigationState* navState = nullptr;          ///< ナビゲーション状態（非所有）
        KifuDisplayCoordinator* displayCoordinator = nullptr; ///< 棋譜表示コーディネータ（非所有）
        LiveGameSession* liveSession = nullptr;          ///< 対局棋譜の更新状態（非所有）
    };

    explicit RecordNavigationHandler(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    /**
     * @brief 棋譜欄の行が変更されたときの処理
     * @param row 選択された行番号
     */
    void onMainRowChanged(int row);

private:
    Deps m_deps;
};

#endif // RECORDNAVIGATIONHANDLER_H
