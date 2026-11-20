#ifndef P2PFRIEND_H
#define P2PFRIEND_H

/// @file p2pfriend.h
/// @brief 友だち一覧の在席の知らせ（2人だけの秘密・協力ノードに預ける番号と鍵・暗号化。protocol/README.md 3.3節）
///
/// 友だちどうしは、互いのプレイヤーID を知っている。追加の受け渡しなしに、自分のプレイヤー鍵と相手の
/// プレイヤーID から、2人だけが計算できる秘密を作る。在席の知らせ（通信プロセスが作った、接続先と申し込み用の
/// トークン）を、その秘密から作った鍵で暗号化して協力ノードに預け、友だちだけが引き取って読める。
/// ソケットも GUI も使わない。秘密の値はログ・IPC・設定へ出さない。

#include <QByteArray>

#include <optional>

namespace P2p {

class SigningKey;

namespace Friend {

/// 友だちの数の上限
inline constexpr int kMaxFriends = 64;
/// 番号と鍵を作り直す間隔（秒）
inline constexpr quint64 kEpochSeconds = 600;
/// nonce・認証の値・預けるものの上限（協力ノードの待ち合わせと同じ）
inline constexpr int kNonceSize = 24;
inline constexpr int kTagSize = 16;
inline constexpr int kMaxBlobSize = 1024;

/// 在席の知らせ（暗号化する前の中身）
struct Presence {
    quint64 expiresAt = 0;   ///< 知らせの期限（UNIX 秒）
    QByteArray body;         ///< 通信プロセスが作った知らせ（接続先とトークン。対局・UI側は読まない）
};

/**
 * @brief 2人だけの秘密（32バイト）
 *
 * X25519(自分のプレイヤー鍵, 相手のプレイヤーID) を、2人のプレイヤーID と一緒にハッシュに通す。2人のどちらから
 * 計算しても同じ値になる。相手の ID が鍵交換に使えない点・自分の鍵が無効・自分自身なら空。
 */
QByteArray pairSecret(const SigningKey& mine, const QByteArray& theirPlayerKey);
/// X25519 の出力から 2人だけの秘密を作る（pairSecret の後半。2人の ID は順序によらない）
QByteArray pairSecretFromShared(const QByteArray& shared, const QByteArray& playerKeyA, const QByteArray& playerKeyB);

/// 時刻（UNIX 秒）の期間の番号
inline quint64 epochOf(quint64 unixSeconds) { return unixSeconds / kEpochSeconds; }

/// 協力ノードに預ける番号（32バイト）。depositor は預ける側のプレイヤーID
QByteArray slot(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor, quint64 epoch);
/// 預けるものを暗号化する鍵（32バイト）
QByteArray key(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor, quint64 epoch);

/// 在席の知らせを暗号化して、預けるもの（nonce と暗号文）にする。nonce が空なら乱数で作る。上限を超えれば空
QByteArray seal(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor, quint64 epoch,
                const Presence& presence, const QByteArray& nonce = {});
/// 預かったものを読む。鍵が違う・書き換えられている・形が合わなければ nullopt（中身を読む前に捨てる）
std::optional<Presence> open(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor,
                             quint64 epoch, const QByteArray& blob);

} // namespace Friend
} // namespace P2p

#endif // P2PFRIEND_H
