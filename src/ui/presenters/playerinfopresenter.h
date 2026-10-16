#ifndef PLAYERINFOPRESENTER_H
#define PLAYERINFOPRESENTER_H

/// @file playerinfopresenter.h
/// @brief 盤の対局者名の表示

#include <QList>
#include <QObject>
#include <QString>

#include "kifparsetypes.h"
#include "matchplayers.h"

class AppActivityState;
class KifuDocument;
class ShogiView;

/**
 * @brief 盤の対局者名を出す（表示だけ）
 *
 * 対局中は対局者、対局していなければ対局情報の名前（初期値は「先手」「後手」）を出す。
 * 対局者と対局情報は棋譜の文書（KifuDocument）が持ち、このクラスは書き換えない。
 * エンジンの表示（通信ログの見出し・2つ目のエンジン欄・評価値グラフの系列名）は EngineDisplayPresenter が受け持つ。
 */
class PlayerInfoPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        const KifuDocument* document = nullptr;           ///< 対局者と対局情報（必須、非所有）
        const AppActivityState* activity = nullptr;       ///< 対局中か（必須、非所有）
        ShogiView* shogiView = nullptr;
    };

    explicit PlayerInfoPresenter(const Deps& deps, QObject* parent = nullptr);
    ~PlayerInfoPresenter() override;

    /// 対局中なら対局者の名前を、対局していなければ対局情報の名前を盤に出す
    void showMatchPlayers();
    /// 対局者の名前を盤に出す（対局を始めるとき。動作状態が対局中になる前に呼ぶ）
    void showPlayers(const MatchPlayers& players);

public slots:
    /// 対局情報の名前を盤に出す（棋譜の置き換え・対局情報の反映）
    void syncBoardNamesWithGameInfo();
    /// 対局情報欄で反映した対局情報の名前を盤に出す
    void onGameInfoUpdated(const QList<KifGameInfoItem>& items);

private:
    void setBoardNames(const QString& black, const QString& white);

    Deps m_deps;
};

#endif // PLAYERINFOPRESENTER_H
