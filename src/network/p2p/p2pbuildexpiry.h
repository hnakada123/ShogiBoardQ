#ifndef P2PBUILDEXPIRY_H
#define P2PBUILDEXPIRY_H

/// @file p2pbuildexpiry.h
/// @brief 実験版のビルドの使用期限（実装仕様 3.1節。期限のあるネットワークだけに掛かる）

#include <QtGlobal>

/**
 * 期限を過ぎたビルドは、限定試験用と公開用のネットワークへ参加しない（ローカル検証用のネットワークと、保存した記録の
 * 確認・検証は使える）。起点と日数は通信プロセスと同じ値を、CMake が p2pbuildinfo.h に書く。
 */
namespace P2pBuildExpiry {

/// 使用期限（UNIX秒）
qint64 expiresAt();
/// 時刻 nowSeconds（UNIX秒）に期限を過ぎているか
bool isExpiredAt(qint64 nowSeconds);
/// 今の OS の時計で期限を過ぎているか
bool isExpired();

} // namespace P2pBuildExpiry

#endif // P2PBUILDEXPIRY_H
