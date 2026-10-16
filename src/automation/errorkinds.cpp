/// @file errorkinds.cpp
/// @brief エラーの分類の表

#include "errorkinds.h"

#include "automationdispatcher.h"

#include <QJsonObject>

namespace {
struct Entry {
    ErrorKind kind;
    const char* slug;
    int rpcCode;
};

// 分類ごとの語と JSON-RPC のコード（並びは ErrorKind と同じ）
constexpr Entry kEntries[] = {
    {ErrorKind::InvalidJson, "invalid_json", AutomationErrorCode::ParseError},
    {ErrorKind::InvalidRequest, "invalid_request", AutomationErrorCode::InvalidRequest},
    {ErrorKind::UnknownOperation, "unknown_operation", AutomationErrorCode::MethodNotFound},
    {ErrorKind::InvalidArgument, "invalid_argument", AutomationErrorCode::InvalidParams},
    {ErrorKind::InternalError, "internal_error", AutomationErrorCode::InternalError},
    {ErrorKind::NotAllowed, "not_allowed", AutomationErrorCode::NotAllowed},
    {ErrorKind::InvalidState, "invalid_state", AutomationErrorCode::InvalidState},
    {ErrorKind::FileError, "file_error", AutomationErrorCode::FileError},
    {ErrorKind::FileExists, "file_exists", AutomationErrorCode::FileError},
    {ErrorKind::UnsavedChanges, "unsaved_changes", AutomationErrorCode::UnsavedChanges},
    {ErrorKind::NotFound, "not_found", AutomationErrorCode::NotFound},
    {ErrorKind::InvalidContent, "parse_error", AutomationErrorCode::InvalidContent},
    {ErrorKind::EngineNotFound, "engine_not_found", AutomationErrorCode::NotFound},
    {ErrorKind::EngineError, "engine_error", AutomationErrorCode::EngineError},
    {ErrorKind::RenderError, "render_error", AutomationErrorCode::InternalError},
};

const Entry& entry(ErrorKind kind)
{
    const Entry& found = kEntries[static_cast<int>(kind)];
    Q_ASSERT(found.kind == kind);
    return found;
}
} // namespace

QString ErrorKinds::slug(ErrorKind kind)
{
    return QString::fromLatin1(entry(kind).slug);
}

int ErrorKinds::rpcCode(ErrorKind kind)
{
    return entry(kind).rpcCode;
}

QJsonArray ErrorKinds::toJson()
{
    QJsonArray kinds;
    for (const Entry& e : kEntries)
        kinds.append(QJsonObject{{QStringLiteral("slug"), QString::fromLatin1(e.slug)},
                                 {QStringLiteral("rpc_code"), e.rpcCode}});
    return kinds;
}
