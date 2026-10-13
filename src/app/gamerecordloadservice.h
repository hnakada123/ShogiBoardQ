#ifndef GAMERECORDLOADSERVICE_H
#define GAMERECORDLOADSERVICE_H

#include <QList>
#include <QStringList>
#include <functional>

class GameRecordModel;
class GameRecordPresenter;
class RecordPane;
struct KifDisplayItem;

/// @brief 棋譜表示時の初期化とコメント同期を担当するサービス
///
/// MainWindow::displayGameRecord から抽出。棋譜読み込み時に
/// - GameRecordModel の初期化
/// - RecordPresenter への表示委譲
/// を一括で行う。
class GameRecordLoadService
{
public:
    struct Deps {
        // Object pointers（非所有）
        RecordPane* recordPane = nullptr;             ///< 棋譜欄ウィジェット

        // Lazy-init callbacks
        std::function<GameRecordPresenter*()> ensureRecordPresenter; ///< RecordPresenter 遅延初期化
        std::function<GameRecordModel*()> ensureGameRecordModel;     ///< GameRecordModel 遅延初期化

        // Data accessor
    };

    void updateDeps(const Deps& deps);

    /// 棋譜表示の初期化を実行する
    void loadGameRecord(const QList<KifDisplayItem>& disp);

private:
    Deps m_deps;
};

#endif // GAMERECORDLOADSERVICE_H
