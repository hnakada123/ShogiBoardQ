#ifndef CSATOSFENCONVERTER_H
#define CSATOSFENCONVERTER_H

#include "kifparsetypes.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QChar>

class CsaToSfenConverter
{
public:
    // CSAを読み込み、KifParseResult を埋める。
    // 成功: true / 失敗: false（warn に警告や理由を入れる）
    static bool parse(const QString& filePath, KifParseResult& out, QString* warn);

    // 追加: CSAファイルからヘッダー情報（対局者名、棋戦名など）を抽出する
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // ---- 盤／指し手処理 ----
    enum Color { Black, White, None };
    enum Piece {
        NO_P, FU, KY, KE, GI, KI, KA, HI, OU,
        TO, NY, NK, NG, UM, RY
    };

    struct Cell {
        Piece  p = NO_P;
        Color  c = None;
    };

    struct Board {
        // 1..9 を有効とする9x9（インデックスは [1..9] で扱う）
        Cell sq[10][10]; // [x][y]
        // 平手初期配置へ。
        void setHirate();
    };

private:
    // ---- 低レベルユーティリティ ----
    static bool readAllLinesDetectEncoding(const QString& path, QStringList& outLines, QString* warn);
    static bool isMoveLine(const QString& s);
    static bool isResultLine(const QString& s);
    static bool isMetaLine(const QString& s);
    static bool isCommentLine(const QString& s);

    static bool parseStartPos(const QStringList& lines, int& idx, QString& baseSfen, Color& stm, Board& board);

    // 【修正】直前の着手位置 (prevTx, prevTy) を参照渡しで受け取るように変更
    static bool parseMoveLine(const QString& line, Color mover, Board& b,
                               int& prevTx, int& prevTy,
                               QString& usiMoveOut, QString& prettyOut, QString* warn);

    // ---- 変換ユーティリティ ----
    static bool parseCsaMoveToken(const QString& token, int& fx, int& fy, int& tx, int& ty, Piece& afterPiece);
    static QChar usiRankLetter(int y);      // 1->'a' ... 9->'i'
    static QString toUsiSquare(int x, int y);
    static bool   isPromotedPiece(Piece p);
    static Piece  basePieceOf(Piece p);
    static QString pieceKanji(Piece p);     // 歩,香,桂,銀,金,角,飛,玉, と/馬/龍/成香/成桂/成銀
    static QString zenkakuDigit(int d);     // 1..9 -> １..９
    static QString kanjiRank(int y);        // 1..9 -> 一..九
    static bool   inside(int v);
};

#endif // CSATOSFENCONVERTER_H
