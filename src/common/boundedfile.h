#ifndef BOUNDEDFILE_H
#define BOUNDEDFILE_H

/// @file boundedfile.h
/// @brief 利用者が選んだファイル（棋譜・局面集）を、大きさの上限つきで読む

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>

/**
 * @brief 上限つきのファイルの読み込み
 *
 * 通常のファイルだけを読む。FIFO や /dev/zero のような特殊ファイルは読み終わらず、大きすぎるファイルは
 * メモリを使い切るので読まない（セキュリティ監査 S14）。
 */
namespace BoundedFile {

/// 棋譜ファイルの上限（KIF で1万手の棋譜が約1MB）
inline constexpr qint64 kMaxKifuBytes = 64LL * 1024 * 1024;
/// 局面集・詰将棋集のファイルの上限（1行1局面）
inline constexpr qint64 kMaxCollectionBytes = 256LL * 1024 * 1024;

enum class Result {
    Ok,
    OpenFailed,       ///< 無い・開けない
    NotRegularFile,   ///< ディレクトリ・FIFO・デバイスなど
    TooLarge,         ///< maxBytes より大きい
};

/// path を maxBytes まで読む。Ok のときだけ out に中身が入る
inline Result read(const QString& path, qint64 maxBytes, QByteArray& out)
{
    out.clear();
    const QFileInfo info(path);
    if (!info.exists()) return Result::OpenFailed;
    if (!info.isFile()) return Result::NotRegularFile;
    if (info.size() > maxBytes) return Result::TooLarge;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return Result::OpenFailed;
    // 調べた後に伸びたファイルも上限までしか読まない
    out = file.read(maxBytes + 1);
    if (out.size() > maxBytes) {
        out.clear();
        return Result::TooLarge;
    }
    return Result::Ok;
}

/// 失敗の理由（ログ・警告に書く英文）
inline QString describe(Result result, const QString& path, qint64 maxBytes)
{
    switch (result) {
    case Result::Ok: return {};
    case Result::OpenFailed: return QStringLiteral("open failed: %1").arg(path);
    case Result::NotRegularFile: return QStringLiteral("not a regular file: %1").arg(path);
    case Result::TooLarge:
        return QStringLiteral("file is too large (over %1 MB): %2").arg(maxBytes / (1024 * 1024)).arg(path);
    }
    return {};
}

} // namespace BoundedFile

#endif // BOUNDEDFILE_H
