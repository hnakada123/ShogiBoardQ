#ifndef P2PRECORDKINDS_H
#define P2PRECORDKINDS_H

/// @file p2precordkinds.h
/// @brief 対局ジャーナルの件の形（開始合意・署名・状態・結果・矛盾した署名の証明・終わり。実装仕様 9章）

#include <QString>

#include <optional>
#include <vector>

#include "p2pevidence.h"
#include "p2pframes.h"
#include "p2pprotocol.h"
#include "p2psettlement.h"

namespace P2p {

/// ジャーナルを読み戻した結果（証跡と、証跡の外の情報）
struct JournalReplay {
    Evidence evidence;
    quint8 mySide = kSideBlack;
    QString opponentDisplayName;   ///< 相手が名乗った表示名（証跡には入れない）
    std::optional<Settlement> settlement;  ///< 終わりの件の確定のしかた（終わる前に止まったジャーナルでは nullopt）
    bool hasConflictProof = false; ///< 相手が同じ手数の異なる状態に署名した証明がある
};

/**
 * @brief ジャーナルの1件（決定的CBORの配列。先頭がラベル）と、その読み戻し
 *
 * 対局の状態機械（Match）が書き、保存先（P2pRecordStore）が replay で読み戻して、再起動の後に証跡へ直す。
 * 件のラベルと形はこのファイルだけが知る。
 */
namespace RecordKinds {
/// 開始の件。対局者の種別（rules）は、エンジンが指す対局のときだけ末尾に足す（双方人間の対局の件は、前の版と同じ形）
CborValue start(const StartAgreement& agreement, const RulesTerms& rules, const TimeControlTerms& terms, quint8 mySide,
                const QString& opponentDisplayName);
CborValue startSignature(quint8 side, const QByteArray& signature);
CborValue state(const SignedState& state);
CborValue resultDigest(const FinalResult& result);
CborValue resultSignature(quint8 side, const QByteArray& signature);
CborValue conflict(const GameState& first, const QByteArray& firstSignature, const GameState& second,
                   const QByteArray& secondSignature);
CborValue control(bool sent, const CborValue& controlPayload);
CborValue ending(Settlement settlement);

/// 書いた後すぐにストレージへ同期する件か。相手から受けた制御メッセージは、相手の振る舞いの記録で、自分の署名や
/// 送る前の約束を含まないので、次に同期する件と一緒に同期すればよい（1件ごとに同期すると、相手が制御メッセージを
/// 続けて送るだけで、GUI のスレッドを同期の待ちで止められる。監査 2026-11-21 S7）
bool needsImmediateSync(const CborValue& record);

/// ジャーナルの件を書いた順に読み、証跡を組み立てる（形の合わない件は飛ばす。開始の件が無ければ nullopt）。
/// 同じ手数の状態は後の件（署名が足されたもの）で置き換える
std::optional<JournalReplay> replay(const std::vector<CborValue>& records);
} // namespace RecordKinds

} // namespace P2p

#endif // P2PRECORDKINDS_H
