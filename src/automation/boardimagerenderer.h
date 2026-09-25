#ifndef BOARDIMAGERENDERER_H
#define BOARDIMAGERENDERER_H

/// @file boardimagerenderer.h
/// @brief SFEN から盤面画像を描画する（自動化・CLI 用。QApplication が必要）

#include <QImage>
#include <QSize>
#include <QString>

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

    /// 描画に失敗すると null 画像を返し、error に理由を入れる
    static QImage render(const QString& sfen, const Options& options, QString* error = nullptr);

    /// 画像ファイルに保存する。形式は拡張子から決まる（png/jpg/bmp/webp など）
    static bool renderToFile(const QString& sfen, const QString& outputPath, const Options& options,
                             QString* error = nullptr, QSize* outSize = nullptr);
};

#endif // BOARDIMAGERENDERER_H
