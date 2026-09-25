#ifndef KIFUCONVERSIONSERVICE_H
#define KIFUCONVERSIONSERVICE_H

/// @file kifuconversionservice.h
/// @brief 棋譜ファイル／文字列を各形式へ変換する GUI 非依存サービス（自動化・CLI 用）

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/**
 * @brief 棋譜の読み込み（KIF/KI2/CSA/JKF/USI/USEN）と書き出し（同形式 + SFEN 列）
 *
 * 既存の `*ToSfenConverter` で解析し、`KifuBranchTree` と `GameRecordModel` を
 * 一時的に組み立てて既存のエクスポータで出力する。ウィジェットは使わない。
 */
class KifuConversionService
{
public:
    enum class Format { Auto, Kif, Ki2, Csa, Jkf, Usi, Usen, Sfen };

    struct Request {
        QString inputPath;                 ///< 入力ファイル（text と排他）
        QString text;                      ///< 入力文字列（inputPath と排他）
        Format inputFormat = Format::Auto; ///< Auto なら拡張子／内容から判定
        Format outputFormat = Format::Kif; ///< 出力形式
    };

    struct Result {
        bool ok = false;
        QString error;                     ///< 失敗理由（ok=false のとき）
        Format inputFormat = Format::Auto; ///< 判定した入力形式
        QStringList lines;                 ///< 出力テキスト（行単位）
        QString initialSfen;               ///< 開始局面
        QStringList usiMoves;              ///< 本譜の USI 手順
        QStringList sfens;                 ///< 本譜の局面列（開始局面を含む）
        QList<KifDisplayItem> moves;       ///< 本譜の表示用手順（ply 0 の開始局面項目を含む）
        QList<KifGameInfoItem> gameInfo;   ///< 対局情報
        QStringList warnings;              ///< 解析時の警告
        bool hasBranches = false;          ///< 変化を含むか
    };

    static Result convert(const Request& request);

    /// 形式名（"kif","ki2","csa","jkf","usi","usen","sfen","auto"）を解釈する
    static std::optional<Format> parseFormat(const QString& name);
    static QString formatName(Format format);
    /// 拡張子から入力形式を判定する（不明なら Auto）
    static Format formatFromPath(const QString& path);

private:
    struct Parsed {
        KifParseResult result;
        QString initialSfen;
        QList<KifGameInfoItem> gameInfo;
        QStringList warnings;
    };
    static bool parseFile(const QString& path, Format format, Parsed& out, QString* error);
    static Format detectFormatFromContent(const QString& content);
    /// 各手後の SFEN と指し手データが無い行を USI 手順から補完する
    static void completeLine(KifLine& line, const QString& baseSfen);
};

#endif // KIFUCONVERSIONSERVICE_H
