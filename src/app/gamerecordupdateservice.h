#ifndef GAMERECORDUPDATESERVICE_H
#define GAMERECORDUPDATESERVICE_H

/// @file gamerecordupdateservice.h
/// @brief 棋譜追記・ライブセッション更新ロジックを集約するサービスの定義

#include <QObject>
#include "recordentry.h"
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class GameRecordModel;
class MatchCoordinator;
class LiveGameSession;
struct ShogiMove;

/// 確定した指し手を正本へ追加する。表示は正本の変更通知を購読する。
class GameRecordUpdateService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // 起動時に注入する依存
        GameRecordModel* gameRecord = nullptr;           ///< 追記したら棋譜を未保存にする

        // Object pointers
        MatchCoordinator* match = nullptr;
        LiveGameSession* liveGameSession = nullptr;

        // State pointers
        QString* currentSfenStr = nullptr;
        QStringList* sfenRecord = nullptr;
    };

    explicit GameRecordUpdateService(Deps deps, QObject* parent = nullptr);

    /**
     * @brief 着手後に currentSfen を局面列の最後（指した後の局面）に更新する
     *
     * onMoveCommitted() の後半処理。指し手は分岐ツリー（棋譜の正本）に記録済み。
     */
    void updateCurrentSfenAfterMove();

public slots:
    void appendRecord(const RecordEntry& entry);

private:
    const Deps m_deps;
};

#endif // GAMERECORDUPDATESERVICE_H
