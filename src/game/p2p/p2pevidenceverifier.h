#ifndef P2PEVIDENCEVERIFIER_H
#define P2PEVIDENCEVERIFIER_H

/// @file p2pevidenceverifier.h
/// @brief 証跡の検証（署名・手順・合法性・時計の算術・ハッシュ・記録の区分。実装仕様 8章・9章）

#include <QByteArray>
#include <QString>

#include <optional>

#include "p2pevidence.h"
#include "p2pgamerules.h"

namespace P2p {

/**
 * @brief 証跡を検証した結果
 *
 * 「署名が一致」「棋譜がルールに適合」「自己申告時間の計算が一致」「結果が棋譜から決まる」を別々に示す。
 * 実際の思考時間・人間が指したこと・対局した日時は確かめられないので、項目を持たない。
 */
struct EvidenceReport {
    /// 記録の区分（実装仕様 8章）
    enum class Category {
        MutuallySigned,      ///< 両方のプレイヤー鍵が同じ FinalResult に署名している
        DeterminedByRecord,  ///< 結果への署名はそろわないが、最後の両署名付き State が規則上の終局
        Unconfirmed,         ///< どちらでもない
    };

    bool formatValid = false;          ///< 形・上限・条件の配列が正しい
    QString error;                     ///< 最初に見つかった問題（なければ空）

    QByteArray gameId;
    QByteArray networkId;
    QString networkName;               ///< このビルドが知るネットワークならその名前
    QByteArray blackPlayerKey;
    QByteArray whitePlayerKey;
    TimeControlTerms timeControl;

    bool conditionsMatch = false;      ///< 条件の配列のハッシュが開始合意と一致
    bool blackStartSigned = false;     ///< 先手の開始署名が正しい
    bool whiteStartSigned = false;

    int stateCount = 0;                ///< 証跡にある状態の数
    int lastMutualPly = -1;            ///< 両方の承認がそろった最後の手数（State0 もなければ -1）
    bool signaturesValid = false;      ///< 状態の承認の署名がすべて正しい（空の欄は除く）
    bool recordLegal = false;          ///< 両署名付きの手順が連続し、合法手だけからなる
    bool clockArithmeticValid = false; ///< 自己申告の消費時間から計算した残り時間が一致する

    std::optional<GameRules::Terminal> terminal;  ///< 両署名付きの手順の最後が規則上の終局ならその結果

    std::optional<FinalResult> result;
    QByteArray resultDigest;
    bool resultMatchesRecord = false;  ///< 最終結果が最後の両署名付き State と Transcript を指す
    bool blackResultSigned = false;
    bool whiteResultSigned = false;
    bool resultConflictsWithRecord = false;  ///< 署名された結果が棋譜から決まる結果と食い違う

    Category category = Category::Unconfirmed;

    /// すべての検査に通ったか（区分は別。結果未確定でも、ここまでの記録は正しいことがある）
    bool isValid() const
    {
        return formatValid && conditionsMatch && blackStartSigned && whiteStartSigned && signaturesValid
            && recordLegal && clockArithmeticValid && !resultConflictsWithRecord
            && (!result || resultMatchesRecord);
    }
};

namespace EvidenceVerifier {

/// 証跡のバイト列を検証する（上限の確認から。ネットワークは使わない）
EvidenceReport verify(QByteArrayView bytes);
/// 読んだ証跡を検証する
EvidenceReport verify(const Evidence& evidence);

} // namespace EvidenceVerifier

} // namespace P2p

#endif // P2PEVIDENCEVERIFIER_H
