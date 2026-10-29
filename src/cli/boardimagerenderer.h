#ifndef BOARDIMAGERENDERER_H
#define BOARDIMAGERENDERER_H

/// @file boardimagerenderer.h
/// @brief SFEN から盤面画像を描画する（CLI の render-board 用。QApplication が必要）

#include "errorkinds.h"
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

/**
 * @brief `ShogiView` を画面に出さずに描画し、PNG などの画像にする
 *
 * GUI と同じ駒画像・配色（設定ファイルの値）で描画する。
 * `QT_QPA_PLATFORM=offscreen` の `QApplication` でも動作する。
 */
class BoardImageRenderer
{
public:
    struct Options {
        int squareSize = 50;      ///< 1 マスの幅(px)。20〜150
        bool flip = false;        ///< 後手側から見る
        QString lastMoveUsi;      ///< 強調表示する最後の指し手（USI 形式。空なら無し）
        QString blackName;        ///< 先手名（空なら記号のみ）
        QString whiteName;        ///< 後手名
    };

    /// 描画に失敗すると null 画像を返し、failure に理由を入れる（局面・指し手・大きさの誤りは invalid_argument）
    static QImage render(const QString& sfen, const Options& options, Failure* failure = nullptr);

    /// 画像ファイルの拡張子（"png" "jpg" "jpeg" "bmp" "webp"）。MCP サーバーは書き出し先をこれに限る
    static QStringList fileSuffixes();

    /// 画像ファイルに保存する。形式は拡張子から決まる（fileSuffixes。拡張子がなければ png）
    static bool renderToFile(const QString& sfen, const QString& outputPath, const Options& options,
                             Failure* failure = nullptr, QSize* outSize = nullptr);
};

#endif // BOARDIMAGERENDERER_H
