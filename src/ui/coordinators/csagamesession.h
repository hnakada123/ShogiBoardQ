#ifndef CSAGAMESESSION_H
#define CSAGAMESESSION_H

/// @file csagamesession.h
/// @brief CSA 通信対局の開始（待機ダイアログ）と、通信対局の進行の画面への反映

#include <QObject>
#include <QString>
#include <functional>

#include "appactivitystate.h"
#include "csaclient.h"
#include "timecontrol.h"
#include "usiscore.h"

class BoardInteractionController;
class BoardMoveController;
class CsaGameCoordinator;
#include "csagamedialog.h"
class CsaLogPanel;
class EvaluationChartWidget;
class KifuFileController;
class MainWindowAppearanceController;
class MatchRecordSink;
class QStatusBar;
class QWidget;
class ShogiView;
class UiNotificationService;
struct RecordEntry;

/**
 * @brief 通信対局の1回の接続（1局または連続した対局）を進める
 *
 * 通信の手順は CsaGameCoordinator が行う。このクラスは接続ダイアログの値で対局を始め、
 * 対局の開始・着手・終局を棋譜・対局情報・盤・評価値グラフ・ステータスバーへ反映し、
 * 終局した棋譜を自動保存する。通信対局の間は動作状態を「通信対局中」にする。
 */
class CsaGameSession : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        CsaGameCoordinator* coordinator = nullptr;          ///< 通信対局の司令塔（必須）
        AppActivityState* activity = nullptr;              ///< 通信対局中の動作状態（必須）
        MatchRecordSink* recordSink = nullptr;             ///< 棋譜の行の書き先（必須）
        ShogiView* shogiView = nullptr;                    ///< 対局者名
        BoardInteractionController* boardController = nullptr; ///< 相手の手の強調
        BoardMoveController* boardMoves = nullptr;         ///< 人間が盤で選んだ手をサーバーへ送る
        CsaLogPanel* csaLog = nullptr;                     ///< CSA の通信ログ・接続状態・手入力のコマンド
        EvaluationChartWidget* evalChart = nullptr;        ///< 自分のエンジンの評価値
        UiNotificationService* notifications = nullptr;    ///< 司令塔のエラー
        KifuFileController* kifuFiles = nullptr;           ///< 終局した棋譜の自動保存
        MainWindowAppearanceController* appearance = nullptr; ///< 後手のとき盤を反転する
        QStatusBar* statusBar = nullptr;
        QWidget* parentWidget = nullptr;                   ///< 待機・結果ダイアログの親
        /// 棋譜と対局情報を開始局面・対局者名・持ち時間（サーバーの対局条件）で作り直し、ライブ記録を始める
        std::function<void(const QString& startSfen, const QString& blackName, const QString& whiteName,
                           const TimeControl& timeControl)> prepareRecord;
        /// ライブ記録を確定し、終局日時を記録して対局情報に反映する
        std::function<void()> recordGameEnd;
    };

    explicit CsaGameSession(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief 接続ダイアログの値で通信対局を始める
     *
     * 待機ダイアログを出し、対局が始まるかエラー・キャンセルまで待つ。始まったら true。
     */
    bool startGame(const CsaGameDialog::Selection& selection);

    CsaGameCoordinator* coordinator() const { return m_deps.coordinator; }
    /// 通信対局中か（待機中・対局中）
    bool isActive() const { return m_deps.activity->is(AppActivityState::Activity::DuringCsaGame); }

public slots:
    /// 待機ダイアログで「キャンセル」した
    void onWaitingCancelled();

private slots:
    void onRecordEntryReady(const RecordEntry& entry);
    void onGameStarted(const QString& blackName, const QString& whiteName, const QList<RecordEntry>& initialRecords);
    void onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    void onTurnChanged(bool isMyTurn);
    void onLogMessage(const QString& message, bool isError);
    void onEngineScoreUpdated(const UsiScore& score, int ply);

private:
    void showGameEndDialog(const QString& title, const QString& message);
    void showStatus(const QString& message, int timeoutMs) const;

    Deps m_deps;

    // 接続ダイアログの詳細設定（このクラスで扱うもの）
    bool m_autoSaveEnabled = false;      ///< 終局時に棋譜を自動保存する
    QString m_autoSaveDir;               ///< 自動保存先フォルダ
    QString m_autoSaveFormat = QStringLiteral("kif"); ///< 自動保存の形式（拡張子）
    bool m_autoFlipBoard = true;         ///< 後手のときは盤を反転する
};

#endif // CSAGAMESESSION_H
