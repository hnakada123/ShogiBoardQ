/// @file automationendpoint.cpp
/// @brief 自動化 API の接続先の実装

#include "automationendpoint.h"

#include "appidentity.h"
#include "settingscommon.h"

#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>

QString AutomationEndpoint::defaultSocketPath()
{
    QString env = qEnvironmentVariable("SHOGIBOARDQ_AUTOMATION_SOCKET");
    if (!env.isEmpty()) return env;
#ifdef Q_OS_WIN
    // 名前付きパイプの名前を予測させない（別の利用者に先に作られると待ち受けを奪われる。セキュリティ監査 V33）。
    // アプリを探す側はエンドポイントファイルの名前を使う。同じプロセスの中では同じ名前を返す
    static const QString name = [] {
        const QString user = qEnvironmentVariable("USERNAME", QStringLiteral("user"));
        return QStringLiteral("%1-automation-%2-%3")
            .arg(QString::fromLatin1(AppIdentity::kRuntimeName), user, QString::number(QRandomGenerator::system()->generate64(), 16));
    }();
    return name;
#else
    // RuntimeLocation（XDG_RUNTIME_DIR など）は所有者だけが使える。使えなければ設定のフォルダに置き、
    // 共有の一時ディレクトリの固定名は使わない（別の利用者に先に作られると待ち受けを奪われる。セキュリティ監査 S10）
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) return QFileInfo(endpointFilePath()).dir().filePath(QStringLiteral("automation.sock"));
    return runtime + QLatin1Char('/') + QString::fromLatin1(AppIdentity::kRuntimeName)
        + QStringLiteral("/automation.sock");
#endif
}

QString AutomationEndpoint::endpointFilePath()
{
    return QFileInfo(SettingsCommon::settingsFilePath()).dir().filePath(QStringLiteral("automation-endpoint.json"));
}
