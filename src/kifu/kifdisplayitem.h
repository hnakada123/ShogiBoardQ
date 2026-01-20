#ifndef KIFDISPLAYITEM_H
#define KIFDISPLAYITEM_H

#include <QString>
#include <QtGlobal>
#include <QMetaType>
#include <QList>     // ← 追加

struct KifDisplayItem
{
    QString prettyMove;
    QString timeText;
    QString comment;
    int     ply = 0;

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

// ★ メタタイプ宣言はこのヘッダに「1か所だけ」
Q_DECLARE_METATYPE(KifDisplayItem)
Q_DECLARE_METATYPE(QList<KifDisplayItem>)   // ← これが無いと QList<...> の接続でコケます

#endif // KIFDISPLAYITEM_H
