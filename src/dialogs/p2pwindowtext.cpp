/// @file p2pwindowtext.cpp
/// @brief P2P対局の画面の文の実装

#include "p2pwindowtext.h"

#include <QStringList>

QString P2pWindowText::playerIdText(const QByteArray& playerKey)
{
    const QString hex = QString::fromLatin1(playerKey.toHex());
    QStringList groups;
    for (qsizetype i = 0; i < hex.size(); i += 8) groups << hex.mid(i, 8);
    return groups.join(QLatin1Char(' '));
}

QString P2pWindowText::shortPlayerId(const QByteArray& playerKey)
{
    return QString::fromLatin1(playerKey.toHex().left(12)) + QStringLiteral("…");
}

QString P2pWindowText::timeControlText(const P2p::TimeControlTerms& terms)
{
    const qint64 mainSeconds = static_cast<qint64>(terms.mainMs / 1000);
    const qint64 byoyomiSeconds = static_cast<qint64>(terms.byoyomiMs / 1000);
    QString main;
    if (mainSeconds % 60 == 0) main = tr("%1分").arg(mainSeconds / 60);
    else main = tr("%1秒").arg(mainSeconds);
    if (byoyomiSeconds == 0) return main;
    if (mainSeconds == 0) return tr("秒読み%1秒").arg(byoyomiSeconds);
    return tr("%1＋秒読み%2秒").arg(main).arg(byoyomiSeconds);
}

QString P2pWindowText::rulesText()
{
    return tr("ルール: 平手。人間どうしの対局で、検討・定跡の表示は使えません。入玉宣言はありません（双方の合意で引き分けにできます）。"
              "千日手は引き分け、連続王手の千日手は王手をかけた側の負け、512手で引き分けです。待ったはありません。");
}

QString P2pWindowText::recordContentsText()
{
    return tr("対局記録（証跡）には、双方のプレイヤーID・条件・対局を始めたおおよその日時・全手順と自己申告の消費時間・双方が"
              "プレイヤー鍵で確定した結果が残り、双方の手元に保存されます。表示名・IPアドレス・手元で付けた名前は残りません。"
              "アプリから公開・送信はしません。");
}

QString P2pWindowText::causeText(P2p::Cause cause)
{
    switch (cause) {
    case P2p::Cause::Resign: return tr("投了");
    case P2p::Cause::Checkmate: return tr("詰み");
    case P2p::Cause::Timeout: return tr("時間切れ");
    case P2p::Cause::Repetition: return tr("千日手");
    case P2p::Cause::PerpetualCheck: return tr("連続王手の千日手");
    case P2p::Cause::AgreedDraw: return tr("合意による引き分け");
    case P2p::Cause::MaxPly: return tr("512手到達");
    case P2p::Cause::AgreedAbort: return tr("合意による中断");
    }
    return QString();
}

QString P2pWindowText::outcomeText(P2p::Outcome outcome, P2p::Cause cause)
{
    QString result;
    switch (outcome) {
    case P2p::Outcome::BlackWins: result = tr("先手の勝ち"); break;
    case P2p::Outcome::WhiteWins: result = tr("後手の勝ち"); break;
    case P2p::Outcome::Draw: result = tr("引き分け"); break;
    case P2p::Outcome::Aborted: result = tr("中断"); break;
    }
    return tr("%1（%2）").arg(result, causeText(cause));
}

QString P2pWindowText::errorText(P2p::ErrorCode code)
{
    switch (code) {
    case P2p::ErrorCode::UnsupportedVersion: return tr("相手のアプリの版に対応していません。");
    case P2p::ErrorCode::WrongNetwork: return tr("相手は別のネットワークに参加しています。");
    case P2p::ErrorCode::Busy: return tr("相手は今、ほかの相手と交渉中か対局中です。");
    case P2p::ErrorCode::Expired: return tr("期限を過ぎました。");
    case P2p::ErrorCode::ConditionMismatch: return tr("条件・先後の希望・表示名が合いませんでした。");
    case P2p::ErrorCode::InvalidSignature: return tr("相手が名乗ったIDの持ち主であることを確かめられませんでした。");
    case P2p::ErrorCode::InvalidMove: return tr("合法でない手を受け取りました。");
    case P2p::ErrorCode::StateConflict: return tr("双方の局面が食い違いました。");
    case P2p::ErrorCode::ClockMismatch: return tr("相手の申告した消費時間が、こちらで測った時間と許容差を超えて食い違いました。");
    case P2p::ErrorCode::LimitExceeded: return tr("メッセージの数が上限を超えました。");
    case P2p::ErrorCode::StorageError: return tr("対局の記録を保存できませんでした。");
    case P2p::ErrorCode::Cancelled: return tr("取りやめました。");
    case P2p::ErrorCode::DisconnectLimit: return tr("1局の切断時間の合計が上限を超えました。");
    case P2p::ErrorCode::GameTimeLimit: return tr("1局の時間の上限に達しました。");
    case P2p::ErrorCode::ClockDiscontinuity: return tr("スリープや時計の変更で、計時の連続性を確かめられなくなりました。");
    }
    return QString();
}

