#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

/// @file kiftosfenconverter.h
/// @brief KIF形式棋譜コンバータクラスの定義


#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/**
 * @brief KIF形式の棋譜を解析し、USI形式やGUI用データに変換するクラス
 *
 * 棋譜は1回だけ行に分けて読み、本譜と各変化を1回の走査で KifParseResult にする
 * （表示用の指し手と USI の指し手は同じ走査で作るので、並びが必ずそろう）。
 * ファイルは KifuLoadParser が文字コードを判別して読み、テキスト版の関数に渡す。
 */
class KifToSfenConverter
{
public:
    // ---- テキスト（貼り付けた棋譜など） ----

    /// 開始局面のSFEN（局面図、なければ手合割から判定）
    static QString detectInitialSfenFromText(const QString& text, QString* detectedLabel = nullptr);

    /// 本譜＋全変化をまとめて抽出（コメント・しおりも格納）
    [[nodiscard]] static bool parseText(const QString& text, KifParseResult& out, QString* errorMessage = nullptr);

    /// 「対局情報」を抽出して順序付きで返す（同一キーは改行で連結、値中の「\n」は改行に変換）
    static QList<KifGameInfoItem> extractGameInfoFromText(const QString& text);

    // ---- 行に分けたテキスト ----

    static QString detectInitialSfenFromLines(const QStringList& lines, QString* detectedLabel = nullptr);
    /// 行に分けた棋譜を読む。detectedLabel には手合割の名前（局面図なら "BOD"）を返す
    [[nodiscard]] static bool parseLines(const QStringList& lines, KifParseResult& out, QString* errorMessage = nullptr,
                                         QString* detectedLabel = nullptr);
    static QList<KifGameInfoItem> extractGameInfoFromLines(const QStringList& lines);

    // ---- 局面 ----

    /// 手合→初期SFEN
    static QString mapHandicapToSfen(const QString& label);

    [[nodiscard]] static bool buildInitialSfenFromBod(const QStringList& lines, QString& outSfen,
                                        QString* detectedLabel = nullptr, QString* warn = nullptr);
};

#endif // KIFTOSFENCONVERTER_H
