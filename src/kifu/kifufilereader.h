#ifndef KIFUFILEREADER_H
#define KIFUFILEREADER_H

/// @file kifufilereader.h
/// @brief 棋譜ファイル読み込みI/O層（フォーマット判定・一時ファイル管理）

#include <QString>
#include <QTemporaryFile>
#include <memory>

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

/// UTF-8の一時ファイルを作成する。返されたオブジェクトの破棄時に削除される。
std::unique_ptr<QTemporaryFile> createTempFile(KifuFormat fmt, const QString& content);

} // namespace KifuFileReader

#endif // KIFUFILEREADER_H
