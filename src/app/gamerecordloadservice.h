#ifndef GAMERECORDLOADSERVICE_H
#define GAMERECORDLOADSERVICE_H

#include <QList>
#include <QStringList>
#include <QVector>
#include <functional>

class GameRecordModel;
class GameRecordPresenter;
class RecordPane;
struct KifDisplayItem;
struct ShogiMove;

/// @brief 棋譜表示時の初期化とコメント同期を担当するサービス
///
/// MainWindow::displayGameRecord から抽出。棋譜読み込み時に
/// - ゲーム状態（USI指し手・ShogiMove）のクリア
/// - GameRecordModel の初期化
/// - commentsByRow の構築
/// - RecordPresenter への表示委譲
/// を一括で行う。
class GameRecordLoadService
{
public:
    struct Deps {
        // State pointers（外部所有）
        QStringList* gameUsiMoves = nullptr;          ///< USI形式指し手リスト
        QVector<ShogiMove>* gameMoves = nullptr;      ///< ShogiMove リスト
        QVector<QString>* commentsByRow = nullptr;    ///< 行ごとのコメント配列

        // Object pointers（非所有）
        GameRecordModel* gameRecord = nullptr;        ///< 棋譜データモデル
        GameRecordPresenter* recordPresenter = nullptr; ///< 棋譜表示プレゼンタ
        RecordPane* recordPane = nullptr;             ///< 棋譜欄ウィジェット

        // Lazy-init callbacks
        std::function<GameRecordPresenter*()> ensureRecordPresenter; ///< RecordPresenter 遅延初期化
        std::function<GameRecordModel*()> ensureGameRecordModel;     ///< GameRecordModel 遅延初期化

        // Data accessor
        std::function<const QStringList*()> sfenRecord; ///< SFEN記録取得
    };

    void updateDeps(const Deps& deps);

    /// 棋譜表示の初期化を実行する
    void loadGameRecord(const QList<KifDisplayItem>& disp);

private:
    Deps m_deps;
};

#endif // GAMERECORDLOADSERVICE_H
