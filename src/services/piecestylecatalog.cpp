/// @file piecestylecatalog.cpp
/// @brief 駒のセットの表の実装

#include "piecestylecatalog.h"

using Family = PieceStyleCatalog::Family;

namespace {
const PieceStyleCatalog::Entry* find(const QString& id)
{
    for (const auto& entry : PieceStyleCatalog::entries())
        if (id == QLatin1String(entry.id)) return &entry;
    return nullptr;
}
} // namespace

const QList<PieceStyleCatalog::Entry>& PieceStyleCatalog::entries()
{
    static const QList<Entry> table = {
    {"standard", Family::Standard, QT_TRANSLATE_NOOP("PieceStyleCatalog", "標準の駒")},
    {"torafu_light", Family::Torafu, QT_TRANSLATE_NOOP("PieceStyleCatalog", "淡虎斑")},
    {"torafu_silk", Family::Torafu, QT_TRANSLATE_NOOP("PieceStyleCatalog", "絹虎斑")},
    {"torafu_amber", Family::Torafu, QT_TRANSLATE_NOOP("PieceStyleCatalog", "飴虎斑")},
    {"torafu_red", Family::Torafu, QT_TRANSLATE_NOOP("PieceStyleCatalog", "紅虎斑")},
    {"torafu_gold", Family::Torafu, QT_TRANSLATE_NOOP("PieceStyleCatalog", "山吹虎斑")},
    {"wood_pale", Family::Wood, QT_TRANSLATE_NOOP("PieceStyleCatalog", "白木")},
    {"wood_classic", Family::Wood, QT_TRANSLATE_NOOP("PieceStyleCatalog", "木目")},
    {"wood_amber", Family::Wood, QT_TRANSLATE_NOOP("PieceStyleCatalog", "飴柾")},
    {"wood_bamboo", Family::Wood, QT_TRANSLATE_NOOP("PieceStyleCatalog", "笹杢")},
    {"wood_walnut", Family::Wood, QT_TRANSLATE_NOOP("PieceStyleCatalog", "胡桃")},
    {"tint_linen", Family::Tint, QT_TRANSLATE_NOOP("PieceStyleCatalog", "生成り")},
    {"tint_sakura", Family::Tint, QT_TRANSLATE_NOOP("PieceStyleCatalog", "薄桜")},
    {"tint_celadon", Family::Tint, QT_TRANSLATE_NOOP("PieceStyleCatalog", "青磁")},
    {"tint_moon", Family::Tint, QT_TRANSLATE_NOOP("PieceStyleCatalog", "月白")},
    {"tint_wisteria", Family::Tint, QT_TRANSLATE_NOOP("PieceStyleCatalog", "藤鼠")},
    {"deep_ebony", Family::Deep, QT_TRANSLATE_NOOP("PieceStyleCatalog", "黒檀")},
    {"deep_navy", Family::Deep, QT_TRANSLATE_NOOP("PieceStyleCatalog", "鉄紺")},
    {"deep_green", Family::Deep, QT_TRANSLATE_NOOP("PieceStyleCatalog", "深緑")},
    {"deep_grape", Family::Deep, QT_TRANSLATE_NOOP("PieceStyleCatalog", "葡萄")},
    {"deep_gold", Family::Deep, QT_TRANSLATE_NOOP("PieceStyleCatalog", "墨金")},
    {"sengoku", Family::Design, QT_TRANSLATE_NOOP("PieceStyleCatalog", "戦国文字")},
    {"chess_facet_wood", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Facet（木肌）")},
    {"chess_facet_paper", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Facet（白）")},
    {"chess_facet_slate", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Facet（墨）")},
    {"chess_atelier_wood", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Atelier（木肌）")},
    {"chess_atelier_paper", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Atelier（白）")},
    {"chess_atelier_slate", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Atelier（墨）")},
    {"chess_ribbon_wood", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Ribbon（木肌）")},
    {"chess_ribbon_paper", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Ribbon（白）")},
    {"chess_ribbon_slate", Family::Chess, QT_TRANSLATE_NOOP("PieceStyleCatalog", "Ribbon（墨）")},
    {"alphabet_sei_wood", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "端正（木肌）")},
    {"alphabet_sei_paper", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "端正（白）")},
    {"alphabet_sei_slate", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "端正（墨）")},
    {"alphabet_rin_wood", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "凛（木肌）")},
    {"alphabet_rin_paper", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "凛（白）")},
    {"alphabet_rin_slate", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "凛（墨）")},
    {"alphabet_sumi_wood", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "墨（木肌）")},
    {"alphabet_sumi_paper", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "墨（白）")},
    {"alphabet_sumi_slate", Family::Alphabet, QT_TRANSLATE_NOOP("PieceStyleCatalog", "墨（墨）")},
    };
    return table;
}

QStringList PieceStyleCatalog::ids()
{
    QStringList result;
    for (const auto& entry : entries()) result.append(QString::fromLatin1(entry.id));
    return result;
}

bool PieceStyleCatalog::contains(const QString& id)
{
    return find(id) != nullptr;
}

QString PieceStyleCatalog::displayName(const QString& id)
{
    const Entry* entry = find(id);
    return tr(entry ? entry->name : entries().constFirst().name);
}

Family PieceStyleCatalog::family(const QString& id)
{
    const Entry* entry = find(id);
    return entry ? entry->family : Family::Standard;
}

QString PieceStyleCatalog::familyName(Family family)
{
    switch (family) {
    case Family::Torafu: return tr("虎斑");
    case Family::Wood: return tr("木肌");
    case Family::Tint: return tr("淡色");
    case Family::Deep: return tr("深色");
    case Family::Design: return tr("意匠");
    case Family::Chess: return tr("チェス");
    case Family::Alphabet: return tr("アルファベット");
    case Family::Standard:
    case Family::Count:
        break;
    }
    return {};
}

QString PieceStyleCatalog::resourcePrefix(const QString& id)
{
    const Entry* entry = find(id);
    if (!entry || entry->family == Family::Standard) return QStringLiteral(":/pieces/");
    return QStringLiteral(":/pieces/%1/").arg(QLatin1String(entry->id));
}
