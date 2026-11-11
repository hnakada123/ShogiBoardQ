#ifndef P2PWINDOWTEXT_H
#define P2PWINDOWTEXT_H

/// @file p2pwindowtext.h
/// @brief P2P対局の画面の文（プレイヤーID・持ち時間・ルール・記録に残る内容・終わり方・記録の区分）

#include <QCoreApplication>
#include <QString>

#include <optional>

#include "p2pevidenceverifier.h"
#include "p2pframes.h"
#include "p2pmatch.h"

/**
 * @brief P2P対局の画面で共通の文
 *
 * 「不正防止済み」「時間切れが証明された」のような表示はしない。署名拒否・接続不能・保存の失敗には別々の説明を出す。
 */
class P2pWindowText
{
    Q_DECLARE_TR_FUNCTIONS(P2pWindowText)

public:
    /// プレイヤーID（公開鍵の16進。省略しない。8文字ごとに区切る）
    static QString playerIdText(const QByteArray& playerKey);
    /// 一覧に出す短いプレイヤーID（先頭12文字。同一性の判断には使わせない）
    static QString shortPlayerId(const QByteArray& playerKey);
    static QString timeControlText(const P2p::TimeControlTerms& terms);
    static QString rulesText();
    static QString recordContentsText();
    static QString outcomeText(P2p::Outcome outcome, P2p::Cause cause);
    static QString causeText(P2p::Cause cause);
    static QString errorText(P2p::ErrorCode code);
    static QString endingText(P2p::MatchEnding ending, std::optional<P2p::ErrorCode> error);
    /// 履歴に保存したローカルの終局理由の語
    static QString localEndingText(const QString& localEnding);
    static QString categoryText(P2p::EvidenceReport::Category category);
};

#endif // P2PWINDOWTEXT_H
