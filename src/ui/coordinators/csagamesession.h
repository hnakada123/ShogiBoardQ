#ifndef CSAGAMESESSION_H
#define CSAGAMESESSION_H

/// @file csagamesession.h
/// @brief CSA 通信対局の開始（待機ダイアログ）と、通信対局の進行の画面への反映

#include <QObject>
#include <QString>

#include "csaclient.h"
#include "csagamedialog.h"

class CsaGameCoordinator;
class MainWindowAppearanceController;
class QStatusBar;
class QWidget;
class ShogiView;

/**
 * @brief 通信対局の1回の接続（1局または連続した対局）を始め、画面に知らせる
 *
 * 通信の手順は CsaGameCoordinator が行う。このクラスは接続ダイアログの値で対局を始め（待機ダイアログ）、
 * 対局の開始・手番・終局を盤の向き・ステータスバー・結果のダイアログへ反映する。
 * 棋譜（対局者・開始と終了・自動保存）と動作状態は、ローカル対局と同じく司令塔と GameSession が受け持ち、
 * 評価値グラフ・通信ログ・エラー・盤の手の接続は合成ルートがつなぐ。
 */
class CsaGameSession : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        CsaGameCoordinator* coordinator = nullptr;          ///< 通信対局の司令塔（必須）
        ShogiView* shogiView = nullptr;                    ///< 盤の向き
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

public slots:
    /// 待機ダイアログで「キャンセル」した
    void onWaitingCancelled();

private slots:
    void onGameStarted(const QString& blackName, const QString& whiteName);
    void onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    void onTurnChanged(bool isMyTurn);
    void onLogMessage(const QString& message, bool isError);

private:
    void showGameEndDialog(const QString& title, const QString& message);
    void showStatus(const QString& message, int timeoutMs) const;

    Deps m_deps;
    bool m_autoFlipBoard = true;         ///< 後手のときは盤を反転する（接続ダイアログの設定）
};

#endif // CSAGAMESESSION_H
