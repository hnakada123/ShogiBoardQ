#ifndef PEERIPC_H
#define PEERIPC_H

/// @file peeripc.h
/// @brief 通信プロセスとの IPC のメッセージの形（4バイト長＋JSON）と、受けたメッセージの検査（protocol/ipc.md）

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace PeerIpc {

/// IPC 1件の上限（128 KiB）
inline constexpr qsizetype kMaxMessageBytes = 128 * 1024;
inline constexpr int kProtocolVersion = 1;

/// 長さの前置きを付けたメッセージ
QByteArray frame(const QJsonObject& message);

/// 受けたメッセージの種類
struct Incoming {
    enum class Kind { Response, Error, Notification };
    Kind kind = Kind::Notification;
    quint64 generation = 0;
    quint64 requestId = 0;     ///< 応答・エラー
    QString name;              ///< 通知の名前
    QJsonObject body;          ///< result・error・通知の params
};

/**
 * @brief 受けた JSON を読み、外枠（版・世代・要求番号・知らないフィールド）を確かめる
 *
 * 中身（result・params）は名前ごとの検査（checkResult・checkNotification）で確かめる。
 */
std::optional<Incoming> parse(const QByteArray& json, QString* error = nullptr);

/// 10進の文字列の符号なし64ビット整数（先頭の 0・符号・空白を許さない）
std::optional<quint64> parseU64(const QJsonValue& value);

/**
 * @brief 要求 method の応答の result が、その要求の形に合うか
 *
 * 知らないフィールド・足りないフィールド・型や範囲の合わない値を拒否する（protocol/ipc.md 3章）。
 */
bool checkResult(const QString& method, const QJsonObject& result);

/// 通知の params が、その通知の形に合うか（protocol/ipc.md 4章）
bool checkNotification(const QString& name, const QJsonObject& params);

/// 募集1件（offersChanged の added の要素）の形
bool checkOffer(const QJsonObject& offer);

} // namespace PeerIpc

#endif // PEERIPC_H
