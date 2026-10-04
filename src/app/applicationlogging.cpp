/// @file applicationlogging.cpp
/// @brief ログハンドラの登録・ファイル出力・終了処理

#include "applicationlogging.h"

#ifdef QT_DEBUG
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstring>
#endif

namespace {

#ifdef QT_DEBUG
struct LogState
{
    QMutex mutex;
    QFile file;
};

LogState& logState()
{
    // ハンドラの復元直前に呼び出されたコールバックも参照するため、
    // ApplicationLogging の破棄後も状態とロックを生存させる。
    static LogState state;
    return state;
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    auto& state = logState();
    const QMutexLocker lock(&state.mutex);
    if (!state.file.isOpen()) return;

    QTextStream out(&state.file);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO"; break;
    case QtWarningMsg:  level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }

    QString category;
    if (context.category && qstrcmp(context.category, "default") != 0) {
        category = QString(" [%1]").arg(context.category);
    }

    QString location;
    if (context.file) {
        const char* file = context.file;
        if (const char* slash = std::strrchr(file, '/'))
            file = slash + 1;
        else if (const char* bslash = std::strrchr(file, '\\'))
            file = bslash + 1;
        location = QString(" %1:%2").arg(file).arg(context.line);
        if (context.function)
            location += QString(" (%1)").arg(context.function);
    }

    out << timestamp << " [" << level << "]" << category << location << " " << msg << "\n";
    out.flush();
}
#else
// Release ではファイル作成も stderr 出力も行わない。
void messageHandler(QtMsgType, const QMessageLogContext&, const QString&) {}
#endif

} // namespace

ApplicationLogging::ApplicationLogging(const QString& filePath)
{
#ifdef QT_DEBUG
    auto& state = logState();
    const QMutexLocker lock(&state.mutex);
    // アプリ全体で1つだけ登録する。既存の管理オブジェクトを上書きしない。
    if (state.file.isOpen()) return;
    state.file.setFileName(filePath);
    // 開けない場合は従来のハンドラを維持する。
    if (!state.file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
#else
    Q_UNUSED(filePath)
#endif
    m_previousHandler = qInstallMessageHandler(messageHandler);
    m_installed = true;
}

ApplicationLogging::~ApplicationLogging()
{
    if (!m_installed) return;
#ifdef QT_DEBUG
    auto& state = logState();
    // 書き込み完了を待ち、遅れて入ったコールバックには閉じた状態を見せる。
    const QMutexLocker lock(&state.mutex);
#endif
    qInstallMessageHandler(m_previousHandler);
#ifdef QT_DEBUG
    state.file.close();
#endif
}
