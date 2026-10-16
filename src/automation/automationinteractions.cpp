/// @file automationinteractions.cpp
/// @brief 自動化 API の、応答の後に行う UI 操作の実装

#include "automationinteractions.h"

#include "automationcommands.h"
#include "automationdispatcher.h"

#include <map>

namespace AutomationInteractions {

namespace {

enum class State { Pending, Running, Done, Failed, Cancelled };

struct Record {
    State state = State::Pending;
    QPointer<QObject> target;
    QJsonObject error;
};

// 自動化 API はメインスレッドだけで使う
std::map<int, Record>& records()
{
    static std::map<int, Record> instance;
    return instance;
}

int& nextId()
{
    static int id = 0;
    return id;
}

QString stateName(State state)
{
    switch (state) {
    case State::Pending: return QStringLiteral("pending");
    case State::Running: return QStringLiteral("running");
    case State::Done: return QStringLiteral("done");
    case State::Failed: return QStringLiteral("failed");
    case State::Cancelled: return QStringLiteral("cancelled");
    }
    return {};
}

void setState(int id, State state, const QJsonObject& error = {})
{
    const auto found = records().find(id);
    if (found == records().end()) return;
    found->second.state = state;
    found->second.error = error;
}

QJsonObject errorJson(ErrorKind kind, const QString& message, const QString& hint = QString())
{
    QJsonObject error = AutomationDispatcher::makeError(QJsonValue(), kind, message, hint)
                            .value(QStringLiteral("error")).toObject();
    return error;
}

void run(int id, const QueuedInteraction& interaction)
{
    if (!interaction.target) {
        setState(id, State::Cancelled,
                 errorJson(ErrorKind::InvalidState, QStringLiteral("The target was closed before the operation ran")));
        return;
    }
    try {
        if (interaction.check) interaction.check();
        setState(id, State::Running);
        interaction.perform();
        setState(id, State::Done);
    } catch (const AutomationError& error) {
        setState(id, State::Failed, errorJson(error.kind(), error.message(), error.hint()));
    }
}

} // namespace

QJsonObject queue(QueuedInteraction interaction, QJsonObject response)
{
    const int id = ++nextId();
    auto& all = records();
    all[id].target = interaction.target;
    while (all.size() > static_cast<std::size_t>(kHistoryLimit)) all.erase(all.begin());

    QObject* parent = interaction.target.data();
    // 対象が消えると予約も消える（実行しない）。その場合の状態は status() が cancelled にする
    AutomationDeferredCall::schedule([id, interaction = std::move(interaction)]() { run(id, interaction); }, parent);
    response[QStringLiteral("queued")] = true;
    response[QStringLiteral("interaction")] = id;
    return response;
}

QJsonObject status(int id)
{
    const auto found = records().find(id);
    if (found == records().end())
        throw AutomationError(ErrorKind::NotFound, QStringLiteral("No queued operation %1").arg(id),
                              QStringLiteral("Only the latest %1 operations are kept").arg(kHistoryLimit));
    Record& record = found->second;
    if (record.state == State::Pending && !record.target) {
        record.state = State::Cancelled;
        record.error = errorJson(ErrorKind::InvalidState, QStringLiteral("The target was closed before the operation ran"));
    }
    QJsonObject result{{QStringLiteral("interaction"), id}, {QStringLiteral("state"), stateName(record.state)}};
    if (!record.error.isEmpty()) result[QStringLiteral("error")] = record.error;
    return result;
}

} // namespace AutomationInteractions
