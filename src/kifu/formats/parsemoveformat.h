#ifndef PARSEMOVEFORMAT_H
#define PARSEMOVEFORMAT_H

/// @file parsemoveformat.h
/// @brief USI/USEN共通の指し手フォーマット変換関数の定義

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

struct KifDisplayItem;

namespace KifuParseCommon {

// USI/USEN共通：USI指し手列から表示アイテムを構築
// SfenPositionTracerで盤面追跡しながら日本語指し手表記を生成
// outDisp に指し手アイテムを追加（開始局面・終局アイテムは含まない）
// 戻り値は最後のply番号
// previousUsi は開始局面に至る直前の指し手（分岐の1手目の「同」判定用。無ければ空）
int buildUsiMoveDisplayItems(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             QList<KifDisplayItem>& outDisp,
                             const QString& previousUsi = QString());

// 任意の開始手番と、分岐内の実手数から終局側を決める。
KifDisplayItem createUsiTerminalDisplayItem(int ply, const QString& term,
                                           const QString& baseSfen, qsizetype moveCount);

} // namespace KifuParseCommon

#endif // PARSEMOVEFORMAT_H
