#ifndef BOARDLOADSERVICE_H
#define BOARDLOADSERVICE_H

/// @file boardloadservice.h
/// @brief SFEN適用/分岐同期の盤面読み込みサービスの定義

#include <QObject>
#include <QString>
#include <functional>

class ShogiGameController;
class ShogiView;
class BoardSyncPresenter;

/**
 * @brief SFEN文字列からの盤面適用とハイライト同期を担当するサービス
 *
 * MainWindow から loadBoardFromSfen / loadBoardWithHighlights のロジックを
 * 集約し、盤面読み込みの責務を一元管理する。
 */
class BoardLoadService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiGameController* gc = nullptr;               ///< ゲームコントローラ（非所有）
        ShogiView* view = nullptr;                        ///< 将棋盤ビュー（非所有）
        BoardSyncPresenter* boardSync = nullptr;          ///< 盤面同期プレゼンタ（非所有）
        QString* currentSfenStr = nullptr;                ///< 現在局面SFEN文字列（外部所有）
        std::function<void()> setCurrentTurn;             ///< 手番表示更新コールバック
        std::function<void()> ensureBoardSyncPresenter;   ///< BoardSyncPresenter 遅延初期化
        std::function<void()> beginBranchNavGuard;        ///< 分岐ナビガード開始
    };

    explicit BoardLoadService(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// SFEN文字列から盤面を直接読み込む（分岐ナビゲーション用）
    void loadFromSfen(const QString& sfen);

    /// SFEN差分から盤面を更新しハイライトを表示する（分岐ナビゲーション用）
    void loadWithHighlights(const QString& currentSfen, const QString& prevSfen);

private:
    Deps m_deps;
};

#endif // BOARDLOADSERVICE_H
