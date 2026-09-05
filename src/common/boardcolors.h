#ifndef BOARDCOLORS_H
#define BOARDCOLORS_H

#include <QColor>

/// 盤面の配色。初期値は従来の色を保持する。
struct BoardColors {
    QColor background{200, 190, 130};
    QColor board{228, 203, 115};
    QColor stand{228, 167, 46};
    QColor grid{80, 60, 30};

    BoardColors normalized() const
    {
        const BoardColors defaults;
        const auto opaque = [](const QColor& color, const QColor& fallback) {
            return color.isValid() ? QColor(color.red(), color.green(), color.blue()) : fallback;
        };
        return {opaque(background, defaults.background), opaque(board, defaults.board),
                opaque(stand, defaults.stand), opaque(grid, defaults.grid)};
    }

    QColor backgroundText(const QColor& dark = QColor(40, 30, 20)) const
    {
        const int brightness = (background.red() * 299 + background.green() * 587
                                + background.blue() * 114) / 1000;
        return brightness < 128 ? QColor(Qt::white) : dark;
    }

    bool operator==(const BoardColors& other) const
    {
        return background == other.background && board == other.board
            && stand == other.stand && grid == other.grid;
    }
    bool operator!=(const BoardColors& other) const { return !(*this == other); }
};

#endif // BOARDCOLORS_H
