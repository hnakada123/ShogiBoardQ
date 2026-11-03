#ifndef TSUMESHOGIPOSITIONGENERATOR_H
#define TSUMESHOGIPOSITIONGENERATOR_H

/// @file tsumeshogipositiongenerator.h
/// @brief ランダム詰将棋候補局面生成クラスの定義

#include <QString>
#include <QRandomGenerator>
#include <QStringList>
#include <QList>
#include <array>
#include "shogiposition.h"
#include "shogitypes.h"
#include "threadtypes.h"

/**
 * @brief ランダムな詰将棋候補局面をSFEN文字列として生成する
 *
 * 攻方（先手）が玉方（後手）の玉を詰ます局面を生成する。
 * 攻方の玉は配置しない（詰将棋の慣例）。
 * 未使用駒は受方持駒に追加する（詰将棋の慣例）。
 */
class TsumeshogiPositionGenerator
{
public:
    struct Settings {
        int maxAttackPieces = 4;     ///< 攻め駒上限（盤上+持駒合計）
        int maxDefendPieces = 1;     ///< 守り駒上限（玉以外）
        int attackRange = 3;         ///< 玉からの攻め駒配置範囲
        bool addRemainingToDefenderHand = true; ///< 未使用駒を受方持駒に入れる
    };

    void setSettings(const Settings& s);
    QString generate();

    /// SFEN局面で守方（後手）玉に王手がかかっているか（攻方玉がなくても判定できる）
    static bool isDefenderKingInCheck(const QString& sfen);

    /// count 個の候補局面を生成する（cancelFlag が立てられた時点で中断し、生成済み分だけ返す）
    /// 局面生成はエンジン探索に比べ十分軽いため逐次で行う。呼び出し側がワーカースレッドで実行する想定
    static QStringList generateBatch(const Settings& settings, int count, const CancelFlag& cancelFlag);

private:
    Settings m_settings;

    /// 置く駒の種類の数（成る前の駒の PieceKind の歩〜飛。玉は別に置く）。枚数は1組の駒
    /// （PieceRules::setCount）、成れるかは isPromotable、行き所のない段は PieceRules::isDeadSquare で決める
    static constexpr int kPlacedKindCount = static_cast<int>(PieceKind::King);

    /// 局面（盤と持駒）。file・rank は 0〜8 で、盤の筋・段の 1〜9 にあたる
    Position m_board;

    /// 駒の種類（PieceKind の歩〜飛）ごとの使った数（盤上の両者の駒と攻方の持駒）
    std::array<int, kPlacedKindCount> m_usedCounts{};

    QString generateOnce();
    void clearState();
    bool isOccupied(int file, int rank) const;
    bool hasUnpromotedPawnInFile(int file, bool isAttacker) const;
    void placeOnBoard(int file, int rank, Piece piece);
    static PieceKind randomPieceKind();
    bool hasRemainingPiece(PieceKind kind) const;
    void usePiece(PieceKind kind);
    void addToAttackerHand(PieceKind kind);
    /// 種類 kind の駒（isAttacker なら先手、そうでなければ後手。promote なら成駒）
    static Piece pieceOf(PieceKind kind, bool isAttacker, bool promote);
    /// 成らずに置くと行き所のない段か（rank は 0〜8）
    static bool needsPromotion(PieceKind kind, int rank, bool isAttacker);
    bool isKingInCheck() const;
    QString buildSfen();
};

#endif // TSUMESHOGIPOSITIONGENERATOR_H
