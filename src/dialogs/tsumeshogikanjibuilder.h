/// @file tsumeshogikanjibuilder.h
/// @brief 詰将棋の漢字PV構築ユーティリティ

#ifndef TSUMESHOGIKANJIBUILDER_H
#define TSUMESHOGIKANJIBUILDER_H

#include <QString>
#include <QStringList>

namespace TsumeshogiKanjiBuilder {

/// ShogiBoard内部の駒文字を漢字表記に変換
QString pieceCharToKanji(QChar piece);

/// USI形式のPVから漢字表記の読み筋テキストを生成
QString buildKanjiPv(const QString& baseSfen, const QStringList& pvMoves);

} // namespace TsumeshogiKanjiBuilder

#endif // TSUMESHOGIKANJIBUILDER_H
