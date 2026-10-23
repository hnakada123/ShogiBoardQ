/// @file tsumepositionpreview.cpp
/// @brief 詰将棋問題一覧の初期局面の小さな表示の実装

#include "tsumepositionpreview.h"

#include "boardappearance.h"
#include "kifupresentation.h"
#include "movefont.h"
#include "pieceimageprovider.h"
#include "piecepainter.h"

#include <QPainter>

TsumePositionPreview::TsumePositionPreview(const QString& sfen, QWidget* parent)
    : QWidget(parent)
    , m_position(Position::fromSfenLike(sfen).value_or(Position{}))
{
    BoardGeometry::Params params;
    params.squareSize = kSquareSize;
    params.flipped = m_position.sideToMove() == Turn::White;   // 攻方を手前にする
    params.font = font();
    params.standColumns = kStandColumns;
    params.standRows = kStandRows;
    m_geometry = BoardGeometry(params);
    setMinimumSize(sizeHint());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged, this, &TsumePositionPreview::appearanceChanged);
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged, this, &TsumePositionPreview::appearanceChanged);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged, this, &TsumePositionPreview::appearanceChanged);
}

QSize TsumePositionPreview::sizeHint() const
{
    return m_geometry.viewSize() + QSize(0, kHeaderHeight);
}

void TsumePositionPreview::appearanceChanged() { update(); }

void TsumePositionPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QSize view = m_geometry.viewSize();
    painter.translate((width() - view.width()) / 2, 0);

    // 翻訳された役割名は駒台幅に収めず、筋番号と別の行で表示する（玉方の駒台は左、攻方は右）
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRect(2, 0, view.width() / 2 - 2, kHeaderHeight), Qt::AlignLeft | Qt::AlignVCenter, tr("玉方"));
    painter.drawText(QRect(view.width() / 2, 0, view.width() / 2 - 2, kHeaderHeight), Qt::AlignRight | Qt::AlignVCenter,
                     tr("攻方"));
    painter.translate(0, kHeaderHeight);

    // 盤・駒台は盤と同じ配色と木目。周りは盤の背景色で塗らず、カードの地のままにする
    BoardColors colors = BoardAppearance::instance().colors();
    colors.background = Qt::transparent;
    const BoardVisuals visuals = BoardAppearance::instance().visuals();
    m_staticLayer.draw(painter, view, m_geometry, colors, visuals);

    const bool flipped = m_geometry.flipped();
    for (int rank = 1; rank <= Position::kRanks; ++rank) {
        for (int file = 1; file <= Position::kFiles; ++file) {
            const Piece piece = m_position.pieceAt(file, rank);
            if (piece != Piece::None)
                PiecePainter::draw(painter, PieceImageProvider::instance().icon(piece, flipped),
                                   m_geometry.square(file, rank), visuals);
        }
    }
    for (const Turn side : {Turn::Black, Turn::White}) drawHand(painter, side, colors, visuals);

    // 筋・段の文字（盤と同じ帯）
    QFont font = painter.font();
    font.setPixelSize(m_geometry.labelPixelSize());
    painter.setPen(colors.grid);
    painter.setFont(MoveFont::font(font));   // 段の漢数字は指し手と同じ書体
    for (int rank = 1; rank <= Position::kRanks; ++rank)
        painter.drawText(m_geometry.rankLabel(rank), Qt::AlignCenter, KifuPresentation::rankLabel(rank));
    painter.setFont(font);
    for (int file = 1; file <= Position::kFiles; ++file)
        painter.drawText(m_geometry.fileLabel(file), Qt::AlignCenter, QString::number(file));
}

void TsumePositionPreview::drawHand(QPainter& painter, Turn side, const BoardColors& colors, const BoardVisuals& visuals)
{
    const QRect stand = m_geometry.stand(side);
    const QSize field = m_geometry.fieldSize();
    QFont font = painter.font();
    font.setPixelSize(qRound(field.width() * 0.58));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(colors.grid);
    int row = 0;
    for (const Piece piece : BoardGeometry::standPieces(side)) {
        const int count = m_position.handCount(piece);
        if (count <= 0) continue;
        const QRect line(stand.left(), stand.top() + row * field.height(), stand.width(), field.height());
        const QRect pieceRect(line.left(), line.top(), field.width(), field.height());
        PiecePainter::draw(painter, PieceImageProvider::instance().icon(piece, m_geometry.flipped()),
                           pieceRect.adjusted(1, 1, -1, -1), visuals);
        painter.drawText(line.adjusted(field.width(), 0, -1, 0), Qt::AlignCenter, QString::number(count));
        ++row;
    }
    if (row == 0) painter.drawText(QRect(stand.topLeft(), QSize(stand.width(), field.height())), Qt::AlignCenter, tr("なし"));
}
