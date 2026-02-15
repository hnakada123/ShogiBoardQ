/// @file test_stubs.cpp
/// @brief テスト用スタブ - 本体モジュールが定義するシンボルの代替定義
///
/// テスト実行ファイルが依存する最小限のシンボルを提供する。
/// 本体の巨大な依存ツリーを回避するために使用。

#include <QLoggingCategory>

// === ログカテゴリ ===
Q_LOGGING_CATEGORY(lcGame, "shogi.game")
Q_LOGGING_CATEGORY(lcKifu, "shogi.kifu")
