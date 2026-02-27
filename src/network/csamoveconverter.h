#ifndef CSAMOVECONVERTER_H
#define CSAMOVECONVERTER_H

/// @file csamoveconverter.h
/// @brief CSA/USI/SFEN形式変換ユーティリティクラスの定義

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QPoint>

#include "shogitypes.h"
#include "csaclient.h"

class ShogiGameController;
class ShogiBoard;

/**
 * @brief CSA/USI/SFEN形式変換ユーティリティクラス
 *
 * CSAプロトコルの指し手・駒表現と、USI/SFEN/漢字表記の相互変換を行う。
 * すべてのメソッドは静的で、状態を持たない。
 */
class CsaMoveConverter
{
    Q_DECLARE_TR_FUNCTIONS(CsaMoveConverter)

public:
    /// CSA形式の指し手をUSI形式に変換する
    static QString csaToUsi(const QString& csaMove);

    /// USI形式の指し手をCSA形式に変換する
    static QString usiToCsa(const QString& usiMove, bool isBlack, ShogiBoard* board);

    /// 盤面座標からCSA形式の指し手に変換する
    static QString boardToCSA(const QPoint& from, const QPoint& to, bool promote,
                              bool isBlackSide, ShogiBoard* board);

    /// 駒（Piece enum）をCSA形式の駒文字に変換する
    static QString pieceCharToCsa(Piece piece, bool promote);

    /// CSA駒記号をUSI駒記号に変換する
    static QString csaPieceToUsi(const QString& csaPiece);

    /// USI駒記号をCSA駒記号に変換する
    static QString usiPieceToCsa(const QString& usiPiece, bool promoted);

    /// CSA駒記号を漢字に変換する
    static QString csaPieceToKanji(const QString& csaPiece);

    /// CSA駒種から駒台インデックスを取得する
    static int pieceTypeFromCsa(const QString& csaPiece);

    /// CSA駒種をPiece enumに変換する
    static Piece csaPieceToSfenPiece(const QString& csaPiece, bool isBlack);

    /// CSA形式の指し手を表示用文字列に変換する
    static QString csaToPretty(const QString& csaMove, bool isPromotion,
                               int prevToFile, int prevToRank, int moveCount);

    /// 対局結果をローカライズ済み文字列に変換する
    static QString gameResultToString(CsaClient::GameResult result);

    /// 終局原因をローカライズ済み文字列に変換する
    static QString gameEndCauseToString(CsaClient::GameEndCause cause);

    /// CSA形式の指し手を盤面に適用する（ビュー更新は行わない）
    static bool applyMoveToBoard(const QString& csaMove, ShogiGameController* gc,
                                 QStringList& usiMoves, QStringList* sfenHistory,
                                 int moveCount);
};

#endif // CSAMOVECONVERTER_H
