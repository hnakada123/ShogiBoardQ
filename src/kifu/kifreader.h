#pragma once
#include <QString>
#include <QStringList>

// NOTE:
// このヘッダは「namespace KifReader」スタイルです。
// 互換のため、旧名 readLinesAuto(...) も inline ラッパとして残しています。
namespace KifReader {

// 文字コードを自動判別してテキストを1行ずつ取得します。
// - usedEncoding: 実際に使われたエンコーディング名（例: "utf-8", "cp932", "euc-jp"）
// - warn: 読み込み中の注意点（必要なら）を追記します。
bool readAllLinesAuto(const QString& filePath,
                      QStringList& outLines,
                      QString* usedEncoding = nullptr,
                      QString* warn = nullptr);

// 互換API（旧名）: 既存コードが KifReader::readLinesAuto(...) を呼んでいても動作します。
inline bool readLinesAuto(const QString& filePath,
                          QStringList& outLines,
                          QString* usedEncoding = nullptr,
                          QString* warn = nullptr)
{
    return readAllLinesAuto(filePath, outLines, usedEncoding, warn);
}

} // namespace KifReader
