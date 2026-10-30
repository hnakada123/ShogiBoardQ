#ifndef PIECEIMAGEPROVIDER_H
#define PIECEIMAGEPROVIDER_H

/// @file pieceimageprovider.h
/// @brief 駒の画像（QIcon）の解決とキャッシュ

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

#include "shogitypes.h"

/**
 * @brief 駒のセットと駒から、駒の画像を引く（全盤面で共有するキャッシュ）
 *
 * 駒のセットは持たない。今の駒のセットとその変更の知らせは BoardAppearance。
 * SVG のアイコンを GUI の基盤より先に破棄するため、アプリケーションを親にした QObject にする。
 */
class PieceImageProvider : public QObject
{
public:
    static PieceImageProvider& instance();

    /// 今の駒のセット（BoardAppearance::pieceStyle）の駒の画像
    QIcon icon(Piece piece, bool flipped = false) const;
    /// 駒のセット style の駒の画像（flipped なら先後を入れ替えた向き）
    QIcon iconForStyle(Piece piece, const QString& style, bool flipped = false) const;

private:
    PieceImageProvider() = default;
    mutable QHash<QString, QIcon> m_icons;
};

#endif // PIECEIMAGEPROVIDER_H
