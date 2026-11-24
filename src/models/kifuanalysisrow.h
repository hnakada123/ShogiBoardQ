#ifndef KIFUANALYSISROW_H
#define KIFUANALYSISROW_H

/// @file kifuanalysisrow.h
/// @brief 棋譜解析の結果の表の1行（1局面の解析結果）

#include <QString>
#include <optional>

#include "pvtext.h"
#include "sfenutils.h"
#include "usiscore.h"

/**
 * @brief 棋譜解析の結果の表の1行（作った後は変えない値）
 *
 * 評価値は先手から見た値で持つ。表示する文字列（表記の設定に合わせた手・読み筋、「mate 3」・形勢）は
 * 表のモデル（KifuAnalysisListModel）が作る。
 */
struct KifuAnalysisRow {
    /// 指し手の判定（評価値損失や詰み逃しによる分類）
    enum class MoveQuality {
        None,        ///< 通常の手・開始局面・定跡・比較不可
        Inaccuracy,  ///< 緩手（?!）：評価値損失 150〜299
        Mistake,     ///< 疑問手（?）：評価値損失 300〜499
        Blunder,     ///< 悪手（??）：評価値損失 500 以上、または詰み逃し・被詰み転落
    };

    static constexpr int kInaccuracyLoss = 150;
    static constexpr int kMistakeLoss = 300;
    static constexpr int kBlunderLoss = 500;

    int ply = -1;                             ///< 解析した局面の手数（sfen の局面）
    QString move;                             ///< 指した手（棋譜の表記 RecordEntry::displayText。開始局面の行は空）
    bool branch = false;                      ///< 指した手に分岐がある（表で棋譜欄と同じ印を付ける）
    QString lastUsiMove;                      ///< 指した手（USI。読み筋の盤で強調する）
    QString beforeSfen;                       ///< 指す前の局面
    QString sfen;                             ///< 指した後の局面（読み筋の始まり）
    UsiScore evaluation;                      ///< 先手から見た評価値（定跡・未取得は None）
    UsiScore previousEvaluation;              ///< 直前の解析局面の評価値（先手視点。詰み逃し判定に使う）
    std::optional<int> evaluationDifference;  ///< 直前の解析局面からの評価値の増減（比べられなければ nullopt）
    bool book = false;                        ///< 定跡（読み筋の代わりに定跡の印を出す）
    PvText pv;                                ///< 読み筋（sfen の局面の上で指した手。定跡は空）
    int depth = -1;                           ///< 読み筋の探索深さ（無ければ -1）
    QString candidateMove;                    ///< 候補手（直前の行の読み筋の最初の手。漢字表記）
    QString candidateUsi;                     ///< 候補手（USI）
    QString candidateSfen;                    ///< 候補手を指す局面

    /// 指した手が直前の局面の候補手と同じか
    bool matchesCandidate() const
    {
        return hasComparableCandidate() && lastUsiMove == candidateUsi;
    }

    /// 候補手と比較できる手か（一致率の分母に含めるか）
    bool hasComparableCandidate() const
    {
        return ply > 0 && !book && !lastUsiMove.isEmpty() && !candidateUsi.isEmpty()
            && !beforeSfen.isEmpty() && beforeSfen == candidateSfen;
    }

    /// この手を指した手番（開始局面は nullopt）
    std::optional<Turn> mover() const
    {
        if (ply <= 0) return std::nullopt;
        if (!beforeSfen.isEmpty()) return SfenUtils::sideToMove(beforeSfen);
        return (ply % 2 == 1) ? Turn::Black : Turn::White;
    }

    /// 手番側から見た評価値損失（正が悪化、負が改善。比較できないときは nullopt）
    std::optional<int> centipawnLoss() const
    {
        if (ply <= 0 || book || !evaluationDifference) return std::nullopt;
        const std::optional<Turn> side = mover();
        if (!side) return std::nullopt;
        return *side == Turn::Black ? -(*evaluationDifference) : *evaluationDifference;
    }

    /// 評価値損失と詰み逃しから判定した指し手の質
    MoveQuality moveQuality() const
    {
        if (ply <= 0 || book || matchesCandidate()) return MoveQuality::None;
        const std::optional<Turn> side = mover();
        if (!side) return MoveQuality::None;
        const int sideSign = *side == Turn::Black ? 1 : -1;

        // 詰み絡みの大悪手（勝ちの詰みを逃した／詰みでない局面から詰まされる局面になった）
        if (!previousEvaluation.isNone() && !evaluation.isNone()) {
            const bool hadOwnMate = previousEvaluation.isMate() && previousEvaluation.mateSign() == sideSign;
            const bool hasOwnMate = evaluation.isMate() && evaluation.mateSign() == sideSign;
            if (hadOwnMate && !hasOwnMate) return MoveQuality::Blunder;

            const bool hadOpponentMate = previousEvaluation.isMate() && previousEvaluation.mateSign() == -sideSign;
            const bool hasOpponentMate = evaluation.isMate() && evaluation.mateSign() == -sideSign;
            if (!hadOpponentMate && hasOpponentMate) return MoveQuality::Blunder;
        }

        const std::optional<int> loss = centipawnLoss();
        if (!loss) return MoveQuality::None;
        if (*loss >= kBlunderLoss) return MoveQuality::Blunder;
        if (*loss >= kMistakeLoss) return MoveQuality::Mistake;
        if (*loss >= kInaccuracyLoss) return MoveQuality::Inaccuracy;
        return MoveQuality::None;
    }
};

#endif // KIFUANALYSISROW_H
