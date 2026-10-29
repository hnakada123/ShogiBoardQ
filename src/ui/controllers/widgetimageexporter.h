#ifndef WIDGETIMAGEEXPORTER_H
#define WIDGETIMAGEEXPORTER_H

/// @file widgetimageexporter.h
/// @brief 部品（盤・評価値グラフ）の画像のクリップボードへのコピーとファイルへの保存

#include <QObject>
#include <QCoreApplication>

class QWidget;
class QImage;

/**
 * @brief 部品を grab() した画像を、クリップボードへコピーする・形式を選んでファイルに保存する
 *
 * 利用者の操作（ImageExportController）から使う。決まった場所へ PNG で保存するのは ScreenshotService。
 */
class WidgetImageExporter final {
    Q_DECLARE_TR_FUNCTIONS(WidgetImageExporter)

public:
    /// 盤面画像をクリップボードにコピーする
    static void copyToClipboard(QWidget* boardWidget);

    /// 盤面画像をファイルに保存する（フォーマット選択ダイアログ付き）
    static void saveImage(QWidget* parent, QWidget* boardWidget,
                          const QString& baseName = QStringLiteral("ShogiBoard"));
};

#endif // WIDGETIMAGEEXPORTER_H
