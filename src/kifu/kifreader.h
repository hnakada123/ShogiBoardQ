#ifndef KIFREADER_H
#define KIFREADER_H

/// @file kifreader.h
/// @brief 棋譜ファイルの文字コード自動判別読み込み機能の定義

#include <QString>
#include <QStringList>

/**
 * @brief 棋譜ファイルの文字コード自動判別読み込み
 *
 * BOM判定 → UTF-8 → CP932/Shift_JIS → EUC-JP の順で
 * 文字コードを推定し、テキストを行単位で返す。
 *
 */
namespace KifReader {

/**
 * @brief 文字コードを自動判別してテキストを1行ずつ取得する
 * @param filePath 読み込み対象のファイルパス
 * @param outLines 出力先の行リスト
 * @param usedEncoding 実際に使われたエンコーディング名（例: "utf-8", "cp932"）
 * @param warn 読み込み中の注意点を追記する先
 * @return 読み込み成功なら true
 */
bool readAllLinesAuto(const QString& filePath,
                      QStringList& outLines,
                      QString* usedEncoding = nullptr,
                      QString* warn = nullptr);

/// 互換API（旧名）: 既存コードが KifReader::readLinesAuto() を呼んでいても動作する
inline bool readLinesAuto(const QString& filePath,
                          QStringList& outLines,
                          QString* usedEncoding = nullptr,
                          QString* warn = nullptr)
{
    return readAllLinesAuto(filePath, outLines, usedEncoding, warn);
}

} // namespace KifReader

#endif // KIFREADER_H
