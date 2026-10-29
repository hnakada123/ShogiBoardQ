/// @file automationcommands_screenshot.cpp
/// @brief 自動化 API の screenshot.* メソッド（画面の画像の保存）の実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationwidgets.h"
#include "screenshotservice.h"

#include <QFileInfo>
#include <QImage>
#include <QLineEdit>
#include <QMainWindow>

void AutomationCommands::registerScreenshotCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("screenshot.capture"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        const QString outputDir = params.value(QStringLiteral("output_dir")).toString();
        QWidget* window = AutomationWidgets::requireWindow(context, target);
        // A revealed secret must not leak through a screenshot of this window or a child dialog.
        for (QWidget* visible : AutomationWidgets::visibleWindows()) {
            const auto edits = visible->findChildren<QLineEdit*>();
            for (auto* edit : edits) {
                if (edit->isVisible() && AutomationWidgets::isSensitive(edit)
                    && edit->echoMode() == QLineEdit::Normal && !edit->text().isEmpty())
                    throw AutomationError(ErrorKind::NotAllowed,
                                          QStringLiteral("Hide sensitive text before taking a screenshot"));
            }
        }
        ScreenshotService service(context.mainWindow);
        if (!outputDir.isEmpty()) {
            if (!QFileInfo(outputDir).isAbsolute()) {
                throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("output_dir must be absolute"));
            }
            service.setOutputDirectory(outputDir);
        }
        const QString label = window == context.mainWindow ? QStringLiteral("main")
                                                            : (window->objectName().isEmpty() ? window->windowTitle() : window->objectName());
        const QString path = window == context.mainWindow ? service.captureMainWindow(label)
                                                           : service.captureWidget(window, label);
        if (path.isEmpty()) {
            throw AutomationError(ErrorKind::InternalError, QStringLiteral("Capturing the screenshot failed"));
        }
        const QImage image(path);
        QJsonObject result;
        result[QStringLiteral("path")] = path;
        result[QStringLiteral("width")] = image.width();
        result[QStringLiteral("height")] = image.height();
        result[QStringLiteral("target")] = label;
        return result;
    });
}
