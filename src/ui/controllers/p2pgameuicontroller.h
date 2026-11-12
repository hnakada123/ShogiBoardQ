#ifndef P2PGAMEUICONTROLLER_H
#define P2PGAMEUICONTROLLER_H

/// @file p2pgameuicontroller.h
/// @brief P2P対局の画面（ウィンドウ・プレイヤー鍵の準備・条件確認・申し出・終局の知らせ。実装仕様 10.2節・11章）

#include <QObject>
#include <QPointer>
#include <QString>

#include <optional>

#include "p2pframes.h"
#include "p2pidentitystore.h"
#include "p2pmatch.h"
#include "peerprocess.h"

class AppActivityState;
class MainWindowAppearanceController;
class P2pConfirmDialog;
class P2pGameCoordinator;
class P2pLobbyService;
class P2pWindow;
class QMessageBox;
class QWidget;
class ShogiView;

/**
 * @brief P2P対局の利用者の操作と画面への反映
 *
 * 通信プロセス・プレイヤー鍵・保存先を持つ部品（P2pLobbyService）とウィンドウは、利用者が「P2P対局」を開いたときに
 * 初めて作る。自動化 API を有効にして起動したときは作らない（ローカルのソケットから招待・対局・署名を始められない
 * ようにする。実装仕様 10.2節）。相手から来た文字列は PlainText で出す。
 */
class P2pGameUiController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        P2pGameCoordinator* game = nullptr;
        AppActivityState* activity = nullptr;
        QWidget* parentWidget = nullptr;
        ShogiView* shogiView = nullptr;                        ///< 盤の向き
        MainWindowAppearanceController* appearance = nullptr;  ///< 後手のとき盤を反転する（通信対局と同じ）
    };

    explicit P2pGameUiController(const Deps& deps, QObject* parent = nullptr);
    ~P2pGameUiController() override;

    /// 自動化 API を有効にして起動した（P2P対局の部品を作らない）
    void disableForAutomation();
    /// アプリの終了（招待を無効にし、対局中なら中断を知らせ、通信プロセスを止める）
    void shutdown();

public slots:
    /// 「P2P対局」のウィンドウを開く（初めてならロックを取り、部品を作る）
    void openWindow();

private slots:
    void onStartRequested();
    void onStopRequested();
    void onRecreateIdentity();
    void onNegotiationChanged();
    void onNegotiationEnded(bool started, std::optional<P2p::ErrorCode> code, bool byPeer);
    void onGameStarted();
    void onOfferReceived(P2p::Cause cause);
    /// 相手の申し出に答えた（result は QMessageBox::Yes なら承諾）
    void onOfferAnswered(int result);
    /// 申し出が無効になった（手数が進んだ・結果が決まった・対局が終わった）なら、尋ねる箱を閉じる
    void closeStaleOffer();
    void onProposalDeclined();
    void onGameFinished(P2p::MatchEnding ending, std::optional<P2p::ErrorCode> error, const QString& evidencePath);
    void onPeerFailed(PeerProcess::Failure failure, const QString& detail);
    /// パスフレーズの要らない読み込み（保管庫・暗号化しないファイル）が終わった
    void onIdentityFinished(P2pIdentityStore::Failure failure);

private:
    bool ensureLobby();
    /// ネットワークに参加する（プレイヤー鍵は読み込み済み）
    void beginNetwork();
    QWidget* dialogParent() const;
    /// 尋ねる箱を、答えとして扱わずに閉じる
    void closeOfferBox();

    Deps m_deps;
    P2pLobbyService* m_lobby = nullptr;          ///< 子オブジェクト（開いたときに作る）
    QPointer<P2pWindow> m_window;
    QPointer<P2pConfirmDialog> m_confirm;
    QPointer<QMessageBox> m_offerBox;   ///< 相手の申し出を尋ねる箱（モードレス。閉じたら消える）
    bool m_automationDisabled = false;
    bool m_startAfterUnlock = false;   ///< 鍵を読み込んだら参加する
};

#endif // P2PGAMEUICONTROLLER_H
