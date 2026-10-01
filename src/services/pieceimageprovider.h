#ifndef PIECEIMAGEPROVIDER_H
#define PIECEIMAGEPROVIDER_H

#include <QObject>
#include <QIcon>
#include <QHash>

/// 全盤面で共有する駒画像の選択と変更通知。
class PieceImageProvider : public QObject
{
    Q_OBJECT
public:
    static PieceImageProvider& instance();

    QString style() const;
    void setStyle(const QString& style);
    QIcon icon(QChar piece, bool flipped = false) const;
    QIcon iconForStyle(QChar piece, const QString& style, bool flipped = false) const;

signals:
    void styleChanged();

private:
    PieceImageProvider() = default;
    mutable QHash<QString, QIcon> m_icons;
};

#endif // PIECEIMAGEPROVIDER_H
