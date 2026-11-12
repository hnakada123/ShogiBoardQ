#ifndef P2PSETTLEMENT_H
#define P2PSETTLEMENT_H

/// @file p2psettlement.h
/// @brief P2P対局の結果の確定のしかた（ローカルの終局理由）と、ジャーナル・履歴に書く語

#include <QString>
#include <QStringView>

#include <optional>

namespace P2p {

/**
 * @brief 結果の確定のしかた（記録の区分とは別の、ローカルの終局理由）
 *
 * 将棋の終局の原因は FinalResult の Cause で、棋譜の終局の行は原因から MatchEnding（matchending.h）で決める。
 */
enum class Settlement {
    None,               ///< 対局中
    MutuallySigned,     ///< 結果が一致し、双方が最終結果に署名した
    SignatureMissing,   ///< 結果は一致したが、相手の署名が届かなかった
    Aborted,            ///< どちらかが ABORT した（結果未確定）
    Error,              ///< 検証の失敗・制限超過など（結果未確定）
    Disconnected,       ///< 回線を復旧できなかった（通信中断・結果未確定）
    Interrupted,        ///< アプリが終わりの件を書く前に止まった（再起動の後にジャーナルから直した記録。結果未確定）
};

/// ジャーナルの終わりの件と履歴の情報に書く語（この表だけが語を決める）
inline QString settlementName(Settlement settlement)
{
    switch (settlement) {
    case Settlement::None: return QStringLiteral("playing");
    case Settlement::MutuallySigned: return QStringLiteral("mutually_signed");
    case Settlement::SignatureMissing: return QStringLiteral("signature_missing");
    case Settlement::Aborted: return QStringLiteral("aborted");
    case Settlement::Error: return QStringLiteral("error");
    case Settlement::Disconnected: return QStringLiteral("disconnected");
    case Settlement::Interrupted: return QStringLiteral("interrupted");
    }
    return QStringLiteral("playing");
}

/// settlementName の語から戻す（知らない語は nullopt）
inline std::optional<Settlement> settlementFromName(QStringView name)
{
    for (const Settlement s : {Settlement::None, Settlement::MutuallySigned, Settlement::SignatureMissing,
                               Settlement::Aborted, Settlement::Error, Settlement::Disconnected, Settlement::Interrupted}) {
        if (name == settlementName(s)) return s;
    }
    return std::nullopt;
}

} // namespace P2p

#endif // P2PSETTLEMENT_H
