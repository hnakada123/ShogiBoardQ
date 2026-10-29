/// @file automationcommands_clipboard.cpp
/// @brief 自動化 API の clipboard.* メソッド（クリップボードの読み取り）の実装

#include "automationcommands.h"
#include "automationdispatcher.h"

#include <QApplication>
#include <QClipboard>
#include <QImage>

void AutomationCommands::registerClipboardCommands(AutomationDispatcher& dispatcher, const AutomationContext& /*context*/)
{
    dispatcher.registerMethod(QStringLiteral("clipboard.get"), [](const QJsonObject& params) {
        const int maxChars = params.value(QStringLiteral("max_chars")).toInt();
        const auto* clipboard = QApplication::clipboard();
        const QString text = clipboard->text();
        const QImage image = clipboard->image();
        return QJsonObject{{QStringLiteral("text"), text.left(maxChars)},
                           {QStringLiteral("truncated"), text.size() > maxChars},
                           {QStringLiteral("has_image"), !image.isNull()},
                           {QStringLiteral("image_width"), image.width()},
                           {QStringLiteral("image_height"), image.height()}};
    });
}
