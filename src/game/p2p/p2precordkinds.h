#ifndef P2PRECORDKINDS_H
#define P2PRECORDKINDS_H

/// @file p2precordkinds.h
/// @brief 対局ジャーナルの件の形（開始合意・署名・状態・結果・矛盾した署名の証明・終わり。実装仕様 9章）

#include <QString>

#include "p2pprotocol.h"
#include "p2pevidence.h"

namespace P2p {

/**
 * @brief ジャーナルの1件（決定的CBORの配列。先頭がラベル）
 *
 * 対局の状態機械（Match）が書き、保存先（P2pRecordStore）が読み戻して、再起動の後に証跡へ直す。
 */
namespace RecordKinds {
CborValue start(const StartAgreement& agreement, const TimeControlTerms& terms, quint8 mySide,
                const QString& opponentDisplayName);
CborValue startSignature(quint8 side, const QByteArray& signature);
CborValue state(const SignedState& state);
CborValue resultDigest(const FinalResult& result);
CborValue resultSignature(quint8 side, const QByteArray& signature);
CborValue conflict(const GameState& first, const QByteArray& firstSignature, const GameState& second,
                   const QByteArray& secondSignature);
CborValue control(bool sent, const CborValue& controlPayload);
CborValue ending(const QString& localEnding);
} // namespace RecordKinds

} // namespace P2p

#endif // P2PRECORDKINDS_H
