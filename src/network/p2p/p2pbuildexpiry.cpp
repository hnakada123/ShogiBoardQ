/// @file p2pbuildexpiry.cpp
/// @brief ビルドの使用期限の実装

#include "p2pbuildexpiry.h"

#include <QDateTime>

#include "p2pbuildinfo.h"

namespace P2pBuildExpiry {

qint64 expiresAt()
{
    return kP2pBuildEpochSeconds + kP2pBuildLifetimeDays * 24 * 60 * 60;
}

bool isExpiredAt(qint64 nowSeconds)
{
    return nowSeconds > expiresAt();
}

bool isExpired()
{
    return isExpiredAt(QDateTime::currentSecsSinceEpoch());
}

} // namespace P2pBuildExpiry
