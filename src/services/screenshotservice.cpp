/// @file screenshotservice.cpp
/// @brief スクリーンショットサービスの実装

#include "screenshotservice.h"
#include "logcategories.h"

#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QImageWriter>
#include <QMainWindow>
#include <QWidget>

ScreenshotService::ScreenshotService(QMainWindow* mainWindow)
    : m_mainWindow(mainWindow)
    , m_outputDir(defaultOutputDirectory())
{
}

QString ScreenshotService::defaultOutputDirectory()
{
    return QDir::temp().filePath(QStringLiteral("shogiboardq-screenshots"));
}

void ScreenshotService::setOutputDirectory(const QString& dir)
{
    m_outputDir = dir;
}

QString ScreenshotService::captureMainWindow(const QString& context)
{
    if (!m_mainWindow) {
        qCWarning(lcUi) << "ScreenshotService: mainWindow is null";
        return {};
    }
    const QImage image = m_mainWindow->grab().toImage();
    if (image.isNull()) {
        qCWarning(lcUi) << "ScreenshotService: grab() returned null image";
        return {};
    }
    return saveCapture(image, context.isEmpty() ? QStringLiteral("mainwindow") : context);
}

QString ScreenshotService::captureWidget(QWidget* widget, const QString& context)
{
    if (!widget) {
        qCWarning(lcUi) << "ScreenshotService: widget is null";
        return {};
    }
    const QImage image = widget->grab().toImage();
    if (image.isNull()) {
        qCWarning(lcUi) << "ScreenshotService: widget grab() returned null image";
        return {};
    }
    return saveCapture(image, context.isEmpty() ? QStringLiteral("widget") : context);
}

QString ScreenshotService::saveCapture(const QImage& image, const QString& context)
{
    QDir dir(m_outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qCWarning(lcUi) << "ScreenshotService: failed to create directory" << m_outputDir;
        return {};
    }

    QImage output = image;
    if (output.width() > kMaxWidth) {
        output = output.scaledToWidth(kMaxWidth, Qt::SmoothTransformation);
    }

    QString filePath = dir.filePath(generateFilename(context));
    QImageWriter writer(filePath, "png");
    if (!writer.write(output)) {
        qCWarning(lcUi) << "ScreenshotService: failed to write" << filePath << writer.errorString();
        return {};
    }

    qCDebug(lcUi) << "ScreenshotService: captured" << filePath
                  << "(" << output.width() << "x" << output.height() << ")";
    return filePath;
}

QString ScreenshotService::generateFilename(const QString& context)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    // ファイル名に使えない文字は置き換える
    QString safe = context;
    static const QString forbidden = QStringLiteral("/\\:*?\"<>| ");
    for (QChar& ch : safe) {
        if (forbidden.contains(ch)) ch = QLatin1Char('_');
    }
    if (safe.isEmpty()) return timestamp + QStringLiteral(".png");
    return timestamp + QStringLiteral("_") + safe.left(60) + QStringLiteral(".png");
}
