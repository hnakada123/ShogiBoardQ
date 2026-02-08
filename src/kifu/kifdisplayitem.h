#ifndef KIFDISPLAYITEM_H
#define KIFDISPLAYITEM_H

/// @file kifdisplayitem.h
/// @brief 棋譜表示用アイテム構造体の定義

#include <QString>
#include <QtGlobal>
#include <QMetaType>
#include <QList>

/**
 * @brief 棋譜の1手分の表示情報を保持する構造体
 *
 * 棋譜一覧やモデルで使用する、指し手・消費時間・コメント・手数のセット。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
struct KifDisplayItem
{
    QString prettyMove; ///< 表示用指し手テキスト（例: "▲７六歩"）
    QString timeText;   ///< 消費時間テキスト
    QString comment;    ///< コメント
    int     ply = 0;    ///< 手数（0始まり）

    KifDisplayItem() = default;

    explicit KifDisplayItem(const QString& move,
                            const QString& time = QString(),
                            const QString& cmt  = QString(),
                            int plyNumber       = 0)
        : prettyMove(move), timeText(time), comment(cmt), ply(plyNumber) {}

    bool operator==(const KifDisplayItem& o) const noexcept {
        return prettyMove == o.prettyMove && timeText == o.timeText
               && comment == o.comment && ply == o.ply;
    }
    bool operator!=(const KifDisplayItem& o) const noexcept { return !(*this == o); }
};

// メタタイプ宣言はこのヘッダに1か所だけ配置する
Q_DECLARE_METATYPE(KifDisplayItem)
Q_DECLARE_METATYPE(QList<KifDisplayItem>)

#endif // KIFDISPLAYITEM_H
