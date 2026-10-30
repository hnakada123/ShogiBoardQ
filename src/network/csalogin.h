#ifndef CSALOGIN_H
#define CSALOGIN_H

/// @file csalogin.h
/// @brief CSA サーバーへのログインの規則（ID・パスワード・ゲーム名）

#include <QString>

#include "shogitypes.h"

#include <optional>

/**
 * @brief CSA サーバーへのログインの規則
 *
 * ID・パスワードは CSA サーバープロトコル 3.1、ゲーム名と手番の希望は shogi-server の書き方に従う。
 * 通信対局のダイアログは check() の結果を文に直すだけで、規則を持たない。
 */
namespace CsaLogin {

/// ログインに使えない入力の種類（check() が最初に見つけたもの）
enum class Issue {
    None,             ///< ログインできる
    MissingHost,      ///< 接続先ホストが空
    MissingId,        ///< ID が空
    MissingPassword,  ///< パスワードが空
    Whitespace,       ///< 接続先ホスト・ID・パスワードに空白や改行がある
    InvalidId,        ///< ID が英数字・「_」・「-」以外を含むか、32文字を超える
    InvalidPassword,  ///< パスワードが半角の英数字・記号以外を含むか、32文字を超える
    InvalidGameName,  ///< ゲーム名が「名前-持時間-秒読み」（末尾の F はフィッシャー）でない
};

/// ログインの入力（接続先ホスト・ID・ゲーム名は前後の空白を除いてから渡す。ゲーム名は空なら希望しない）
struct Credentials {
    QString host;
    QString id;
    QString password;
    QString gameName;
};

/// 入力がログインの規則に合うか。合わなければ最初に見つけた問題を返す
Issue check(const Credentials& credentials);

/**
 * @brief LOGIN に送るパスワード（shogi-server の「ゲーム名[-b|-w],パスワード」）
 * @param gameName 希望するゲーム名（空ならパスワードだけ）
 * @param preferred 希望する手番（-b は先手、-w は後手。無ければ付けない）
 */
QString passwordField(const QString& password, const QString& gameName, std::optional<Turn> preferred);

} // namespace CsaLogin

#endif // CSALOGIN_H
