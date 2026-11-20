#ifndef P2PWINDOWTEXT_H
#define P2PWINDOWTEXT_H

/// @file p2pwindowtext.h
/// @brief P2P対局の画面の文（プレイヤーID・持ち時間・ルール・記録に残る内容・終わり方・記録の区分・使えない理由）

#include <QCoreApplication>
#include <QString>

#include <optional>

#include "p2pevidenceverifier.h"
#include "p2pframes.h"
#include "p2plobbyservice.h"
#include "p2pmatch.h"
#include "peerprocess.h"

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
    /// 対局者の種別（「人間」「エンジン」）
    static QString playerKindText(P2p::PlayerKind kind);
    /// 先手・後手の対局者の種別（「先手: 人間、後手: エンジン」）
    static QString playerKindsText(const P2p::RulesTerms& rules);
    /// ルールの説明（rules は先手・後手の対局者の種別。エンジンが指す対局かどうかで最初の文が変わる）
    static QString rulesText(const P2p::RulesTerms& rules);
    static QString recordContentsText();
    static QString outcomeText(P2p::Outcome outcome, P2p::Cause cause);
    /// 自分から見た結果（「勝ち（投了）」「負け（時間切れ）」「引き分け（千日手）」「中断（合意による中断）」）。
    /// 記録の一覧で、先後と「先手の勝ち」を見比べなくても勝ち負けが分かるようにする
    static QString myOutcomeText(P2p::Outcome outcome, P2p::Cause cause, bool iAmBlack);
    static QString causeText(P2p::Cause cause);
    static QString errorText(P2p::ErrorCode code);
    static QString settlementText(P2p::Settlement ending, std::optional<P2p::ErrorCode> error);
    /// 記録の表に出す、結果の確定のしかたの短い語（通知の文は settlementText）
    static QString settlementLabel(P2p::Settlement settlement);
    static QString categoryText(P2p::EvidenceReport::Category category);
    /// P2P対局を使い始められない理由（None なら空）
    static QString startFailureText(P2pLobbyService::StartFailure failure);
    /// 「招待を受ける」の欄の文字列が招待文字列か（そうでなければ合言葉として扱う）
    static bool isInviteString(const QString& input);
    /// 「招待を受ける」の欄の文字列を、通信プロセスへ渡す形にする。招待文字列は前後の空白を除くだけ。合言葉は
    /// 全角の英数字を半角にし、英数字だけを大文字で残す（区切り・空白・全角のハイフンは、どれも読み飛ばす）
    static QString inviteInput(const QString& input);
    /// 通信プロセスが使えなくなった理由（None なら空。権限の制限の失敗だけ、診断の detail を添える）
    static QString peerFailureText(PeerProcess::Failure failure, const QString& detail);
};

#endif // P2PWINDOWTEXT_H
