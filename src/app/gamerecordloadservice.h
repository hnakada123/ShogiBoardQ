#ifndef GAMERECORDLOADSERVICE_H
#define GAMERECORDLOADSERVICE_H

/// @file gamerecordloadservice.h
/// @brief 読み込んだ棋譜を棋譜モデルと棋譜欄へ表示するサービス

#include <QList>
#include <QObject>

#include "kifdisplayitem.h"

class GameRecordModel;
class GameRecordPresenter;
class RecordPane;

/**
 * @brief 棋譜の読み込み完了時に、棋譜モデルを初期化して棋譜欄へ表示する
 *
 * KifuLoadCoordinator::displayGameRecord から呼ばれる。
 */
class GameRecordLoadService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        RecordPane* recordPane = nullptr;              ///< 棋譜欄ウィジェット（非所有）
        GameRecordPresenter* recordPresenter = nullptr; ///< 棋譜欄の表示（非所有）
        GameRecordModel* gameRecord = nullptr;          ///< 棋譜モデル（非所有）
    };

    explicit GameRecordLoadService(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    void loadGameRecord(const QList<KifDisplayItem>& disp);

private:
    Deps m_deps;
};

#endif // GAMERECORDLOADSERVICE_H
