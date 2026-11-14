#ifndef NETWORKSETTINGS_H
#define NETWORKSETTINGS_H

/// @file networksettings.h
/// @brief CSAネットワーク設定の永続化
///
/// CSA通信対局・待機・ログに関する設定を提供します。

#include <QSize>
#include <QString>
#include "setting.h"

namespace NetworkSettings {

/// CSA通信ログのフォントサイズ（デフォルト: 10）
inline const BoundedSetting<int> csaLogFontSize{"FontSize/csaLog", 10, 8, 24};
/// CSA通信ログの長い行を折り返すか
inline const Setting<bool> csaLogWrapLines{"CsaLog/wrapLines", false};

/// CSA待機ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> csaWaitingDialogFontSize{"FontSize/csaWaitingDialog", 10};

/// CSA通信対局ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> csaGameDialogFontSize{"FontSize/csaGameDialog", 10};

/// CSA通信ログウィンドウのサイズ（デフォルト: 550x450）
inline const Setting<QSize> csaLogWindowSize{"SizeRelated/csaLogWindowSize", QSize(550, 450)};

/// CSA通信対局ダイアログのウィンドウサイズ（デフォルト: 450x380）
inline const Setting<QSize> csaGameDialogSize{"SizeRelated/csaGameDialogSize", QSize(450, 380)};

inline const Setting<QSize> csaWaitingDialogSize{"SizeRelated/csaWaitingDialogSize", QSize(400, 260)};

// --- 通信対局ダイアログの対局者・接続先 ---

/// 対局者が人間か（false ならエンジン。デフォルト: true）
inline const Setting<bool> csaPlayerIsHuman{"CsaGameSettings/isHuman", true};

/// 接続先のホスト名（デフォルト: 空）
inline const Setting<QString> csaHost{"CsaGameSettings/host", QString()};

/// 接続先のポート番号（デフォルト: 4081）
inline const Setting<int> csaPort{"CsaGameSettings/port", 4081};

inline const Setting<bool> csaUseTls{"CsaGameSettings/useTls", false};

/// ログインID（デフォルト: 空）
inline const Setting<QString> csaId{"CsaGameSettings/id", QString()};

/// 旧版が平文で保存していたパスワード。起動時に消す（app/main.cpp）
inline const Setting<QString> csaLegacyPassword{"CsaGameSettings/password", QString()};

// --- 通信対局ダイアログの詳細設定 ---

/// 対局中に空行（LF）を送って接続を維持するか（デフォルト: true）
inline const Setting<bool> csaKeepAliveEnabled{"CsaGameSettings/keepAliveEnabled", true};

/// 空行キープアライブの間隔（秒、デフォルト: 40。仕様の下限は30）
inline const Setting<int> csaKeepAliveIntervalSec{"CsaGameSettings/keepAliveIntervalSec", 40};

/// OS の TCP キープアライブを使うか（デフォルト: true）
inline const Setting<bool> csaTcpKeepAliveEnabled{"CsaGameSettings/tcpKeepAliveEnabled", true};

/// エンジンに渡す持時間から差し引く通信の余裕（秒、デフォルト: 1）
inline const Setting<int> csaThinkingMarginSec{"CsaGameSettings/thinkingMarginSec", 1};

/// 対局数の決め方（0: 1局、1: 指定回数、2: 無制限。デフォルト: 0）
inline const Setting<int> csaGameCountMode{"CsaGameSettings/gameCountMode", 0};

/// 指定回数のときの対局数（デフォルト: 2）
inline const Setting<int> csaGameCount{"CsaGameSettings/gameCount", 2};

/// 終局後にサーバーが切断したら再接続して次の対局を待つか（デフォルト: false）
inline const Setting<bool> csaReconnectAfterGame{"CsaGameSettings/reconnectAfterGame", false};

/// 終局時に棋譜を自動保存するか（デフォルト: false）
inline const Setting<bool> csaAutoSaveEnabled{"CsaGameSettings/autoSaveEnabled", false};

/// 自動保存先フォルダ（デフォルト: 空）
inline const Setting<QString> csaAutoSaveDir{"CsaGameSettings/autoSaveDir", QString()};

/// 自動保存の形式を拡張子で表す（保存の表 KifuFormats::saveTargets の拡張子。選んだことがなければ空で、
/// 通信対局のダイアログはローカル対局と同じ既定の形式 MatchAutoSave::extension を使う）
inline const Setting<QString> csaAutoSaveFormat{"CsaGameSettings/autoSaveFormat", QString()};

/// 後手のときに盤を反転するか（デフォルト: true）
inline const Setting<bool> csaAutoFlipBoard{"CsaGameSettings/autoFlipBoard", true};

/// shogi-server のゲーム名（デフォルト: 空）
inline const Setting<QString> csaGameName{"CsaGameSettings/gameName", QString()};

/// shogi-server の手番の希望（0: 指定なし、1: 先手、2: 後手。デフォルト: 0）
inline const Setting<int> csaTurnPreference{"CsaGameSettings/turnPreference", 0};

/// CSA 通信対局の接続先の履歴の1件
struct CsaServerHistoryEntry {
    QString host;
    int port = 4081;
    bool useTls = false;
    QString id;