QString P2pWindowText::settlementText(P2p::Settlement ending, std::optional<P2p::ErrorCode> error)
{
    switch (ending) {
    case P2p::Settlement::MutuallySigned: return tr("結果が一致し、双方で確定しました。");
    case P2p::Settlement::SignatureMissing: return tr("結果は一致しましたが、相手の確定が届きませんでした（相手の確定なしの記録）。");
    case P2p::Settlement::Aborted:
        return tr("対局は中断されました（結果未確定）。") + (error ? errorText(*error) : QString());
    case P2p::Settlement::Error:
        return tr("対局を続けられませんでした（結果未確定）。") + (error ? errorText(*error) : QString());
    case P2p::Settlement::Disconnected: return tr("回線を復旧できませんでした（通信中断・結果未確定）。");
    // アプリの終了での中断は、再起動の後にジャーナルから直した記録だけ（通知には出ない）
    case P2p::Settlement::Interrupted:
    case P2p::Settlement::None: break;
    }
    return QString();
}

QString P2pWindowText::settlementLabel(P2p::Settlement settlement)
{
    switch (settlement) {
    case P2p::Settlement::MutuallySigned: return tr("双方で確定");
    case P2p::Settlement::SignatureMissing: return tr("相手の確定なし");
    case P2p::Settlement::Aborted: return tr("中断（結果未確定）");
    case P2p::Settlement::Error: return tr("エラー（結果未確定）");
    case P2p::Settlement::Disconnected: return tr("通信中断（結果未確定）");
    case P2p::Settlement::Interrupted: return tr("アプリの終了で中断（結果未確定）");
    case P2p::Settlement::None: break;
    }
    return QString();
}

QString P2pWindowText::categoryText(P2p::EvidenceReport::Category category)
{
    switch (category) {
    case P2p::EvidenceReport::Category::MutuallySigned: return tr("双方で確定");
    case P2p::EvidenceReport::Category::DeterminedByRecord: return tr("棋譜から決まる結果");
    case P2p::EvidenceReport::Category::Unconfirmed: return tr("結果未確定");
    }
    return QString();
}

QString P2pWindowText::startFailureText(P2pLobbyService::StartFailure failure)
{
    switch (failure) {
    case P2pLobbyService::StartFailure::None: break;
    case P2pLobbyService::StartFailure::Locked: return tr("ほかの ShogiBoardQ が P2P対局を使っています。");
    case P2pLobbyService::StartFailure::NetworkNotAllowed: return tr("このビルドはそのネットワークに参加できません。");
    case P2pLobbyService::StartFailure::BuildExpired:
        return tr("このビルドは使用期限を過ぎたので、「インターネット」のネットワークに参加できません。新しい版に更新してください。"
                  "保存した記録の確認と、「ローカル」のネットワークは使えます。");
    case P2pLobbyService::StartFailure::NoIdentity: return tr("プレイヤー鍵を読み込んでいません。");
    case P2pLobbyService::StartFailure::AutomationEnabled:
        return tr("自動化 API を有効にして起動したときは、P2P対局を使えません。");
    case P2pLobbyService::StartFailure::Busy:
        return tr("対局・検討などの最中は接続できません。終えてから接続してください。");
    }
    return QString();
}

QString P2pWindowText::peerFailureText(PeerProcess::Failure failure, const QString& detail)
{
    switch (failure) {
    case PeerProcess::Failure::None: break;
    case PeerProcess::Failure::NotFound: return tr("通信プロセス（shogiboardq-peer）が見つかりません。");
    case PeerProcess::Failure::UnsupportedOs:
        return tr("この OS では、通信プロセスの権限を制限できないので、P2P対局を使えません。");
    case PeerProcess::Failure::SandboxFailed: {
        const QString text = tr("通信プロセスの権限を制限できなかったので、P2P対局を使えません。制限なしでは続けません。");
        return detail.isEmpty() ? text : text + QLatin1Char('\n') + detail;
    }
    case PeerProcess::Failure::HelloMismatch:
    case PeerProcess::Failure::ProtocolViolation:
        return tr("通信プロセスとのやり取りに異常があったので、通信プロセスを止めました。");
    case PeerProcess::Failure::Crashed:
        return tr("通信プロセスが異常終了しました。招待・交渉は取り消し、対局中なら中断（結果未確定）になります。");
    }
    return QString();
}
