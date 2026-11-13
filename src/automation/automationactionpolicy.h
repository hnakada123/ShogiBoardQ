#ifndef AUTOMATIONACTIONPOLICY_H
#define AUTOMATIONACTIONPOLICY_H

/// @file automationactionpolicy.h
/// @brief 自動化 API から実行してよい QAction の許可リスト

#include <QString>
#include <QStringList>

/**
 * @brief `action.trigger` の許可リスト
 *
 * 終了・上書き保存・外部サイトを開くなど、ユーザーの意図なしに実行すべきでない動作は含めない。
 * 言語の切り替え（再起動後に反映）とドックレイアウトの保存・リセットは含める。
 */
namespace AutomationActionPolicy {

/// 許可された QAction の objectName（mainwindow.ui の定義順）
const QStringList& allowedActions();

bool isAllowed(const QString& objectName);

/// 自動化 API から P2P対局を操作してよいか。ガイドの画像を撮るためのビルド（CMake の
/// ENABLE_P2P_DOCS_AUTOMATION）で、しかも撮影の環境だと環境変数 SHOGIBOARDQ_P2P_DOCS_LAB=1 で明示して起動した
/// ときだけ true。ふだんのビルドでは、ローカルのソケットから招待・対局・確定を始めさせないため false
/// （docs/dev/p2p-game-spec.md 11章）。撮影用のビルドを誤って配っても、ふだんの起動では動かせない（監査 2026-11-13 I-e）
bool allowsP2pPlay();

/// 撮影の環境だと明示する環境変数（値は "1"）
inline constexpr char kP2pDocsLabVariable[] = "SHOGIBOARDQ_P2P_DOCS_LAB";

} // namespace AutomationActionPolicy

#endif // AUTOMATIONACTIONPOLICY_H
