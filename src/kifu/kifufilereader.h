#ifndef KIFUFILEREADER_H
#define KIFUFILEREADER_H

/// @file kifufilereader.h
/// @brief 棋譜ファイル読み込みI/O層（フォーマット判定・一時ファイル管理）

#include <QString>

namespace KifuFileReader {

/// 棋譜フォーマットの種別
enum class KifuFormat {
    Unknown,
    KIF,
    KI2,
    CSA,
    USI,
    JKF,
    USEN,
    SFEN,
    BOD
};

/// テキスト内容から棋譜フォーマットを自動判定する
KifuFormat detectFormat(const QString& content);

/// フォーマットに応じた一時ファイルパスを返す
QString tempFilePath(KifuFormat fmt);

/// 一時ファイルにUTF-8で書き込む
bool writeTempFile(const QString& path, const QString& content);

} // namespace KifuFileReader

#endif // KIFUFILEREADER_H
