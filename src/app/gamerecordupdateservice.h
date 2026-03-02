#ifndef GAMERECORDUPDATESERVICE_H
#define GAMERECORDUPDATESERVICE_H

/// @file gamerecordupdateservice.h
/// @brief 棋譜追記・ライブセッション更新ロジックを集約するサービスの定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class MatchCoordinator;
class GameRecordPresenter;
class LiveGameSessionUpdater;
class LiveGameSession;
struct ShogiMove;

/**
 * @brief 棋譜追記・ライブセッション更新ロジックを集約するサービス
 *
 * MainWindow から appendKifuLine / updateGameRecord / onMoveCommitted の後半処理を分離し、
 * 棋譜モデル更新・LiveGameSession 連携・USI 指し手記録を一元管理する。
 */
class GameRecordUpdateService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // Lazy-init callbacks（呼び出し時に ensure + ポインタ返却）
        std::function<GameRecordPresenter*()> ensureRecordPresenter;
        std::function<LiveGameSessionUpdater*()> ensureLiveGameSessionUpdater;

        // Object pointers
        MatchCoordinator* match = nullptr;
        LiveGameSession* liveGameSession = nullptr;

        // State pointers
        QList<ShogiMove>* gameMoves = nullptr;
        QStringList* gameUsiMoves = nullptr;
        QString* currentSfenStr = nullptr;
        QStringList* sfenRecord = nullptr;
    };

    explicit GameRecordUpdateService(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /**
     * @brief Game Record を更新する
     * @param moveText 指し手テキスト（空の場合はスキップ）
     * @param elapsedTime 消費時間
     */
    void updateGameRecord(const QString& moveText, const QString& elapsedTime);

    /**
     * @brief USI形式の指し手を記録し currentSfen を更新する
     *
     * onMoveCommitted() の後半処理（USI追記・SFEN更新）を担当する。
     */
    void recordUsiMoveAndUpdateSfen();

public slots:
    /**
     * @brief 棋譜1行の追記を記録更新フローへ橋渡しする
     * @param text 指し手テキスト
     * @param elapsedTime 消費時間
     */
    void appendKifuLine(const QString& text, const QString& elapsedTime);

private:
    Deps m_deps;
};

#endif // GAMERECORDUPDATESERVICE_H
