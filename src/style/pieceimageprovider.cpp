/// @file pieceimageprovider.cpp
/// @brief 駒の画像の解決とキャッシュの実装

#include "pieceimageprovider.h"
#include "boardappearance.h"
#include "piecestylecatalog.h"
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

QIcon PieceImageProvider::icon(Piece piece, bool flipped) const
{
    return iconForStyle(piece, BoardAppearance::instance().pieceStyle(), flipped);
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

    const QString prefix = PieceStyleCatalog::resourcePrefix(currentStyle);
    const QString side = isBlackPiece(piece) != flipped
        ? QStringLiteral("Sente_") : QStringLiteral("Gote_");
    const QString path = prefix + side + QLatin1String(name) + QStringLiteral("45.svg");
    auto it = m_icons.constFind(path);
    if (it != m_icons.constEnd()) return it.value();
    const QIcon result(path);
    m_icons.insert(path, result);
    return result;
}
