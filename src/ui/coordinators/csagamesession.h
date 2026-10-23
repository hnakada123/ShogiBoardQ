#ifndef CSAGAMESESSION_H
#define CSAGAMESESSION_H

/// @file csagamesession.h
/// @brief CSA 通信対局の開始（待機ダイアログ）と、通信対局の進行の画面への反映

#include <QObject>
#include <QString>

#include "appactivitystate.h"
#include "csaclient.h"
#include "usiscore.h"

class BoardInteractionController;
class BoardMoveController;
class CsaGameCoordinator;
#include "csagamedialog.h"
class CsaLogPanel;
class EvaluationGraphPresenter;
class MainWindowAppearanceController;
class QStatusBar;
class QWidget;
class ShogiView;
class NotificationPresenter;
struct RecordEntry;

/**
 * @brief 通信対局の1回の接続（1局または連続した対局）を進める
 *
 * 通信の手順は CsaGameCoordinator が行う。このクラスは接続ダイアログの値で対局を始め、
 * 対局の開始・手番・終局を盤の向き・評価値グラフ・ステータスバー・結果のダイアログへ反映する。
 * 通信対局の間は動作状態を「通信対局中」にする。棋譜（対局者・開始と終了・自動保存）は
 * ローカル対局と同じく GameSession が受け持つ。
 */
class CsaGameSession : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        CsaGameCoordinator* coordinator = nullptr;          ///< 通信対局の司令塔（必須）
        AppActivityState* activity = nullptr;              ///< 通信対局中の動作状態（必須）
        ShogiView* shogiView = nullptr;                    ///< 盤の向き
        BoardInteractionController* boardController = nullptr; ///< 相手の手の強調
        BoardMoveController* boardMoves = nullptr;         ///< 人間が盤で選んだ手をサーバーへ送る
        CsaLogPanel* csaLog = nullptr;                     ///< CSA の通信ログ・接続状態・手入力のコマンド
        EvaluationGraphPresenter* evalGraph = nullptr;     ///< 自分のエンジンの評価値
        NotificationPresenter* notifications = nullptr;    ///< 司令塔のエラー
        MainWindowAppearanceController* appearance = nullptr; ///< 後手のとき盤を反転する
        QStatusBar* statusBar = nullptr;
        QWidget* parentWidget = nullptr;                   ///< 待機・結果ダイアログの親
    };

    explicit CsaGameSession(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief 接続ダイアログの値で通信対局を始める
     *
     * 待機ダイアログを出し、対局が始まるかエラー・キャンセルまで待つ。始まったら true。
     */
    bool startGame(const CsaGameDialog::Selection& selection);

    CsaGameCoordinator* coordinator() const { return m_deps.coordinator; }
    /// 通信対局中か（対局が始まってから終局まで）
    bool isActive() const { return m_activityLease.isCurrent(); }

public slots:
    /// 待機ダイアログで「キャンセル」した
    void onWaitingCancelled();

private slots:
    void onGameStarted(const QString& blackName, const QString& whiteName);
    void onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    void onTurnChanged(bool isMyTurn);
    void onLogMessage(const QString& message, bool isError);
    void onEngineScoreUpdated(const UsiScore& score, int ply);

private:
    void showGameEndDialog(const QString& title, const QString& message);
    void showStatus(const QString& message, int timeoutMs) const;

    Deps m_deps;
    ActivityLease m_activityLease;       ///< 通信対局中の動作状態（対局の開始で受け取り、終局・取りやめで手放す）

    bool m_autoFlipBoard = true;         ///< 後手のときは盤を反転する（接続ダイアログの設定）
};

#endif // CSAGAMESESSION_H