    bool operator==(const CsaServerHistoryEntry& other) const
    {
        return host == other.host && port == other.port && useTls == other.useTls && id == other.id;
    }
};

/// CSA 通信対局の接続先の履歴（新しい順。最大 kCsaServerHistoryLimit 件）
inline const StructListSetting<CsaServerHistoryEntry> csaServerHistory{
    "CsaServerHistory",
    {SettingField<CsaServerHistoryEntry>::of("host", &CsaServerHistoryEntry::host),
     SettingField<CsaServerHistoryEntry>::of("port", &CsaServerHistoryEntry::port),
     SettingField<CsaServerHistoryEntry>::of("useTls", &CsaServerHistoryEntry::useTls),
     SettingField<CsaServerHistoryEntry>::of("id", &CsaServerHistoryEntry::id)}};
inline constexpr int kCsaServerHistoryLimit = 10;

// --- P2P対局（実装仕様 11章）。プレイヤー鍵・パスフレーズ・招待文字列は保存しない ---

/// 参加するネットワーク（このビルドが参加できる名前のどれか。既定はローカル）
inline const Setting<QString> p2pNetworkName{"P2p/networkName", QStringLiteral("sbq-local-1")};
/// 名乗る表示名（相手に見える。既定は空で、初めて使うときに入れてもらう）。この PC の何人目のプレイヤーか（1〜）ごとに
/// 覚える（同じ PC でいくつものアプリを動かして対局するとき、同じ名前にならないように。1人目は P2p/displayName、
/// 2人目からは P2p/displayName2・P2p/displayName3…）
inline Setting<QString> p2pDisplayName(int player)
{
    QByteArray key("P2p/displayName");
    if (player > 1) key += QByteArray::number(player);
    return {std::move(key), QString()};
}
/// 招待の持ち時間（秒。既定 10分＋30秒）
inline const BoundedSetting<int> p2pMainTimeSeconds{"P2p/mainTimeSeconds", 600, 0, 86400};
inline const BoundedSetting<int> p2pByoyomiSeconds{"P2p/byoyomiSeconds", 30, 0, 600};
/// 先後の希望（0=指定なし、1=先手、2=後手）
inline const BoundedSetting<int> p2pSidePreference{"P2p/sidePreference", 0, 0, 2};
/// 利用者が指定した協力ノード（/p2p/<PeerId> で終わる multiaddr。16件まで）
inline const ListSetting<QString> p2pBootstrap{QStringLiteral("P2p/bootstrap"), "address"};
/// 直接接続を使わない（協力ノードの中継だけを通り、相手に自分の IP を見せない。既定は無効）
inline const Setting<bool> p2pRelayOnly{"P2p/relayOnly", false};
/// 対局したことのない相手の表示名を出さない（既定は無効）
inline const Setting<bool> p2pHideUnknownNames{"P2p/hideUnknownNames", false};
/// 証跡の書き出し先（最後に使ったフォルダ）
inline const Setting<QString> p2pExportDirectory{"P2p/exportDirectory", QString()};
/// P2P対局のウィンドウの大きさと文字の大きさ
inline const Setting<QSize> p2pWindowSize{"SizeRelated/p2pWindowSize", QSize(820, 680)};
inline const Setting<int> p2pWindowFontSize{"FontSize/p2pWindow", 10};
/// 条件確認画面の大きさと文字の大きさ
inline const Setting<QSize> p2pConfirmDialogSize{"SizeRelated/p2pConfirmDialogSize", QSize(600, 660)};
inline const Setting<int> p2pConfirmDialogFontSize{"FontSize/p2pConfirmDialog", 10};
/// プレイヤー鍵のダイアログの文字の大きさ
inline const Setting<int> p2pIdentityDialogFontSize{"FontSize/p2pIdentityDialog", 10};

} // namespace NetworkSettings

#endif // NETWORKSETTINGS_H
