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
class SfenPositionTracer;

namespace KifuParseCommon {

// USI/USEN共通：USI駒文字から漢字駒名を取得 (P→歩, L→香, etc.)
QString usiPieceToKanji(QChar usiPiece);

// USI/USEN共通：盤面トークンから漢字駒名を取得（成駒対応: +P→と, +B→馬 etc.）
QString usiTokenToKanji(QStringView token);

// USI/USEN共通：USI指し手から駒トークンを抽出
// tracer: 現在の盤面状態を追跡するトレーサー（指し手適用前の状態）
QString extractUsiPieceToken(QStringView usi, SfenPositionTracer& tracer);

// USI/USEN共通：USI指し手から日本語表記を生成
// pieceToken: SfenPositionTracerから取得した駒トークン（例: "P", "+B"）
QString usiMoveToPretty(const QString& usi, int plyNumber,
                        int& prevToFile, int& prevToRank,
                        const QString& pieceToken);

// USI/USEN共通：USI指し手列から表示アイテムを構築
// SfenPositionTracerで盤面追跡しながら日本語指し手表記を生成
// outDisp に指し手アイテムを追加（開始局面・終局アイテムは含まない）
// 戻り値は最後のply番号
int buildUsiMoveDisplayItems(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             QList<KifDisplayItem>& outDisp);

} // namespace KifuParseCommon

#endif // PARSEMOVEFORMAT_H
