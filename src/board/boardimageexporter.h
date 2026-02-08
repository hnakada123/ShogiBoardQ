#ifndef BOARDIMAGEEXPORTER_H
#define BOARDIMAGEEXPORTER_H

/// @file boardimageexporter.h
/// @brief 盤面画像のクリップボードコピー・ファイル保存機能
/// @todo remove コメントスタイルガイド適用済み

#include <QObject>
class QWidget;
class QImage;

/**
 * @brief 盤面ウィジェットのキャプチャ画像をエクスポートするユーティリティ
 * @todo remove コメントスタイルガイド適用済み
 *
 * クリップボードへのコピーと、複数画像フォーマットでのファイル保存に対応する。
 */
class BoardImageExporter final {
public:
    /// 盤面画像をクリップボードにコピーする
    static void copyToClipboard(QWidget* boardWidget);

    /// 盤面画像をファイルに保存する（フォーマット選択ダイアログ付き）
    static void saveImage(QWidget* parent, QWidget* boardWidget);
};

#endif // BOARDIMAGEEXPORTER_H
