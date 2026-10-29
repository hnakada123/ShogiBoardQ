#ifndef AUTOMATIONENDPOINT_H
#define AUTOMATIONENDPOINT_H

/// @file automationendpoint.h
/// @brief 自動化 API の接続先（ソケットの場所・エンドポイントファイル・1メッセージの上限）

#include <QString>

/**
 * @brief 自動化 API の接続先の決まり
 *
 * 待ち受ける側（automation の AutomationServer）と、それを MCP サーバーに教える側（CLI の environment）で共有する。
 */
namespace AutomationEndpoint {

/// 1 メッセージ（1 行）の上限（棋譜テキストを含む。MCP サーバーは shogiboardq-cli environment で読む）
inline constexpr qsizetype kMaxMessageBytes = 8 * 1024 * 1024;

/// 既定のソケットパス（環境変数 SHOGIBOARDQ_AUTOMATION_SOCKET → RuntimeLocation/shogiboardq/automation.sock。
/// RuntimeLocation が使えなければ設定のフォルダの automation.sock。Windows は名前付きパイプ名）
QString defaultSocketPath();
/// エンドポイント情報ファイル（設定ディレクトリの automation-endpoint.json）
QString endpointFilePath();

} // namespace AutomationEndpoint

#endif // AUTOMATIONENDPOINT_H
