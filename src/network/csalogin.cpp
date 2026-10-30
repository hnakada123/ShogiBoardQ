/// @file csalogin.cpp
/// @brief CSA サーバーへのログインの規則の実装

#include "csalogin.h"

#include <QRegularExpression>

namespace CsaLogin {

Issue check(const Credentials& credentials)
{
    if (credentials.host.isEmpty()) return Issue::MissingHost;
    if (credentials.id.isEmpty()) return Issue::MissingId;
    if (credentials.password.isEmpty()) return Issue::MissingPassword;
    static const QRegularExpression whitespace(QStringLiteral("\\s"));
    if (credentials.host.contains(whitespace) || credentials.id.contains(whitespace)
        || credentials.password.contains(whitespace)) {
        return Issue::Whitespace;
    }
    // CSA サーバープロトコル 3.1: ID は英数字・「_」・「-」で32バイト以内、
    // パスワードは空白を含まない32バイト以内の7ビット ASCII
    static const QRegularExpression idPattern(QStringLiteral("^[0-9A-Za-z_-]{1,32}$"));
    if (!idPattern.match(credentials.id).hasMatch()) return Issue::InvalidId;
    static const QRegularExpression passwordPattern(QStringLiteral("^[\\x21-\\x7e]{1,32}$"));
    if (!passwordPattern.match(credentials.password).hasMatch()) return Issue::InvalidPassword;
    // shogi-server のゲーム名（名前-持時間-秒読み、末尾の F はフィッシャー）
    static const QRegularExpression gameNamePattern(QStringLiteral("^[0-9A-Za-z_@.\\-]+-\\d+-\\d+F?$"));
    if (!credentials.gameName.isEmpty() && !gameNamePattern.match(credentials.gameName).hasMatch())
        return Issue::InvalidGameName;
    return Issue::None;
}

QString passwordField(const QString& password, const QString& gameName, std::optional<Turn> preferred)
{
    if (gameName.isEmpty()) return password;
    QString token = gameName;
    if (preferred) token += *preferred == Turn::Black ? QStringLiteral("-b") : QStringLiteral("-w");
    return token + QLatin1Char(',') + password;
}

} // namespace CsaLogin
