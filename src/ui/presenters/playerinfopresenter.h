#ifndef PLAYERINFOPRESENTER_H
#define PLAYERINFOPRESENTER_H

/// @file playerinfopresenter.h
/// @brief 対局者名・エンジン名の表示

#include <QList>
#include <QObject>
#include <QString>

#include "kifparsetypes.h"
#include "matchplayers.h"

class AppActivityState;
class ConsiderationTabManager;
class EvaluationGraphPresenter;
class KifuDocument;
class ShogiView;
class ThinkingPanel;
class UsiCommLogModel;

/**
 * @brief 対局者名・エンジン名を表示先へ反映する（表示だけ）
 *
 * 表示先は、盤の対局者名・通信ログの見出し（E1・E2）・思考タブの2つ目のエンジン欄・検討タブ・評価値グラフ。
 * 対局者と対局情報は棋譜の文書（KifuDocument）が持ち、このクラスは書き換えない。
 * 盤の対局者名は、対局中は対局者、対局していなければ対局情報の名前（初期値は「先手」「後手」）。
 */
class PlayerInfoPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        const KifuDocument* document = nullptr;           ///< 対局者と対局情報（必須、非所有）
        const AppActivityState* activity = nullptr;       ///< 対局中か（必須、非所有）
        ShogiView* shogiView = nullptr;
        ThinkingPanel* thinking = nullptr;                ///< 2つ目のエンジン欄を出すか・エンジン名
        ConsiderationTabManager* consideration = nullptr; ///< 検討のエンジン名
        EvaluationGraphPresenter* evalGraph = nullptr;    ///< 評価値グラフの系列名
        UsiCommLogModel* logModel1 = nullptr;             ///< エンジン欄の名前（E1）
        UsiCommLogModel* logModel2 = nullptr;             ///< エンジン欄の名前（E2）
    };

    explicit PlayerInfoPresenter(const Deps& deps, QObject* parent = nullptr);
    ~PlayerInfoPresenter() override;

    /// 対局中の対局者を、盤の名前・通信ログの見出し・思考タブ・評価値グラフに出す。
    /// 対局していなければエンジン名を消し、盤には対局情報の名前を出す
    void showMatchPlayers();
    /// 対局者を出す（対局を始めるとき。動作状態が対局中になる前に呼ぶ）
    void showPlayers(const MatchPlayers& players);

public slots:
    /// 対局情報の名前を盤に出す（棋譜の置き換え・対局情報の反映）
    void syncBoardNamesWithGameInfo();
    /// 対局情報欄で反映した対局情報の名前を盤に出す
    void onGameInfoUpdated(const QList<KifGameInfoItem>& items);
    /// 検討・詰み探索のエンジンの名前が変わった（検討タブと思考タブのエンジン名・評価値グラフに出す）
    void onAnalysisEngineNamesChanged(const QString& engine1, const QString& engine2);

private:
    /// 対局者を盤・通信ログの見出し・思考タブ・評価値グラフに出す（nullptr は対局していない）
    void showPlayers(const MatchPlayers* players);
    void setBoardNames(const QString& black, const QString& white);
    void setSecondEngineVisible(bool visible);

    Deps m_deps;
    bool m_secondEngineVisible = false;  ///< 思考タブを後から渡されたときにも引き継ぐ
};

#endif // PLAYERINFOPRESENTER_H
