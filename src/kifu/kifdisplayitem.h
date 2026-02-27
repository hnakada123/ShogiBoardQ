#ifndef KIFDISPLAYITEM_H
#define KIFDISPLAYITEM_H

/// @file kifdisplayitem.h
/// @brief 棋譜表示用アイテム構造体の定義

#include <QString>
#include <QtGlobal>
#include <QMetaType>
#include <QList>

#include <utility>

/**
 * @brief 棋譜の1手分の表示情報を保持する構造体
 *
 * 棋譜一覧やモデルで使用する、指し手・消費時間・コメント・手数のセット。
 *
 */
struct KifDisplayItem
{
    QString prettyMove; ///< 表示用指し手テキスト（例: "▲７六歩"）
    QString timeText;   ///< 消費時間テキスト
    QString comment;    ///< コメント
    QString bookmark;   ///< しおり
    int     ply = 0;    ///< 手数（0始まり）

    KifDisplayItem() = default;

    explicit KifDisplayItem(QString move,
                            QString time = QString(),
                            QString cmt  = QString(),
                            int plyNumber       = 0)
        : prettyMove(std::move(move)), timeText(std::move(time)), comment(std::move(cmt)), ply(plyNumber) {}

    bool operator==(const KifDisplayItem& o) const noexcept {
        return prettyMove == o.prettyMove && timeText == o.timeText
               && comment == o.comment && bookmark == o.bookmark && ply == o.ply;
    }
    bool operator!=(const KifDisplayItem& o) const noexcept { return !(*this == o); }
};

// メタタイプ宣言はこのヘッダに1か所だけ配置する
Q_DECLARE_METATYPE(KifDisplayItem)
Q_DECLARE_METATYPE(QList<KifDisplayItem>)

#endif // KIFDISPLAYITEM_H
