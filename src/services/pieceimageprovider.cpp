#include "pieceimageprovider.h"
#include "appsettings.h"

PieceImageProvider& PieceImageProvider::instance()
{
    static PieceImageProvider provider;
    return provider;
}

QString PieceImageProvider::style() const
{
    return AppSettings::pieceStyle();
}

void PieceImageProvider::setStyle(const QString& newStyle)
{
    if (newStyle != QStringLiteral("standard") && newStyle != QStringLiteral("clear")) return;
    if (newStyle == style()) return;
    AppSettings::setPieceStyle(newStyle);
    emit styleChanged();
}

QIcon PieceImageProvider::icon(QChar piece, bool flipped) const
{
    const char* name = nullptr;
    switch (piece.toUpper().toLatin1()) {
    case 'P': name = "fu"; break;
    case 'L': name = "kyou"; break;
    case 'N': name = "kei"; break;
    case 'S': name = "gin"; break;
    case 'G': name = "kin"; break;
    case 'B': name = "kaku"; break;
    case 'R': name = "hi"; break;
    case 'K': name = piece.isUpper() ? "ou" : "gyoku"; break;
    case 'Q': name = "to"; break;
    case 'M': name = "narikyou"; break;
    case 'O': name = "narikei"; break;
    case 'T': name = "narigin"; break;
    case 'C': name = "uma"; break;
    case 'U': name = "ryuu"; break;
    default: return {};
    }

    const QString prefix = style() == QStringLiteral("clear")
        ? QStringLiteral(":/pieces/clear/") : QStringLiteral(":/pieces/");
    const QString side = piece.isUpper() != flipped
        ? QStringLiteral("Sente_") : QStringLiteral("Gote_");
    return QIcon(prefix + side + QLatin1String(name) + QStringLiteral("45.svg"));
}
