/// @file kifufilefilter.cpp
/// @brief 棋譜ファイルを開くダイアログの絞り込みの実装

#include "kifufilefilter.h"

#include "kifuformat.h"

#include <QCoreApplication>
#include <QFileInfo>
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

/// 保存できる拡張子1つの絞り込み（"KIF形式 UTF-8 (*.kifu)"）
QString saveFilter(const KifuFormats::SaveTarget& target)
{
    return QStringLiteral("%1 (*.%2)").arg(tr(target.label), target.suffix);
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

QString KifuFileFilter::forSaving()
{
    QStringList filters;
    for (const KifuFormats::SaveTarget& target : KifuFormats::saveTargets()) filters << saveFilter(target);
    filters << tr("すべてのファイル (*)");
    return filters.join(QStringLiteral(";;"));
}

QString KifuFileFilter::pathForSaving(const QString& path, const QString& selectedFilter)
{
    if (path.isEmpty() || !QFileInfo(path).suffix().isEmpty()) return path;
    // 絞り込みは forSaving の「見出し (*.拡張子)」（見出しは訳した文なので、末尾の拡張子で見分ける）
    QString suffix = QStringLiteral("kifu");
    for (const KifuFormats::SaveTarget& target : KifuFormats::saveTargets()) {
        if (selectedFilter.endsWith(QStringLiteral("(*.%1)").arg(target.suffix))) suffix = target.suffix;
    }
    return path + QLatin1Char('.') + suffix;
}
