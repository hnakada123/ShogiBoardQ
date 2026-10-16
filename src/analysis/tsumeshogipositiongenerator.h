#ifndef TSUMESHOGIPOSITIONGENERATOR_H
#define TSUMESHOGIPOSITIONGENERATOR_H

/// @file tsumeshogipositiongenerator.h
/// @brief ランダム詰将棋候補局面生成クラスの定義

#include <QString>
#include <QRandomGenerator>
#include <QStringList>
#include <QList>
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

    /// 駒種（生駒）
    enum PieceType { Pawn, Lance, Knight, Silver, Gold, Bishop, Rook, PieceTypeCount };

    /// 各駒種の最大数
    static constexpr int kMaxCounts[PieceTypeCount] = {18, 4, 4, 4, 4, 2, 2};

    /// 駒種の先手の生駒
    static constexpr Piece kBlackPieces[PieceTypeCount] = {
        Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
        Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook};

    /// 成駒があるか（金は成れない）
    static constexpr bool kCanPromote[PieceTypeCount] = {true, true, true, true, false, true, true};

    /// 局面（盤と持駒）。file・rank は 0〜8 で、盤の筋・段の 1〜9 にあたる
    Position m_board;

    /// 駒使用数カウンタ（各駒種の使用数。盤上の両者の駒と攻方の持駒）
    int m_usedCounts[PieceTypeCount] = {};

    QString generateOnce();
    void clearState();
    bool isOccupied(int file, int rank) const;
    bool hasUnpromotedPawnInFile(int file, bool isAttacker) const;
    void placeOnBoard(int file, int rank, Piece piece);
    PieceType randomPieceType();
    bool hasRemainingPiece(PieceType pt) const;
    void usePiece(PieceType pt);
    void addToAttackerHand(PieceType pt);
    /// 駒種の駒（isAttacker なら先手、そうでなければ後手。promote なら成駒）
    static Piece pieceOf(PieceType pt, bool isAttacker, bool promote);
    bool needsPromotion(PieceType pt, int rank, bool isAttacker) const;
    bool isKingInCheck() const;
    QString buildSfen();
};

#endif // TSUMESHOGIPOSITIONGENERATOR_H
