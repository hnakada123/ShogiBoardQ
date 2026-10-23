/// @file kifufilefilter.cpp
/// @brief 棋譜ファイルを開くダイアログの絞り込みの実装

#include "kifufilefilter.h"

#include "kifuformat.h"

#include <QCoreApplication>
#include <QStringList>

namespace {
QString tr(const char* text)
{
    return QCoreApplication::translate("KifuFileFilter", text);
}

/// 拡張子の並びを "*.kif *.kifu" にする
QString patterns(const QStringList& suffixes)
{
    QStringList result;
    for (const QString& suffix : suffixes) result.append(QStringLiteral("*.") + suffix);
    return result.join(QLatin1Char(' '));
}
} // namespace

QString KifuFileFilter::forOpening()
{
    struct Kind {
        const char* label;
        KifuFormat format;
    };
    static const Kind kinds[] = {{"KIF", KifuFormat::Kif}, {"KI2", KifuFormat::Ki2},   {"CSA", KifuFormat::Csa},
                                 {"JKF", KifuFormat::Jkf}, {"USI", KifuFormat::Usi},   {"SFEN", KifuFormat::Sfen},
                                 {"USEN", KifuFormat::Usen}};
    QStringList filters;
    filters << tr("棋譜ファイル (%1)").arg(patterns(KifuFormats::readableSuffixes()));
    for (const Kind& kind : kinds)
        filters << tr("%1形式 (%2)").arg(QLatin1String(kind.label), patterns(KifuFormats::readableSuffixes(kind.format)));
    filters << tr("すべてのファイル (*)");
    return filters.join(QStringLiteral(";;"));
}
