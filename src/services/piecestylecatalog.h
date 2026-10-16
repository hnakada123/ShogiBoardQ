#ifndef PIECESTYLECATALOG_H
#define PIECESTYLECATALOG_H

/// @file piecestylecatalog.h
/// @brief 駒のセットの表（ID・分類・表示名・画像の場所）

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief 駒のセットの一覧
 *
 * 駒のセットを足すときは、この表に1行足し、resources の qrc に画像を置く。ID の一覧・表示名・
 * 分類（外観ダイアログの絞り込み）・画像のパスはすべてこの表から引く。
 */
class PieceStyleCatalog
{
    Q_DECLARE_TR_FUNCTIONS(PieceStyleCatalog)

public:
    /// 分類。外観ダイアログの絞り込みの並び（0 は「すべて」で、標準の駒はそこにだけ出る）
    enum class Family { Standard, Torafu, Wood, Tint, Deep, Design, Chess, Alphabet, Count };

    struct Entry {
        const char* id;    ///< 保存する名前（画像は :/pieces/<id>/ に置く。標準の駒だけ :/pieces/）
        Family family;
        const char* name;  ///< 表示名（翻訳前）
    };

    /// 表（並びは外観ダイアログの一覧の並び。先頭は標準の駒）
    static const QList<Entry>& entries();
    static QStringList ids();
    static bool contains(const QString& id);
    /// 標準の駒の ID（知らない ID の代わりにも使う）
    static QString standardId() { return QStringLiteral("standard"); }
    /// 表示名（知らない ID は標準の駒の名前）
    static QString displayName(const QString& id);
    static Family family(const QString& id);
    /// 分類の表示名（絞り込みの項目。Standard は無い）
    static QString familyName(Family family);
    /// 駒の画像のパスの前半（知らない ID は標準の駒）。後ろに "Sente_fu45.svg" などを付ける
    static QString resourcePrefix(const QString& id);
};

#endif // PIECESTYLECATALOG_H
