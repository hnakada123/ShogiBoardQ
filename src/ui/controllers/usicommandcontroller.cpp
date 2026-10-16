/// @file usicommandcontroller.cpp
/// @brief USIコマンド送信コントローラクラスの実装

#include "usicommandcontroller.h"

#include "logcategories.h"

#include "enginelifecyclemanager.h"
#include "usilogpanel.h"
#include "usi.h"

#include <utility>

UsiCommandController::UsiCommandController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void UsiCommandController::appendStatus(const QString& text)
{
    if (m_deps.usiLog) {
        m_deps.usiLog->appendStatus(text);
    }
}

void UsiCommandController::sendCommand(int target, const QString& command)
{
    qCDebug(lcUi).noquote() << "sendCommand: target=" << target << "command=" << command;

    Usi* usi1 = m_deps.engines ? m_deps.engines->engineInSlot(1) : nullptr;
    Usi* usi2 = m_deps.engines ? m_deps.engines->engineInSlot(2) : nullptr;

    const auto sendIfReady = [this, &command](Usi* engine, const QString& label) {
        if (engine && engine->isRunning()) {
            engine->sendConsoleCommand(command);
            qCDebug(lcUi).noquote() << "Sent to" << label << ":" << command;
        } else {
            qCDebug(lcUi).noquote() << label << "is not available or not running";
            appendStatus(tr("%1: エンジンが起動していません").arg(label));
        }
    };

    if (target == 0 || target == 2) {
        sendIfReady(usi1, QStringLiteral("E1"));
    }
    if (target == 1 || target == 2) {
        sendIfReady(usi2, QStringLiteral("E2"));
    }
}
