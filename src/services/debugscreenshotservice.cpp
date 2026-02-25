/// @file debugscreenshotservice.cpp
/// @brief デバッグ用スクリーンショットキャプチャサービスの実装

#include "debugscreenshotservice.h"

#ifdef QT_DEBUG

#include <QMainWindow>
#include <QWidget>
#include <QImage>
#include <QImageWriter>
#include <QDir>
#include <QDateTime>
#include <QDebug>

DebugScreenshotService::DebugScreenshotService(QMainWindow* mainWindow)
    : m_mainWindow(mainWindow)
{
}

void DebugScreenshotService::setOutputDirectory(const QString& dir)
{
    m_outputDir = dir;
}

QString DebugScreenshotService::outputDirectory() const
{
    return m_outputDir;
}

void DebugScreenshotService::setMaxWidth(int width)
{
    m_maxWidth = width;
}

QString DebugScreenshotService::lastCapturedPath() const
{
    return m_lastCapturedPath;
}

QString DebugScreenshotService::captureMainWindow(const QString& context)
{
    if (!m_mainWindow) {
        qDebug() << "DebugScreenshotService: mainWindow is null";
        return {};
    }

    QImage image = m_mainWindow->grab().toImage();
    if (image.isNull()) {
        qDebug() << "DebugScreenshotService: grab() returned null image";
        return {};
    }

    return saveCapture(image, context.isEmpty() ? QStringLiteral("mainwindow") : context);
}

QString DebugScreenshotService::captureWidget(QWidget* widget, const QString& context)
{
    if (!widget) {
        qDebug() << "DebugScreenshotService: widget is null";
        return {};
    }

    QImage image = widget->grab().toImage();
    if (image.isNull()) {
        qDebug() << "DebugScreenshotService: widget grab() returned null image";
        return {};
    }

    return saveCapture(image, context.isEmpty() ? QStringLiteral("widget") : context);
}

void DebugScreenshotService::clearOutputDirectory()
{
    QDir dir(m_outputDir);
    if (!dir.exists()) return;

    const QStringList pngFiles = dir.entryList({QStringLiteral("*.png")}, QDir::Files);
    for (const QString& file : pngFiles) {
        dir.remove(file);
    }
    qDebug() << "DebugScreenshotService: cleared" << pngFiles.size() << "files from" << m_outputDir;
}

QString DebugScreenshotService::saveCapture(const QImage& image, const QString& context)
{
    QDir dir(m_outputDir);
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            qDebug() << "DebugScreenshotService: failed to create directory" << m_outputDir;
            return {};
        }
    }

    QImage output = image;
    if (output.width() > m_maxWidth) {
        output = output.scaledToWidth(m_maxWidth, Qt::SmoothTransformation);
    }

    const QString filename = generateFilename(context);
    const QString filePath = dir.filePath(filename);

    QImageWriter writer(filePath, "png");
    if (!writer.write(output)) {
        qDebug() << "DebugScreenshotService: failed to write" << filePath << writer.errorString();
        return {};
    }

    m_lastCapturedPath = filePath;
    qDebug() << "DebugScreenshotService: captured" << filePath
             << "(" << output.width() << "x" << output.height() << ")";
    return filePath;
}

QString DebugScreenshotService::generateFilename(const QString& context)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    if (context.isEmpty()) {
        return timestamp + QStringLiteral(".png");
    }
    return timestamp + QStringLiteral("_") + context + QStringLiteral(".png");
}

#endif // QT_DEBUG
