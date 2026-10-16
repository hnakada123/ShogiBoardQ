#include "pieceimageprovider.h"
#include "appsettings.h"
#include <QCoreApplication>
#include <QPointer>

PieceImageProvider& PieceImageProvider::instance()
{
    // SVGアイコンのキャッシュはGUI基盤より先に破棄する。
    static QPointer<PieceImageProvider> provider;
    if (!provider) {
        provider = new PieceImageProvider;
        provider->setParent(QCoreApplication::instance());
    }
    return *provider;
}

QString PieceImageProvider::style() const
{
    return AppSettings::pieceStyle();
}

void PieceImageProvider::setStyle(const QString& newStyle)
{
    if (!AppSettings::availablePieceStyles().contains(newStyle)) return;
    if (newStyle == style()) return;
    AppSettings::setPieceStyle(newStyle);
    emit styleChanged();
}

QIcon PieceImageProvider::icon(Piece piece, bool flipped) const
{
    return iconForStyle(piece, style(), flipped);
}

QIcon PieceImageProvider::iconForStyle(Piece piece, const QString& currentStyle, bool flipped) const
{
    if (piece == Piece::None) return {};
    // 画像のファイル名（PieceKind の順。玉は先手が「王」、後手が「玉」）
    static constexpr const char* kNames[kPieceKindCount] = {
        "fu", "kyou", "kei", "gin", "kin", "kaku", "hi", "ou",
        "to", "narikyou", "narikei", "narigin", "uma", "ryuu"};
    const PieceKind kind = pieceKind(piece);
    const char* name = (kind == PieceKind::King && isWhitePiece(piece)) ? "gyoku" : kNames[static_cast<int>(kind)];

    const QString selected = AppSettings::availablePieceStyles().contains(currentStyle)
        ? currentStyle : QStringLiteral("standard");
    const QString prefix = selected == QStringLiteral("standard")
        ? QStringLiteral(":/pieces/") : QStringLiteral(":/pieces/%1/").arg(selected);
    const QString side = isBlackPiece(piece) != flipped
        ? QStringLiteral("Sente_") : QStringLiteral("Gote_");
    const QString path = prefix + side + QLatin1String(name) + QStringLiteral("45.svg");
    auto it = m_icons.constFind(path);
    if (it != m_icons.constEnd()) return it.value();
    const QIcon result(path);
    m_icons.insert(path, result);
    return result;
}
