#ifndef DEBUGSCREENSHOTSERVICE_H
#define DEBUGSCREENSHOTSERVICE_H
#ifdef QT_DEBUG

/// @file debugscreenshotservice.h
/// @brief デバッグ用スクリーンショットキャプチャサービス

#include <QString>
class QMainWindow;
class QWidget;

/**
 * @brief MainWindow全体や任意ウィジェットのスクリーンショットをPNG保存するデバッグ用サービス
 *
 * デバッグビルド専用（QT_DEBUGガード）。
 * キャプチャ画像は /tmp/shogiboardq-debug/ に保存され、
 * Claude Code の Read ツールで画像を解析できる。
 */
class DebugScreenshotService final {
public:
    explicit DebugScreenshotService(QMainWindow* mainWindow);

    void setOutputDirectory(const QString& dir);
    QString outputDirectory() const;
    void setMaxWidth(int width);
    QString lastCapturedPath() const;

    /// MainWindow全体をキャプチャしPNG保存する
    QString captureMainWindow(const QString& context = {});
    /// 任意ウィジェットをキャプチャしPNG保存する
    QString captureWidget(QWidget* widget, const QString& context = {});
    /// 出力ディレクトリ内のPNGファイルを一括削除する
    void clearOutputDirectory();

private:
    QString saveCapture(const QImage& image, const QString& context);
    static QString generateFilename(const QString& context);

    QMainWindow* m_mainWindow = nullptr;
    QString m_outputDir = QStringLiteral("/tmp/shogiboardq-debug");
    int m_maxWidth = 1920;
    QString m_lastCapturedPath;
};

#endif // QT_DEBUG
#endif // DEBUGSCREENSHOTSERVICE_H
