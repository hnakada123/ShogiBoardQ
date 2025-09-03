#ifndef KIFDISPLAYITEM_H
#define KIFDISPLAYITEM_H

#include <QString>
#include <QtGlobal>
#include <QMetaType>   // ★ これが無いと Q_DECLARE_METATYPE が未定義でエラー

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

Q_DECLARE_METATYPE(KifDisplayItem)

#endif // KIFDISPLAYITEM_H
