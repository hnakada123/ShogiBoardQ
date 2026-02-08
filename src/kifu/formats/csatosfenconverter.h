#ifndef CSATOSFENCONVERTER_H
#define CSATOSFENCONVERTER_H

/// @file csatosfenconverter.h
/// @brief CSA形式棋譜コンバータクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include "kifparsetypes.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QChar>

/**
 * @brief CSA形式の棋譜ファイルを解析し、SFEN/USI形式およびGUI用データに変換するクラス
 *
 * CSA形式はコンピュータ将棋協会が策定した棋譜フォーマット。
 * PI/P1..P9による初期局面指定、+/-による指し手、$による対局情報を解析する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class CsaToSfenConverter
{
public:
    // --- 公開API ---

    /**
     * @brief CSAファイルを解析して本譜データを構築する
     * @param filePath CSAファイルパス
     * @param out 解析結果の格納先
     * @param warn 警告メッセージの格納先（nullptrで省略可）
     * @return 成功時 true
     */
    static bool parse(const QString& filePath, KifParseResult& out, QString* warn);

    /// CSAファイルからヘッダー情報（対局者名、棋戦名など）を抽出する
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // --- 盤面・駒の型定義 ---

    /// 手番
    enum Color { Black, White, None };

    /// 駒種
    enum Piece {
        NO_P, FU, KY, KE, GI, KI, KA, HI, OU,
        TO, NY, NK, NG, UM, RY
    };

    /// 盤上の1マス
    struct Cell {
        Piece  p = NO_P; ///< 駒種
        Color  c = None;  ///< 手番（どちらの駒か）
    };

    /// 9x9盤面
    struct Board {
        Cell sq[10][10]; ///< [x][y], インデックス1..9が有効範囲
        /// 平手初期配置に設定する
        void setHirate();
    };

private:
    // --- 行種別判定 ---

    static bool readAllLinesDetectEncoding(const QString& path, QStringList& outLines, QString* warn);
    static bool isMoveLine(const QString& s);
    static bool isResultLine(const QString& s);
    static bool isMetaLine(const QString& s);
    static bool isCommentLine(const QString& s);

    // --- 局面・指し手解析 ---

    static bool parseStartPos(const QStringList& lines, int& idx, QString& baseSfen, Color& stm, Board& board);

    static bool parseMoveLine(const QString& line, Color mover, Board& b,
                               int& prevTx, int& prevTy,
                               QString& usiMoveOut, QString& prettyOut, QString* warn);

    // --- 変換ユーティリティ ---

    static bool parseCsaMoveToken(const QString& token, int& fx, int& fy, int& tx, int& ty, Piece& afterPiece);
    static QChar usiRankLetter(int y);
    static QString toUsiSquare(int x, int y);
    static bool   isPromotedPiece(Piece p);
    static Piece  basePieceOf(Piece p);
    static QString pieceKanji(Piece p);
    static QString zenkakuDigit(int d);
    static QString kanjiRank(int y);
    static bool   inside(int v);
};

#endif // CSATOSFENCONVERTER_H
