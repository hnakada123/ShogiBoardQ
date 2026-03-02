#ifndef TSUMESHOGIPOSITIONGENERATOR_H
#define TSUMESHOGIPOSITIONGENERATOR_H

/// @file tsumeshogipositiongenerator.h
/// @brief ランダム詰将棋候補局面生成クラスの定義

#include <QString>
#include <QRandomGenerator>
#include <QStringList>
#include <QList>
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

    /// スレッドプールで count 個の候補局面を並列生成する
    static QStringList generateBatch(const Settings& settings, int count, const CancelFlag& cancelFlag);

private:
    Settings m_settings;

    /// 駒種（生駒）
    enum PieceType { Pawn, Lance, Knight, Silver, Gold, Bishop, Rook, PieceTypeCount };

    /// 各駒種の最大数
    static constexpr int kMaxCounts[PieceTypeCount] = {18, 4, 4, 4, 4, 2, 2};

    /// 生駒のSFEN文字（先手大文字）
    static constexpr char kSfenChars[PieceTypeCount] = {'P', 'L', 'N', 'S', 'G', 'B', 'R'};

    /// 成駒のSFEN表記があるか（金は成れない）
    static constexpr bool kCanPromote[PieceTypeCount] = {true, true, true, true, false, true, true};

    /// 盤面配列（9×9, [rank][file] = SFEN文字 or '\0'）
    QChar m_board[9][9];

    /// 駒使用数カウンタ（各駒種の使用数を追跡）
    int m_usedCounts[PieceTypeCount] = {};

    /// 先手持駒カウンタ
    int m_attackerHand[PieceTypeCount] = {};

    QString generateOnce();
    void clearState();
    bool isOccupied(int file, int rank) const;
    bool placeOnBoard(int file, int rank, const QString& sfenPiece);
    PieceType randomPieceType();
    bool hasRemainingPiece(PieceType pt) const;
    void usePiece(PieceType pt);
    QString promotedSfen(PieceType pt, bool isAttacker) const;
    QString unpromatedSfen(PieceType pt, bool isAttacker) const;
    bool needsPromotion(PieceType pt, int rank, bool isAttacker) const;
    bool isInvalidPlacement(PieceType pt, int rank, bool isAttacker) const;
    bool isKingInCheck() const;
    QList<Piece> buildBoardData() const;
    static QChar promotedPieceChar(QChar base);
    QString buildSfen() const;
};

#endif // TSUMESHOGIPOSITIONGENERATOR_H
