#ifndef PLAINTOOLTIP_H
#define PLAINTOOLTIP_H

/// @file plaintooltip.h
/// @brief 外から来た文字列を、文字のまま出すツールチップ

#include <QString>
#include <QStringList>

/**
 * @brief 文字列を、HTML として読ませずに文字のまま出すツールチップにする
 *
 * Qt のツールチップは、タグらしいものがあると HTML として描く（`<img>` でローカルのファイルを
 * 読ませることもできる。セキュリティ監査 S12）。棋譜のコメント・対局情報・定跡ファイルの
 * コメント・エンジン名など、外から来た文字列をツールチップに出すときはこれを通す。
 * 文字はエスケープし、空白と改行もそのまま出す（`white-space:pre-wrap`）。空の文字列は空のまま
 * （ツールチップを出さない）。
 */
QString plainToolTip(const QString& text);

/// 区切り線で分けた段を、どれも文字のまま出すツールチップにする（空の段は飛ばす。すべて空なら空）
QString plainToolTip(const QStringList& sections);

/// ツールチップとして利用者に見える文字（Qt と同じく、タグらしいものがあれば HTML として描いた文字。
/// 自動化 API が部品のツールチップを返すときに使う）
QString shownToolTipText(const QString& toolTip);

#endif // PLAINTOOLTIP_H
