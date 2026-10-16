#ifndef TSUMECANDIDATEJOB_H
#define TSUMECANDIDATEJOB_H

/// @file tsumecandidatejob.h
/// @brief 詰将棋の候補局面1つ分の処理（手数の確認・唯一解の検査・要らない駒の除去）

#include <QList>
#include <QString>
#include <QStringList>
#include <memory>

#include "matequeryloop.h"
#include "shogitypes.h"
#include "tsumeverificationjob.h"

/**
 * @brief 候補局面1つを、採択か棄却まで進める（引き出し型。Qt のイベントループを知らない）
 *
 * 段階は「手数の確認 → 元局面の検査 → i枚目を除く → 除いた局面の検査 → … → 採択／棄却」の順に進む。
 * - 手数の確認: ちょうど目標手数の詰みでなければ棄却
 * - 検査: 唯一解でなければ棄却
 * - 除去: 玉以外の駒を1枚ずつ玉方の持駒へ移し、目標手数のまま唯一解なら、除いた局面を新しい元局面にして
 *   最初の駒からやり直す。詰まなければその駒は要る。判定できない答えは「要る」の証明にならないので、
 *   候補全体を棄却する
 * 採択の結果（outcome）は、全ての1枚除去の判定を終えた局面と、その局面の検査が返した手順からしか作られない。
 */
class TsumeCandidateJob : public MateQuerySource
{
public:
    struct Settings {
        int targetMoves = 3;                     ///< 目標手数
        int timeoutMs = 5000;                    ///< 1回の詰み探索と、1局面の検査全体の時間
        bool allowFinalMoveAlternatives = true;  ///< 主手順の最終手の複数解を許容する
    };

    enum class Stage {
        CheckLength,     ///< 候補局面の詰みの手数を確かめる
        Verify,          ///< 候補局面の唯一解を検査する
        Trim,            ///< 駒を1枚除いた局面の詰みを確かめる
        VerifyTrimmed,   ///< 駒を1枚除いた局面の唯一解を検査する
        Accepted,
        Rejected,
    };

    struct Outcome {
        QString sfen;
        QStringList pv;
    };

    TsumeCandidateJob(const QString& sfen, const Settings& settings);
    ~TsumeCandidateJob() override;

    std::optional<MateQuery> nextQuery() override;
    void submit(const MateAnswer& answer, qint64 elapsedMs) override;
    bool isDone() const override { return m_stage == Stage::Accepted || m_stage == Stage::Rejected; }

    /// 打ち切る（棄却する）
    void abort();

    Stage stage() const { return m_stage; }
    /// 採択した局面と手順（Accepted のときだけ値がある）
    const std::optional<Outcome>& outcome() const { return m_outcome; }
    /// 手数の確認を通った（検査・除去に進んだ）
    bool passedLengthCheck() const { return m_passedLengthCheck; }
    /// 検査・除去で棄却した局面の数と、そのうち判定できずに棄却した数
    int rejectedCount() const { return m_rejected; }
    int inconclusiveCount() const { return m_inconclusive; }
    /// 試している除去（1始まり）と除去の候補の数（Trim・VerifyTrimmed のとき）
    int trimCandidate() const { return m_trimIndex + 1; }
    int trimCandidateCount() const { return static_cast<int>(m_trimCandidates.size()); }
    /// 検査中なら、その検査で送った問い合わせの数
    int verificationQueries() const { return m_verification ? m_verification->queries() : 0; }

    /// 正規化済みの先手攻方SFENから、玉以外の盤上駒・攻方持駒を1枚ずつ玉方持駒へ移した局面。
    /// 生成時の除去と保存済み問題集の再検査で同じ除去操作を使う
    static QStringList onePieceRemovedPositions(const QString& sfen);

private:
    struct TrimCandidate {
        enum Location { Board, Hand };
        Location location = Board;
        BoardSquare square;          ///< Board のとき: 除く駒のマス
        Piece piece = Piece::None;   ///< 除く駒（盤上は先後・成りを含む。持駒は先手の生駒）
    };

    static QList<TrimCandidate> enumerateRemovablePieces(const QString& sfen);
    static QString removePiece(const QString& sfen, const TrimCandidate& candidate);

    void startVerification(const QString& sfen, Stage stage);
    void finishVerification();
    void startTrimming(const QString& sfen, const QStringList& pv);
    /// 次に除く駒の局面を決める（事前選別で却下した除去は飛ばす）。無ければ採択する
    std::optional<MateQuery> nextTrimQuery();
    void onTrimAnswer(const MateAnswer& answer);
    void reject(bool inconclusive);

    Settings m_settings;
    Stage m_stage = Stage::CheckLength;
    QString m_candidateSfen;
    std::optional<Outcome> m_outcome;
    bool m_passedLengthCheck = false;
    int m_rejected = 0;
    int m_inconclusive = 0;

    std::unique_ptr<TsumeVerificationJob> m_verification;

    QString m_baseSfen;                     ///< 除去の元局面（検査を通った局面）
    QStringList m_basePv;                   ///< 元局面の検査が返した手順
    QList<TrimCandidate> m_trimCandidates;  ///< 元局面の除去の候補
    int m_trimIndex = 0;
    QString m_trimSfen;                     ///< 試している除去の局面
    bool m_trimProvenInTarget = false;      ///< 除いた局面が目標手数で詰むことを内蔵探索で確かめた
};

#endif // TSUMECANDIDATEJOB_H
