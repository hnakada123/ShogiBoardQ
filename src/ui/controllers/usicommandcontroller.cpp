/// @file usicommandcontroller.cpp
/// @brief USIコマンド送信コントローラクラスの実装

#include "usicommandcontroller.h"

#include "logcategories.h"

#include "matchcoordinator.h"
#include "engineanalysistab.h"
#include "usi.h"

UsiCommandController::UsiCommandController(QObject* parent)
    : QObject(parent)
{
}

void UsiCommandController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void UsiCommandController::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}

void UsiCommandController::appendStatus(const QString& text)
{
    if (m_analysisTab) {
        m_analysisTab->appendUsiLogStatus(text);
    }
}

void UsiCommandController::sendCommand(int target, const QString& command)
{
    qCDebug(lcUi).noquote() << "sendCommand: target=" << target << "command=" << command;

    Usi* usi1 = m_match ? m_match->primaryEngine() : nullptr;
    Usi* usi2 = m_match ? m_match->secondaryEngine() : nullptr;

    const auto sendIfReady = [this, &command](Usi* engine, const QString& label) {
        if (engine && engine->isEngineRunning()) {
            engine->sendRaw(command);
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
