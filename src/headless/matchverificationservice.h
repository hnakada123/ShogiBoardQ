#ifndef MATCHVERIFICATIONSERVICE_H
#define MATCHVERIFICATIONSERVICE_H

/// @file matchverificationservice.h
/// @brief P2P対局の証跡ファイルの検証と、その結果の JSON（shogiboardq-cli verify-match。実装仕様 9章）

#include <QJsonObject>
#include <QString>

#include <variant>

#include "errorkinds.h"

namespace MatchVerificationService {

/**
 * @brief 証跡ファイルを上限つきで読み、検証した結果を JSON にする
 *
 * 他人から受け取ったファイルは信頼しない入力として、上限（8 MiB）を確かめてから決定的CBORの読み取りだけで読む。
 * ネットワークは使わない。ファイルが読めない・証跡の形でないときは Failure を返す（検証に失敗した証跡は
 * 失敗ではなく、結果の JSON の valid が false になる）。
 */
std::variant<QJsonObject, Failure> verifyFile(const QString& path);

} // namespace MatchVerificationService

#endif // MATCHVERIFICATIONSERVICE_H
