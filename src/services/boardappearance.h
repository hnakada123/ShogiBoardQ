#ifndef BOARDAPPEARANCE_H
#define BOARDAPPEARANCE_H

#include "boardcolors.h"
#include <QObject>

/// 全盤面に共通する配色の保存と変更通知。
class BoardAppearance : public QObject
{
    Q_OBJECT
public:
    static BoardAppearance& instance();
    BoardColors colors() const;
    void setColors(const BoardColors& colors);

signals:
    void colorsChanged();

private:
    BoardAppearance() = default;
};

#endif // BOARDAPPEARANCE_H
