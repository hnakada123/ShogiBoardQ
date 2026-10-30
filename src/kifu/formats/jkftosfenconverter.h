#ifndef JKFTOSFENCONVERTER_H
#define JKFTOSFENCONVERTER_H

/// @file jkftosfenconverter.h
/// @brief JKF形式棋譜コンバータクラスの定義

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringView>

#include "kifparsetypes.h"

class Position;

/**
 * @brief JSON棋譜フォーマット(JKF)の棋譜を読む
 *
 * JKF仕様: https://github.com/na2hiro/json-kifu-format
 *
 * JSON は1回だけ解析し、棋譜・開始局面とその書き方・対局情報をまとめて返す。
 * 指し手・初期局面の変換は JkfCodec、手順の組み立ては KifLineBuilder に任せる。
 */
class JkfToSfenConverter
{
public:
    /// 棋譜を読む。開始局面の書き方は preset なら Handicap、OTHER の局面図なら Diagram、initial が無ければ Default
    static KifuParseOutcome parse(QStringView text, const ParseOptions& options = {});

private:
    static bool parseJson(const QByteArray& data, QJsonObject& root, QString* warn);
    static QList<KifGameInfoItem> gameInfoFromRoot(const QJsonObject& root);
    static QString buildInitialSfen(const QJsonObject& root, StartKind* kind = nullptr);
    static bool readRecord(const QJsonArray& moves, const Position& start, KifParseResult& out, ParseReport& report);
};

#endif // JKFTOSFENCONVERTER_H
