#ifndef KIFUCONVERSIONSERVICE_H
#define KIFUCONVERSIONSERVICE_H

/// @file kifuconversionservice.h
/// @brief 棋譜ファイル／文字列を各形式へ変換する GUI 非依存サービス（自動化・CLI 用）

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "errorkinds.h"
#include "kifparsetypes.h"
#include "kifuformat.h"

/**
 * @brief 棋譜の読み込み（KIF/KI2/CSA/JKF/USI/USEN/SFEN/BOD）と書き出し（KIF/KI2/CSA/JKF/USI/USEN と SFEN 列）
 *
 * 読み込みは GUI と同じ KifuLoadParser で行い、`KifuBranchTree` を一時的に組み立てて、
 * GUI の保存と同じ書き出しの表（`KifuExport::lines`）で出力する。ウィジェットは使わない。
 */
class KifuConversionService
{
public:
    struct Request {
        QString inputPath;                       ///< 入力ファイル（text と排他）
        QString text;                            ///< 入力文字列（inputPath と排他）
        std::optional<KifuFormat> inputFormat;   ///< なければ拡張子／内容から判定
        KifuFormat outputFormat = KifuFormat::Kif; ///< 出力形式（Sfen は本譜の局面列）
        /// 出力を保存する文字コード（KIF・KI2・CSA の先頭行の宣言に書く。保存先は KifuSaveFile::encodingForPath）
        KifuEncoding outputEncoding = KifuEncoding::Utf8;
    };

    struct Result {
        bool ok = false;
        Failure failure;                   ///< 失敗の分類と理由（ok=false のとき）
        KifuFormat inputFormat = KifuFormat::Kif; ///< 読んだ入力形式
        QStringList lines;                 ///< 出力テキスト（行単位）
        QString initialSfen;               ///< 開始局面
        QStringList usiMoves;              ///< 本譜の USI 手順
        QStringList sfens;                 ///< 本譜の局面列（開始局面を含む）
        RecordLine mainline;               ///< 本譜（終局の行と、開始局面のコメント・しおりを含む）
        QList<KifGameInfoItem> gameInfo;   ///< 対局情報
        QStringList warnings;              ///< 解析時の警告
        bool hasBranches = false;          ///< 変化を含むか
    };

    static Result convert(const Request& request);
};

#endif // KIFUCONVERSIONSERVICE_H
