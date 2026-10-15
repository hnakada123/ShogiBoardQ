#ifndef PLAYERINFOPRESENTER_H
#define PLAYERINFOPRESENTER_H

/// @file playerinfopresenter.h
/// @brief 対局者名・エンジン名と対局情報の表示

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

#include "matchplayers.h"
#include "kifparsetypes.h"
#include "timecontrol.h"

class QTabWidget;
class QWidget;
class GameInfoPaneController;
class ShogiView;
class EngineAnalysisTab;
class EvaluationGraphController;
class TimeControlController;
class GameRecordModel;
class LiveGameSession;
class UsiCommLogModel;
struct AppState;

/**
 * @brief 対局者名・エンジン名と対局情報を表示先へ反映する
 *
 * 表示先は、盤の対局者名・対局情報ドック（GameInfoPaneController。このクラスが作る）・
 * 通信ログの見出し（E1・E2）・思考タブの2つ目のエンジン欄・検討タブ・評価値グラフ。
 * 対局が決まると対局者を AppState::players に記録し、対局情報（開始日時・対局者・手合割・持ち時間）を作る。
 */
class PlayerInfoPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;
        QTabWidget* tabWidget = nullptr;                  ///< 前回選んだタブを復元する先（任意）
        ShogiView* shogiView = nullptr;
        AppState* state = nullptr;                        ///< 対局者・動作状態・開始局面（非所有）
        TimeControlController* timeController = nullptr;  ///< 持ち時間と開始日時（非所有）
        GameRecordModel* gameRecord = nullptr;            ///< 対局情報を編集したら未保存にする（非所有）
        /// 読み込んだ棋譜の途中局面から対局を続けているかの判定と、続けた対局の対局者・日時を
        /// 分岐の最初の手のコメントとして残す先（非所有）
        LiveGameSession* liveGameSession = nullptr;
    };

    explicit PlayerInfoPresenter(const Deps& deps, QObject* parent = nullptr);
    ~PlayerInfoPresenter() override;

    // --- 後から作られる表示先 ---
    void setAnalysisTab(EngineAnalysisTab* analysisTab);
    void setEvalGraphController(EvaluationGraphController* controller);
    void setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2);

    /// 対局情報ドック
    GameInfoPaneController* gameInfoController() const { return m_gameInfoController; }

    /// 起動時の対局情報を用意し、前回選んだタブを復元する
    void addGameInfoTabAtStartup();

    // --- 対局者 ---

    /// 対局中の対局者（AppState::players）を、盤の名前・通信ログの見出し・思考タブ・評価値グラフに出す。
    /// 対局していなければエンジン名を消し、盤には対局情報の名前（初期値は「先手」「後手」）を出す
    void showMatchPlayers();
    /// 対局情報の先手・後手名を更新する
    void updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName);
    /**
     * @brief 対局情報の対局者名を将棋盤の対局者名ラベルに表示する
     *
     * 名前が空なら「先手」「後手」と表示する（駒落ちの見出しは下手・上手の行から読む）。
     */
    void syncBoardNamesWithGameInfo();

    // --- 対局情報 ---

    /**
     * @brief 対局開始時の対局情報を設定（持ち時間を含む）
     * @param handicap 手合割（駒落ちなら対局者の見出しを下手・上手にする）
     * @param timeControl 持ち時間（双方とも時間制限がなければ「無制限」と記録する）
     */
    void setGameInfoForMatchStart(const QDateTime& startDateTime,
                                  const QString& blackName,
                                  const QString& whiteName,
                                  const QString& handicap,
                                  const TimeControl& timeControl);
    /// 対局終了時の終了日時を対局情報に書く
    void updateGameInfoWithEndTime(const QDateTime& endDateTime);
    /// 持ち時間を対局情報に書く（双方とも時間制限がなければ「無制限」）
    void updateGameInfoWithTimeControl(const TimeControl& timeControl);

public slots:
    /// 読み込んだ棋譜の元の対局情報を覚える
    void setOriginalGameInfo(const QList<KifGameInfoItem>& items);

    /**
     * @brief 対局開始ダイアログで対局者が決まった（対局を始める前）
     *
     * 対局者を AppState に記録して表示し、対局情報（開始日時・対局者・手合割・持ち時間）を作る。
     * 読み込んだ棋譜の途中から続ける対局では、棋譜の対局情報を残して分岐の最初の手にメモを付ける。
     */
    void onMatchPlayersResolved(const MatchPlayers& players);

    /// 検討・詰み探索のエンジンの名前が変わった（検討タブと思考タブのエンジン名・評価値グラフに出す）
    void onAnalysisEngineNamesChanged(const QString& engine1, const QString& engine2);

signals:
    /// 中央のタブが切り替わった
    void tabCurrentChanged(int index);

private:
    void onGameInfoUpdated(const QList<KifGameInfoItem>& items);
    void applyBoardNames(const QList<KifGameInfoItem>& items);
    /// 対局者を盤・対局情報・通信ログの見出し・思考タブ・評価値グラフに出す（nullptr は対局していない）
    void showPlayers(const MatchPlayers* players);
    void setSecondEngineVisible(bool visible);
    bool continuesExistingRecord() const;

    QWidget* m_parentWidget = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    ShogiView* m_shogiView = nullptr;
    AppState* m_state = nullptr;
    TimeControlController* m_timeController = nullptr;
    GameRecordModel* m_gameRecord = nullptr;
    LiveGameSession* m_liveGameSession = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    EvaluationGraphController* m_evalGraphController = nullptr;
    UsiCommLogModel* m_logModel1 = nullptr;
    UsiCommLogModel* m_logModel2 = nullptr;

    GameInfoPaneController* m_gameInfoController = nullptr;  ///< 親は parentWidget

    /// 棋譜の途中から続けた対局では、終局時にも棋譜の対局情報（終了日時など）を書き換えない
    bool m_keepRecordHeader = false;
    bool m_secondEngineVisible = false;  ///< 思考タブを後から渡されたときにも引き継ぐ
};

#endif // PLAYERINFOPRESENTER_H
