#ifndef P2PEVIDENCE_H
#define P2PEVIDENCE_H

/// @file p2pevidence.h
/// @brief 検証用証跡（.sbqmatch）の値と、その読み書き（実装仕様 9章）

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <optional>
#include <vector>

#include "p2pprotocol.h"

namespace P2p {

/**
 * @brief 証跡（開始合意・条件・全状態と両署名・最終結果とその署名）
 *
 * 表示名・IP アドレス・PeerId そのもの・招待トークン・手元で付けた名前は入れない。
 */
struct Evidence {
    /// ファイル全体の上限
    static constexpr qsizetype kMaxBytes = 8 * 1024 * 1024;
    /// 状態の数の上限（State0 から512手目まで）
    static constexpr std::size_t kMaxStates = kMaxPly + 1;
    /// 証跡ファイルの拡張子
    static constexpr const char* kSuffix = "sbqmatch";

    StartAgreement agreement;
    QByteArray blackStartSignature;
    QByteArray whiteStartSignature;
    RulesTerms rules;   ///< 先手・後手の対局者の種別（開始合意の rulesHash と一致すること）
    TimeControlTerms timeControl;
    std::vector<SignedState> states;
    std::optional<FinalResult> result;
    QByteArray blackResultSignature;
    QByteArray whiteResultSignature;

    QByteArray gameId() const { return agreement.gameId(); }

    CborValue toCbor() const;
    QByteArray encode() const { return Cbor::encode(toCbor()); }
    /// 上限・決定的CBOR・形を確かめて読む（署名と手順は確かめない。それは EvidenceVerifier）
    static std::optional<Evidence> decode(QByteArrayView bytes, QString* error = nullptr);
};

} // namespace P2p

#endif // P2PEVIDENCE_H
