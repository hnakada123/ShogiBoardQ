#ifndef SFENPOSITIONTRACER_H
#define SFENPOSITIONTRACER_H

/// @file sfenpositiontracer.h
/// @brief USI手列を順に適用して各手後のSFEN局面を生成する簡易トレーサ

#include <QString>
#include <QStringList>
#include <QPoint>
#include <QList>
#include <memory>
#include "shogimove.h"

class ShogiBoard;

/**
 * @brief USI手列を順に適用し、各手後のSFEN局面文字列を生成する簡易トレーサ
 *
 * 平手初期局面または任意のSFENから開始し、USI形式の手を順に適用して
 * 局面列やShogiMove列を構築する。合法手チェックは行わない。
 * 局面は盤（ShogiBoard）で持ち、指し手の読み取りと SFEN の書き出しは盤と同じものを使う。
 */
class SfenPositionTracer {
public:
    SfenPositionTracer();
    ~SfenPositionTracer();
    SfenPositionTracer(const SfenPositionTracer& other);
    SfenPositionTracer& operator=(const SfenPositionTracer& other);

    void resetToStartpos();

    /// USI一手（例: "7g7f", "P*5e", "2b3c+"）を適用する
    [[nodiscard]] bool applyUsiMove(const QString& usi);

    /// 現在局面のSFEN文字列を返す（例: "lnsgkgsnl/... b - 1"）
    QString toSfenString() const;

    /// USIのpositionコマンドから、moves適用後の4フィールドSFENを生成する
    static QString sfenFromPositionCommand(const QString& positionCommand);

#ifdef SHOGIBOARDQ_TESTING
    /// テスト用: 手列を順に適用し、各手の直後局面のSFENを配列で返す
    QStringList generateSfensForMoves(const QStringList& usiMoves);
#endif

    /// 4フィールド形式のSFENから現在局面をセットする
    [[nodiscard]] bool setFromSfen(const QString& sfen);

    bool blackToMove() const { return m_blackToMove; }

    /// 指定マスの駒（file・rank とも 1..9。範囲外は None）
    Piece pieceAt(int file, int rank) const;

    /// 指定マスのトークンを返す（file: 1..9, rank: 'a'..'i'、空なら空文字列。成駒は "+P" の形）
    QString tokenAtFileRank(int file, QChar rankLetter) const;

    /// 指定マスのトークンを返す（file: 1..9, row: 0..8 が 'a'..'i'。範囲外は空文字列）
    QString tokenAt(int file, int row) const;

    // --- 局面列構築ユーティリティ ---

    /// 初期SFENとUSI手列から局面列を構築する（hasTerminal==trueのとき終局用に末尾を複製）
    static QStringList buildSfenRecord(const QString& initialSfen,
                                       const QStringList& usiMoves,
                                       bool hasTerminal);

    /// 初期SFENとUSI手列から可視化用のShogiMove列を構築する
    static QList<ShogiMove> buildGameMoves(const QString& initialSfen,
                                             const QStringList& usiMoves);

private:
    std::unique_ptr<ShogiBoard> m_board;  ///< 局面（盤と駒台）
    bool m_blackToMove = true;            ///< 手番（true=先手b, false=後手w）
    int m_plyNext = 1;                    ///< 次の手数（SFENの第4フィールド）
};

#endif // SFENPOSITIONTRACER_H
