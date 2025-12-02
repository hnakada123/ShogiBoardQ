#ifndef KI2TOSFENCONVERTER_H
#define KI2TOSFENCONVERTER_H

#include "kifparsetypes.h"

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>

class Ki2ToSfenConverter
{
public:
    // --- エントリポイント（KI2は分岐を持たない前提で本譜のみを構築） ---
    static bool parse(const QString& filePath,
                      KifParseResult& out,
                      QString* warn = nullptr);

    // KIF とインターフェースを揃えるためのラッパ（実質 parse と同じ）
    static bool parseWithVariations(const QString& filePath,
                                    KifParseResult& out,
                                    QString* warn = nullptr)
    {
        return parse(filePath, out, warn);
    }

    // ヘッダ情報の抽出（必要に応じて拡張）
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

private:
    // 文字コード自動判定（簡易）。UTF-8(BOM) / UTF-16(BOM) は QTextStream に任せ、
    // それ以外は UTF-8 を既定とします。必要なら Shift_JIS 判定を後日拡張してください。
    static bool readAllLinesDetectEncoding_(const QString& filePath,
                                            QStringList& outLines,
                                            QString* warn);

    // 初期局面の決定（手合割 or BOD → SFEN）。見つからなければ平手。
    static QString decideInitialSfen_(const QStringList& lines, int& startIndex, QString* warn);

    // 本譜部の先頭インデックスを返す（ヘッダ/BOD をスキップ）
    static int findMainlineStart_(const QStringList& lines);

    // 行から KI2 の「1手表記トークン」をすべて取り出す（1 行に複数手が並ぶ想定）
    static void extractKi2TokensFromLine_(const QString& line, QStringList& outTokens);

    // トークンを正規化（全角スペース→半角、連続空白圧縮など）
    static QString normalizeToken_(QString t);

    // 駒種：漢字 → 基本駒（英大文字）と成りフラグ（例：歩→'P'、と→'P'+promoted=true）
    static bool mapKanjiPiece_(const QString& s, QChar& base, bool& promoted);

    // 「７六」などの終点を (file, rank) に変換（1..9）。「同」は -1,-1 を返す。
    static void parseDestSquare_(const QString& twoChars, int& file, int& rank, bool& isDou);

    // 千日手/投了/中断…の終局語
    static bool isTerminalWord_(const QString& t, QString& termLabel);
};

#endif // KI2TOSFENCONVERTER_H
