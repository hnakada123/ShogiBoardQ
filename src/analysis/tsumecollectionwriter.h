#ifndef TSUMECOLLECTIONWRITER_H
#define TSUMECOLLECTIONWRITER_H

/// @file tsumecollectionwriter.h
/// @brief 詰将棋局面の一覧（局面集ファイル）の書き出し

#include <QString>
#include <QStringList>

/// 詰将棋局面生成の結果を、局面集ファイル（1行1局面）として書き出す。
/// 先頭の '#' 行はコメント（TsumeCollection::parse と局面集ビューアは読み飛ばす）。
namespace TsumeCollectionWriter {

/// 1局面の行。includePv なら USI の position 構文に合わせて " moves " に続けて手順を付加する
QString line(const QString& sfen, const QStringList& pv, bool includePv);

/// header（'#' 始まりのコメント行）と lines を1行ずつ書き、すべて書けたときだけファイルを置き換える。
/// 空の行は書かない。失敗したら false を返し、errorMessage に理由（QSaveFile のエラー文）を入れる
bool save(const QString& filePath, const QStringList& header, const QStringList& lines,
          QString* errorMessage = nullptr);

} // namespace TsumeCollectionWriter

#endif // TSUMECOLLECTIONWRITER_H
