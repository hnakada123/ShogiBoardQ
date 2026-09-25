#ifndef SCREENSHOTSERVICE_H
#define SCREENSHOTSERVICE_H

/// @file screenshotservice.h
/// @brief ウィンドウ・ウィジェットのスクリーンショットを PNG 保存するサービス

#include <QString>
class QImage;
class QMainWindow;
class QWidget;

/**
 * @brief MainWindow 全体や任意ウィジェットのスクリーンショットを PNG 保存する
 *
 * デバッグビルドの F12（`DebugScreenshotWiring`）と、自動化 API の `screenshot.capture` から使う。
 * 既定の保存先は一時ディレクトリ配下の `shogiboardq-screenshots`。
 */
class ScreenshotService final {
public:
    explicit ScreenshotService(QMainWindow* mainWindow);

    void setOutputDirectory(const QString& dir);
    QString outputDirectory() const;
    void setMaxWidth(int width);
    QString lastCapturedPath() const;

    /// MainWindow 全体をキャプチャして PNG 保存する。失敗時は空文字列
    QString captureMainWindow(const QString& context = {});
    /// 任意ウィジェットをキャプチャして PNG 保存する。失敗時は空文字列
    QString captureWidget(QWidget* widget, const QString& context = {});
    /// 出力ディレクトリ内の PNG ファイルを一括削除する
    void clearOutputDirectory();

    /// 既定の保存先ディレクトリ
    static QString defaultOutputDirectory();

private:
    QString saveCapture(const QImage& image, const QString& context);
    static QString generateFilename(const QString& context);

    QMainWindow* m_mainWindow = nullptr;
    QString m_outputDir;
    int m_maxWidth = 1920;
    QString m_lastCapturedPath;
};

#endif // SCREENSHOTSERVICE_H
