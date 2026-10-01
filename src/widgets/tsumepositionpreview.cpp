#include "tsumepositionpreview.h"
#include "pieceimageprovider.h"
#include "boardappearance.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"
#include <QPainter>

TsumePositionPreview::TsumePositionPreview(const QString& sfen, QWidget* parent) : QWidget(parent)
{
    m_position.set_sfen(sfen.toStdString(), true);
    setMinimumSize(336, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged, this, &TsumePositionPreview::appearanceChanged);
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged, this, &TsumePositionPreview::appearanceChanged);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged, this, &TsumePositionPreview::appearanceChanged);
}

void TsumePositionPreview::appearanceChanged() { update(); }

void TsumePositionPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    // プレビュー内の文字は固定の24pxマスに合わせる。
    QFont boardFont = font();
    boardFont.setPixelSize(13);
    painter.setFont(boardFont);
    painter.translate((width() - 336) / 2, 0);
    const auto colors = BoardAppearance::instance().colors();
    const auto visuals = BoardAppearance::instance().visuals();
    const bool flipped = m_position.side_to_move() == shogi::Color::White;
    BoardSurfacePainter::draw(painter, QRect(55, 24, 216, 216), colors.board, visuals.woodGrain, 24);
    BoardSurfacePainter::draw(painter, QRect(2, 24, 48, 216), colors.stand, visuals.woodGrain, 24);
    BoardSurfacePainter::draw(painter, QRect(289, 24, 46, 216), colors.stand, visuals.woodGrain, 24);
    painter.setPen(colors.grid);
    for (int i = 0; i <= 9; ++i) {
        painter.drawLine(55 + i * 24, 24, 55 + i * 24, 240);
        painter.drawLine(55, 24 + i * 24, 271, 24 + i * 24);
    }
    painter.setPen(palette().color(QPalette::Text));
    static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
    for (int i = 0; i < 9; ++i) {
        painter.drawText(QRect(55 + i * 24, 2, 24, 20), Qt::AlignCenter, QString::number(flipped ? i + 1 : 9 - i));
        painter.drawText(QRect(272, 24 + i * 24, 16, 24), Qt::AlignCenter, QString(kanjiRanks.at(flipped ? 8 - i : i)));
    }
    static const char codes[] = " PLNSGBRKQMOTCU";
    for (int square = 0; square < 81; ++square) {
        const int piece = m_position.piece_at(square);
        if (!piece) continue;
        int row = shogi::square_row(square), column = shogi::square_col(square);
        if (flipped) { row = 8 - row; column = 8 - column; }
        QChar code = QLatin1Char(codes[static_cast<int>(shogi::piece_type(piece))]);
        if (shogi::piece_color(piece) == shogi::Color::White) code = code.toLower();
        PiecePainter::draw(painter, PieceImageProvider::instance().icon(code, flipped),
                           QRect(56 + column * 24, 25 + row * 24, 22, 22), visuals);
    }
    for (int side = 0; side < 2; ++side) {
        const auto owner = side == 1 ? m_position.side_to_move() : shogi::opposite(m_position.side_to_move());
        const int x = side == 1 ? 290 : 3;
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRect(x, 0, 45, 22), Qt::AlignCenter, side == 1 ? tr("攻方") : tr("玉方"));
        painter.setPen(colors.grid);
        int y = 30;
        for (int kind = 7; kind >= 1; --kind) {
            const int count = m_position.hand_count(owner, static_cast<shogi::PieceType>(kind));
            if (!count) continue;
            QChar code = QLatin1Char(codes[kind]);
            if (owner == shogi::Color::White) code = code.toLower();
            PiecePainter::draw(painter, PieceImageProvider::instance().icon(code, flipped), QRect(x, y, 23, 23), visuals);
            painter.drawText(QRect(x + 24, y, 21, 23), Qt::AlignCenter, QString::number(count));
            y += 29;
        }
        if (y == 30) painter.drawText(QRect(x, y, 45, 23), Qt::AlignCenter, tr("なし"));
    }
}
