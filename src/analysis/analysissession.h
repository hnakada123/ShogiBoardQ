#ifndef ANALYSISSESSION_H
#define ANALYSISSESSION_H

/// @file analysissession.h
/// @brief 検討・詰み探索（1つのエンジンで局面を調べる）のセッション

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include "analysisoptions.h"
#include "enginelifecyclemanager.h"
#include "usimessages.h"

class AppActivityState;
class KifuNavigationState;
class Usi;

/**
 * @brief 検討・詰み探索のセッション
 *
 * エンジンを E1 の枠に借りて（EngineLease）局面を調べる。検討は局面や候補手の数が変わるたびに探索をやり直し、
 * 詰み探索は結果が出たら終わる。開始・停止・やり直しを1つの状態遷移として扱う。
 * 対局（MatchCoordinator）・棋譜解析とはエンジンの枠だけを共有する（同時には動かない）。ほかの処理が枠を取ったら
 * セッションを終え、終わったことを知らせる。
 */
class AnalysisSession : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        EngineLifecycleManager* engines = nullptr;  ///< エンジンの枠（必須、非所有）
        AppActivityState* activity = nullptr;       ///< アプリの動作状態（必須、非所有）
        const KifuNavigationState* navState = nullptr; ///< 表示中の手（検討は表示中の局面を追う。非所有）
    };

    explicit AnalysisSession(const Deps& deps, QObject* parent = nullptr);

    /// 検討・詰み探索を始める（前のセッションと、ほかの処理のエンジンは破棄する）。起動の失敗は後で知らせる
    bool start(const AnalysisOptions& opt);
    /// 終わらせて自分のエンジンを手放す（思考情報の表示は残す）。検討・詰み探索をしていなければ何もしない
    void stop();

    bool isConsidering() const { return m_mode == Mode::Consideration; }
    bool isTsumeSearching() const { return m_mode == Mode::Tsume; }
    bool isShutdownInProgress() const;
    /// 検討・詰み探索に使っているエンジン（使っていなければ nullptr）
    Usi* engine() const;
    /// 検討・詰み探索に使っているエンジンの名前（使っていなければ空）
    QString engineName() const { return m_mode == Mode::None ? QString() : m_engineName; }

    /// 検討の候補手の数（検討の表の行数）
    int considerationMultiPV() const { return m_multiPV; }
    /// 検討中に候補手の数を変える
    void updateMultiPV(int multiPV);
    /**
     * @brief 検討中に局面を変える
     * @return true: やり直しを始めた, false: 検討中でないか同じ局面
     */
    bool updatePosition(const UsiPosition& position);

public slots:
    /// 検討中なら、表示中の手の局面で検討をやり直す
    void followDisplayedPosition();

signals:
    /// 検討の探索を始めた（開始・局面や候補手の数を変えたやり直し。経過時間を数え直す）
    void considerationSearchStarted();
    /// 検討の表の候補手が古くなった（局面・候補手の数を変えた。表を消す）
    void considerationResultsCleared();
    void considerationModeEnded();
    void tsumeSearchModeEnded();
    /// 検討の探索が終わり、次の局面を待っている（経過時間の表示を止める）
    void considerationWaitingStarted();
    /// 枠 E1・E2 のエンジン名が変わった（検討・詰み探索は E1 だけ使う。エンジン名の表示を合わせる）
    void engineNamesChanged(const QString& engine1, const QString& engine2);
    /// 結果やエラーの知らせ（ダイアログ）
    void messageRequested(const QString& title, const QString& message);

private slots:
    void onEngineError(const QString& message);
    void onCheckmate(const UsiCheckmate& result);
    void onTsumeBestMoveReceived(const UsiBestMove& result);
    void onSearchTimedOut();
    void onConsiderationBestMoveReceived(const UsiBestMove& result);
    void restartConsiderationDeferred();
    void onEngineRevoked(QObject* owner);

private:
    enum class Mode { None, Consideration, Tsume };
    enum class State { Idle, Searching, StoppingSearch, RestartQueued, Waiting };

    void startSearch();
    /// セッションを終える（知らせない）。終えたモードを返す
    Mode endSession();

    void finalizeTsumeSearch(const QString& resultMessage);
    void requestConsiderationRestart();

    Deps m_deps;
    EngineLease m_lease;                          ///< 枠 E1 のエンジンを使う権利
    QPointer<Usi> m_engine;                       ///< m_lease のエンジン
    Mode m_mode = Mode::None;
    State m_state = State::Idle;
    QString m_engineName;                         ///< 使っているエンジンの名前
    QTimer m_restartTimer;                       ///< 停止時に取り消す再開要求
    UsiPosition m_position;                       ///< 検討・詰み探索する局面
    int m_byoyomiMs = 0;                          ///< 検討の時間制限（ms）
    int m_multiPV = 1;                            ///< 検討の候補手数
};

#endif // ANALYSISSESSION_H
