#ifndef NETWORKSETTINGS_H
#define NETWORKSETTINGS_H

/// @file networksettings.h
/// @brief CSAネットワーク設定の永続化
///
/// CSA通信対局・待機・ログに関する設定を提供します。
/// 呼び出し元: csawaitingdialog.cpp, csagamedialog.cpp, csalogpanel.cpp（CSA通信ログ）

#include <QSize>
#include <QString>
#include "setting.h"

namespace NetworkSettings {

/// CSA通信ログのフォントサイズ（デフォルト: 10）
inline const Setting<int> csaLogFontSize{"FontSize/csaLog", 10};

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

/// 対局に使うエンジン名（登録一覧から名前で選び直す）
inline const Setting<QString> csaEngineName{"CsaGameSettings/engineName", QString()};

/// 名前で見つからないときに使う登録一覧の番号（旧版の保存値）
inline const Setting<int> csaEngineNumber{"CsaGameSettings/engineNumber", 0};

/// 接続先のホスト名（デフォルト: 空）
inline const Setting<QString> csaHost{"CsaGameSettings/host", QString()};

/// 接続先のポート番号（デフォルト: 4081）
inline const Setting<int> csaPort{"CsaGameSettings/port", 4081};

inline const Setting<bool> csaUseTls{"CsaGameSettings/useTls", false};

/// ログインID（デフォルト: 空）
inline const Setting<QString> csaId{"CsaGameSettings/id", QString()};

/// 旧版が平文で保存していたパスワード。読み込み・保存のたびに消す
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

/// 自動保存の形式を拡張子で表す（kif/kifu/ki2/csa/jkf/usi。デフォルト: kif）
inline const Setting<QString> csaAutoSaveFormat{"CsaGameSettings/autoSaveFormat", QStringLiteral("kif")};

/// 後手のときに盤を反転するか（デフォルト: true）
inline const Setting<bool> csaAutoFlipBoard{"CsaGameSettings/autoFlipBoard", true};

/// shogi-server のゲーム名（デフォルト: 空）
inline const Setting<QString> csaGameName{"CsaGameSettings/gameName", QString()};

/// shogi-server の手番の希望（0: 指定なし、1: 先手、2: 後手。デフォルト: 0）
inline const Setting<int> csaTurnPreference{"CsaGameSettings/turnPreference", 0};

} // namespace NetworkSettings

#endif // NETWORKSETTINGS_H
