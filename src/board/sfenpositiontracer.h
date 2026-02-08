#ifndef SFENPOSITIONTRACER_H
#define SFENPOSITIONTRACER_H

/// @file sfenpositiontracer.h
/// @brief USI手列を順に適用して各手後のSFEN局面を生成する簡易トレーサ

#include <QString>
#include <QStringList>
#include <array>
#include <QPoint>
#include <QVector>
#include "shogimove.h"

/**
 * @brief USI手列を順に適用し、各手後のSFEN局面文字列を生成する簡易トレーサ
 * @todo remove コメントスタイルガイド適用済み
 *
 * 平手初期局面または任意のSFENから開始し、USI形式の手を順に適用して
 * 局面列やShogiMove列を構築する。合法手チェックは行わない。
 */
class SfenPositionTracer {
public:
    SfenPositionTracer();

    void resetToStartpos();

    /// USI一手（例: "7g7f", "P*5e", "2b3c+"）を適用する
    bool applyUsiMove(const QString& usi);

    /// 現在局面のSFEN文字列を返す（例: "lnsgkgsnl/... b - 1"）
    QString toSfenString() const;

    /// 手列を順に適用し、各手の直後局面のSFENを配列で返す
    QStringList generateSfensForMoves(const QStringList& usiMoves);

    /// 4フィールド形式のSFENから現在局面をセットする
    bool setFromSfen(const QString& sfen);

    bool blackToMove() const { return m_blackToMove; }

    /// 指定マスのトークンを返す（file: 1..9, rank: 'a'..'i'、空なら空文字列）
    QString tokenAtFileRank(int file, QChar rankLetter) const;

private:
    // --- 盤面データ ---
    /// 盤: [rank 0..8][file 0..8]  rank0='a'(最上段) ～ rank8='i'(最下段)
    QString m_board[9][9];

    bool m_blackToMove = true; ///< 手番（true=先手b, false=後手w）
    int m_plyNext = 1; ///< 次の手数（SFENの第4フィールド）

    // --- 持ち駒 ---
    enum Kind {
        P, ///< 歩
        L, ///< 香
        N, ///< 桂
        S, ///< 銀
        G, ///< 金
        B, ///< 角
        R, ///< 飛
        KIND_N ///< 種別数
    };
    std::array<int, KIND_N> m_handB{}; ///< 先手の持ち駒
    std::array<int, KIND_N> m_handW{}; ///< 後手の持ち駒

    // --- 座標変換・トークン操作ユーティリティ ---
    static int fileToCol(int file);
    static int rankLetterToRow(QChar r);
    static QChar kindToLetter(Kind k);
    static Kind letterToKind(QChar upper);
    static QChar toSideCase(QChar upper, bool black);
    static QString makeToken(QChar upper, bool black, bool promoted);
    static bool isPromotedToken(const QString& t);
    static QChar baseUpperFromToken(const QString& t);

    void clearBoard();
    void setStartposBoard();
    void addHand(bool black, Kind k, int n=1);
    bool subHand(bool black, Kind k, int n=1);

    /// 盤面配列からSFEN第1フィールドを生成する
    QString boardToSfenField() const;

    /// 持ち駒からSFEN第3フィールドを生成する（なければ "-"）
    QString handsToSfenField() const;

public:
    // --- 局面列構築ユーティリティ ---

    /// 初期SFENとUSI手列から局面列を構築する（hasTerminal==trueのとき終局用に末尾を複製）
    static QStringList buildSfenRecord(const QString& initialSfen,
                                       const QStringList& usiMoves,
                                       bool hasTerminal);

    /// 初期SFENとUSI手列から可視化用のShogiMove列を構築する
    static QVector<ShogiMove> buildGameMoves(const QString& initialSfen,
                                             const QStringList& usiMoves);

    // --- USI座標ヘルパ ---

    /// 段文字を数値に変換する（'a'..'i' → 1..9、無効時は -1）
    static int    rankLetterToNum(QChar r);

    /// 駒打ち時の「駒台」擬似座標を返す
    static QPoint dropFromSquare(QChar dropUpper, bool black);

    /// 先手なら大文字/後手なら小文字に変換する
    static QChar  dropLetterWithSide(QChar upper, bool black);

    /// "+p" 等の複数文字トークンを一文字（成駒対応）にマップする
    static QChar  tokenToOneChar(const QString& tok);
};

#endif // SFENPOSITIONTRACER_H
