#ifndef SFENPOSITIONTRACER_H
#define SFENPOSITIONTRACER_H

#pragma once
#include <QString>
#include <QStringList>
#include <array>

/**
 * @brief USI手列を順に適用し、各手後のSFENを生成する簡易トレーサ。
 * - 平手初期局面から開始
 * - 盤上は SFEN と同じ文字（先手=大文字, 後手=小文字, 成駒は頭に'+'）
 * - 持ち駒は種類別に枚数管理（先手/後手）
 * - 合法手チェックはしない（与えられたUSI手を素直に適用）
 */
class SfenPositionTracer {
public:
    SfenPositionTracer();

    void resetToStartpos();
    /// USI一手（例: "7g7f", "P*5e", "2b3c+"）を適用
    bool applyUsiMove(const QString& usi);

    /// 現在局面の SFEN 文字列を返す（例: "lnsgkgsnl/... b - 1"）
    QString toSfenString() const;

    /// 手列を順に適用し、各手の直後局面の SFEN を配列で返す
    QStringList generateSfensForMoves(const QStringList& usiMoves);

    // 初期SFENから現在局面をセットする（4フィールド形式を想定: board stm hands ply）
    bool setFromSfen(const QString& sfen);

    bool blackToMove() const { return m_blackToMove; }
    // file: 1..9, rank: 'a'..'i'。空なら空文字列を返す
    QString tokenAtFileRank(int file, QChar rankLetter) const;

private:
    // 盤: [rank 0..8][file 0..8]  rank0='a'(最上段) ～ rank8='i'(最下段), file0='1' ～ file8='9'
    QString m_board[9][9];

    // 手番（true=先手b, false=後手w）
    bool m_blackToMove = true;

    // 次の手数（初期=1）。SFENの第4フィールドに入れる
    int m_plyNext = 1;

    // 持ち駒 [種別] の枚数
    enum Kind { P, L, N, S, G, B, R, KIND_N };
    std::array<int, KIND_N> m_handB{}; // 先手
    std::array<int, KIND_N> m_handW{}; // 後手

    // --- ユーティリティ ---
    static int fileToCol(int file);            // '1'..'9' → 0..8
    static int rankLetterToRow(QChar r);       // 'a'..'i' → 0..8
    static QChar kindToLetter(Kind k);         // 種別 → 'P','L',...（大文字）
    static Kind letterToKind(QChar upper);     // 'P','L',... → 種別
    static QChar toSideCase(QChar upper, bool black); // sideで大/小文字付与
    static QString makeToken(QChar upper, bool black, bool promoted); // 盤上トークン
    static bool isPromotedToken(const QString& t);      // "+p" 等
    static QChar baseUpperFromToken(const QString& t);  // "+p"→'P', 's'→'S' 等

    void clearBoard();
    void setStartposBoard();
    void addHand(bool black, Kind k, int n=1);
    bool subHand(bool black, Kind k, int n=1);

    // 盤→SFEN第1フィールド
    QString boardToSfenField() const;
    // 持ち駒→SFEN第3フィールド（なければ "-"）
    QString handsToSfenField() const;
};

#endif // SFENPOSITIONTRACER_H
