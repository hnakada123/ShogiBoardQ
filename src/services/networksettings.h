#ifndef NETWORKSETTINGS_H
#define NETWORKSETTINGS_H

/// @file networksettings.h
/// @brief CSAネットワーク設定の永続化
///
/// CSA通信対局・待機・ログに関する設定を提供します。
/// 呼び出し元: csawaitingdialog.cpp, csagamedialog.cpp, engineanalysistab.cpp（CSAログタブ共用）

#include <QSize>
#include <QString>

namespace NetworkSettings {

/// CSA通信ログのフォントサイズ（デフォルト: 10）
int csaLogFontSize();
void setCsaLogFontSize(int size);

/// CSA待機ダイアログのフォントサイズ（デフォルト: 10）
int csaWaitingDialogFontSize();
void setCsaWaitingDialogFontSize(int size);

/// CSA通信対局ダイアログのフォントサイズ（デフォルト: 10）
int csaGameDialogFontSize();
void setCsaGameDialogFontSize(int size);

/// CSA通信ログウィンドウのサイズ（デフォルト: 550x450）
QSize csaLogWindowSize();
void setCsaLogWindowSize(const QSize& size);

/// CSA通信対局ダイアログのウィンドウサイズ（デフォルト: 450x380）
QSize csaGameDialogSize();
void setCsaGameDialogSize(const QSize& size);

QSize csaWaitingDialogSize();
void setCsaWaitingDialogSize(const QSize& size);

// --- 通信対局ダイアログの詳細設定 ---

/// 対局中に空行（LF）を送って接続を維持するか（デフォルト: true）
bool csaKeepAliveEnabled();
void setCsaKeepAliveEnabled(bool enabled);

/// 空行キープアライブの間隔（秒、デフォルト: 40。仕様の下限は30）
int csaKeepAliveIntervalSec();
void setCsaKeepAliveIntervalSec(int seconds);

/// OS の TCP キープアライブを使うか（デフォルト: true）
bool csaTcpKeepAliveEnabled();
void setCsaTcpKeepAliveEnabled(bool enabled);

/// エンジンに渡す持時間から差し引く通信の余裕（秒、デフォルト: 1）
int csaThinkingMarginSec();
void setCsaThinkingMarginSec(int seconds);

/// 対局数の決め方（0: 1局、1: 指定回数、2: 無制限。デフォルト: 0）
int csaGameCountMode();
void setCsaGameCountMode(int mode);

/// 指定回数のときの対局数（デフォルト: 2）
int csaGameCount();
void setCsaGameCount(int count);

/// 終局後にサーバーが切断したら再接続して次の対局を待つか（デフォルト: false）
bool csaReconnectAfterGame();
void setCsaReconnectAfterGame(bool enabled);

/// 終局時に棋譜を自動保存するか（デフォルト: false）
bool csaAutoSaveEnabled();
void setCsaAutoSaveEnabled(bool enabled);

/// 自動保存先フォルダ（デフォルト: 空）
QString csaAutoSaveDir();
void setCsaAutoSaveDir(const QString& dir);

/// 自動保存の形式を拡張子で表す（kif/kifu/ki2/csa/jkf/usi。デフォルト: kif）
QString csaAutoSaveFormat();
void setCsaAutoSaveFormat(const QString& extension);

/// 後手のときに盤を反転するか（デフォルト: true）
bool csaAutoFlipBoard();
void setCsaAutoFlipBoard(bool enabled);

/// shogi-server のゲーム名（デフォルト: 空）
QString csaGameName();
void setCsaGameName(const QString& name);

/// shogi-server の手番の希望（0: 指定なし、1: 先手、2: 後手。デフォルト: 0）
int csaTurnPreference();
void setCsaTurnPreference(int preference);

} // namespace NetworkSettings

#endif // NETWORKSETTINGS_H
