#ifndef KIFUTEXTDECODER_H
#define KIFUTEXTDECODER_H

/// @file kifutextdecoder.h
/// @brief 棋譜・局面のテキストの文字コードの判別（全形式の読み込みで共通）

#include <QString>
#include <QStringList>

/**
 * @brief 棋譜ファイルの文字コード自動判別読み込み（全形式の棋譜の読み込みで使う。棋譜ファイルのバイト列をテキストにするのはここだけ）
 *
 * BOM → 先頭の行の文字コードの宣言（KIF・KI2 の "#KIF version=2.0 encoding=Shift_JIS"、CSA の
 * "'CSA encoding=UTF-8"・"'encoding=..."。宣言どおりに読めるときだけ従う）→ UTF-8 → CP932/Shift_JIS → EUC-JP の順で
 * 文字コードを決める。形式によらず同じ規則で、宣言の読み方も形式で分けない。
 */
namespace KifuTextDecoder {

/**
 * @brief 文字コードを決めてテキストを1行ずつ取得する（decodeAuto の規則）
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

/**
 * @brief 文字コードを決めてバイト列をテキストにする（BOM → 宣言 → UTF-8 → CP932 → EUC-JP → ローカル 8bit）
 * @param usedEncoding 実際に使われたエンコーディング名（宣言に従ったときは "cp932(declared)" のように付ける）
 * @param warn どれとも決められずにローカル 8bit で読んだときの注意を追記する先
 */
QString decodeAuto(const QByteArray& bytes, QString* usedEncoding = nullptr, QString* warn = nullptr);

/**
 * @brief 文字コードを決めてファイル全体をテキストとして取得する（decodeAuto の規則。改行は変換しない）
 * @return 読み込み成功なら true
 */
bool readAllTextAuto(const QString& filePath,
                     QString& outText,
                     QString* usedEncoding = nullptr,
                     QString* warn = nullptr);

/// テキストを行に分ける（先頭の BOM を除き、CRLF・CR を LF とみなす）
QStringList splitLines(const QString& text);

} // namespace KifuTextDecoder

#endif // KIFUTEXTDECODER_H
