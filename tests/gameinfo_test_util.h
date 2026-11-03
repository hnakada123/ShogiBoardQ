#ifndef GAMEINFO_TEST_UTIL_H
#define GAMEINFO_TEST_UTIL_H

/// @file gameinfo_test_util.h
/// @brief 棋譜の文書の対局情報を見出しで引く（テスト用）

#include <QString>

#include "gameinfokeys.h"
#include "kifudocument.h"

/// 文書の対局情報の値（見出しが無ければ空）。持ち時間と開始・終了の日時は対局情報にだけ書く
inline QString headerValue(const KifuDocument& document, const QString& key)
{
    const QList<KifGameInfoItem> header = document.header();
    for (const KifGameInfoItem& item : header) {
        if (item.key == key) return item.value;
    }
    return {};
}

#endif // GAMEINFO_TEST_UTIL_H
