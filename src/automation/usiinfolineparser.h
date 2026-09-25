#ifndef USIINFOLINEPARSER_H
#define USIINFOLINEPARSER_H

/// @file usiinfolineparser.h
/// @brief USI の info 行を構造化する軽量パーサ（GUI 非依存）

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

/// info 行 1 行分の解析結果。未指定の項目は -1 / nullopt
struct UsiInfoLine {
    int depth = -1;
    int seldepth = -1;
    int multipv = 1;
    std::optional<int> scoreCp;
    std::optional<int> scoreMate;   ///< 詰み手数（エンジン視点。負は詰まされる）
    QString bound;                  ///< "lower" / "upper" / 空
    qint64 nodes = -1;
    qint64 nps = -1;
    qint64 timeMs = -1;
    int hashfull = -1;
    QString currmove;
    QStringList pv;
    QString string;                 ///< info string の内容

    bool hasPv() const { return !pv.isEmpty(); }
    /// JSON 表現（snake_case、未指定項目は省略）
    QJsonObject toJson() const;
};

/**
 * @brief USI の info 行を解析する
 *
 * `ShogiEngineInfoParser` は盤面データと漢字変換に結び付いているため、
 * 自動化・CLI では USI 手をそのまま返すこのパーサを使う。
 */
class UsiInfoLineParser
{
public:
    /// 先頭が "info" でない行は既定値のまま返す
    static UsiInfoLine parse(const QString& line);
};

#endif // USIINFOLINEPARSER_H
