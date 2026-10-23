#include "boardappearancecatalog.h"

namespace {
BoardAppearanceSample board(const char* key, QRgb surface, QRgb grid, bool grain = true)
{
    BoardColors colors;
    colors.board = QColor(surface);
    colors.grid = QColor(grid);
    return {QString::fromUtf8(key), BoardAppearanceCatalog::tr(key), colors, grain};
}

BoardAppearanceSample background(const char* key, QRgb color)
{
    BoardColors colors;
    colors.background = QColor(color);
    return {QString::fromUtf8(key), BoardAppearanceCatalog::tr(key), colors, false};
}

BoardAppearanceSample stand(const char* key, QRgb surface, bool grain = true)
{
    BoardColors colors;
    colors.stand = QColor(surface);
    return {QString::fromUtf8(key), BoardAppearanceCatalog::tr(key), colors, grain};
}

BoardAppearanceSample card(const char* key, QRgb background, QRgb text, QRgb border,
                          QRgb accent, bool dark = false)
{
    BoardColors colors;
    colors.cardBackground = QColor(background);
    colors.nameText = colors.clockText = QColor(text);
    colors.cardBorder = QColor(border);
    colors.activeCardBorder = colors.turnBackground = colors.turnBorder = QColor(accent);
    colors.turnText = dark ? QColor(0x171c20) : QColor(Qt::white);
    colors.clockWarningText = dark ? QColor(0xffd586) : QColor(0x825015);
    colors.clockCriticalText = dark ? QColor(0xffb0a6) : QColor(0xa52e34);
    return {QString::fromUtf8(key), BoardAppearanceCatalog::tr(key), colors, false};
}
}

QList<BoardAppearanceSample> BoardAppearanceCatalog::samples(Component component)
{
    if (component == Component::Board) {
        return {
            {QStringLiteral("榧"), tr("榧"), BoardColors{}, true},
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白榧"), 0xe9d4a5, 0x65533b),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "本榧"), 0xd5ac69, 0x594025),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "琥珀"), 0xe3b161, 0x634321),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "胡桃"), 0xb89a7a, 0x513e31),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "焦茶"), 0x796452, 0xe1cdb0),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白桐"), 0xe8dfc9, 0x766b54),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "生成り"), 0xddd4c0, 0x716956),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "月白"), 0xdce2e2, 0x5d6a71, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "銀鼠"), 0xc1c6c7, 0x546169, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "薄墨"), 0x646c70, 0xcbd2d3, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨夜"), 0x354047, 0x91a1ac, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青磁"), 0xbcd1c3, 0x4d6b5e, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "若竹"), 0xcbd1a7, 0x596442),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "苔庭"), 0xabb28b, 0x3e4c36),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藍青"), 0x91afbf, 0x39586b, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "薄桜"), 0xe2c9c4, 0x7c5653, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藤鼠"), 0xc8c2d4, 0x635773, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "砂岩"), 0xd9c6a9, 0x796348, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青灰"), 0xb6c7cf, 0x506877, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・木肌"), 0xe4ca98, 0x9e865c, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・白"), 0xe9ede7, 0xa2afa4, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・墨"), 0x3c5055, 0x809294, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・木肌"), 0xdfc69a, 0xa28a60, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・白"), 0xdfe5dc, 0xa5afa0, false),
            board(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・墨"), 0x384841, 0x809186, false)};
    }
    if (component == Component::Background) {
        return {
            {QStringLiteral("畳"), tr("畳"), BoardColors{}, false},
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白緑"), 0x8f978c), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "松葉"), 0x6c7965),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "焦茶"), 0x39352f), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "胡桃"), 0x514c46),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "炭"), 0x383c3a), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "柳鼠"), 0x929a8c),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "生成り"), 0xaaa59b), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青鼠"), 0x616e76),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "銀鼠"), 0x545e65), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "薄墨"), 0x353d43),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨夜"), 0x202930), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青磁"), 0x657f74),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "若竹"), 0x747e66), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "苔庭"), 0x52604a),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藍青"), 0x354957), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "薄桜"), 0x8d7a79),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藤鼠"), 0x767080), background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "砂岩"), 0x857c6d),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青灰"), 0x667983),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・木肌"), 0xe9dcc1),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・白"), 0xf0f2ed),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・墨"), 0x44555b),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・木肌"), 0xe9dec4),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・白"), 0xe9ede7),
            background(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・墨"), 0x43504b)};
    }
    if (component == Component::Stand) {
        return {
            {QStringLiteral("木肌"), tr("木肌"), BoardColors{}, true},
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白榧"), 0xd2bc8e), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "本榧"), 0xb39460),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "飴色"), 0xa97640), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "胡桃"), 0x856b54),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "黒檀"), 0x423b32), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白桐"), 0xd9d1ba),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "生成り"), 0xc7bca7), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "月白"), 0xc8d0d2, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "銀鼠"), 0x8b969c, false), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "薄墨"), 0x596167, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨夜"), 0x303a41, false), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青磁"), 0x91ad9e, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "若竹"), 0x9da57d), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "苔庭"), 0x788560),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藍青"), 0x4d6e84, false), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "薄桜"), 0xbea09b, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藤鼠"), 0x9c90af, false), stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "砂岩"), 0xb2a083, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青灰"), 0x8096a2, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・木肌"), 0xe4ca98, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・白"), 0xe9ede7, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "チェス・墨"), 0x3c5055, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・木肌"), 0xdfc69a, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・白"), 0xdfe5dc, false),
            stand(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "欧文・墨"), 0x384841, false)};
    }
    return {
        {QStringLiteral("若草"), tr("若草"), BoardColors{}, false},
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "生成り"), 0xf0eade, 0x443f35, 0xbcb2a0, 0x746347),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白磁"), 0xf2f4f3, 0x344348, 0xb0bfc1, 0x496c76),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "桜霞"), 0xf2e5e5, 0x553f45, 0xcfb0b7, 0x875260),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藤霞"), 0xeee8f4, 0x4b4059, 0xbdb0ce, 0x715a87),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青磁"), 0xe4eee8, 0x344f46, 0xa4bbaf, 0x486f5f),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "水浅葱"), 0xe2eff0, 0x325158, 0x9ebfc2, 0x3f7079),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "砂丘"), 0xeee4d4, 0x554531, 0xc2ae8e, 0x80613d),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "琥珀"), 0xf1e1c5, 0x593f28, 0xc9a775, 0x865a30),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "胡桃"), 0xe4d8ca, 0x4d3c30, 0xbca48e, 0x75563e),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨"), 0x30383b, 0xeff1e9, 0x637174, 0xaabbb1, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "藍夜"), 0x263b50, 0xe5eef4, 0x556e85, 0x9ac2d9, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "深緑"), 0x2d443b, 0xe4eee4, 0x5c7864, 0xa9c5a0, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "葡萄"), 0x49394f, 0xf3e7f1, 0x866c8a, 0xd4b7cf, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "鉄紺"), 0x303c4b, 0xe9eef5, 0x64768b, 0xafbed5, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨金"), 0x36352e, 0xf2e9cf, 0x847a59, 0xd5c18a, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "銀鼠"), 0xe0e5e7, 0x3c4851, 0xa0aeb7, 0x566c7d),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "紅殻"), 0x4b3531, 0xf5e8da, 0x947569, 0xdcb697, true),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "石庭"), 0xdde4db, 0x3f5043, 0xa1b19f, 0x536f55),
        card(QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "月白"), 0xe8edf2, 0x384c61, 0xb1beca, 0x4d6783)};
}

int BoardAppearanceCatalog::indexOf(Component component, const QString& key)
{
    const QList<BoardAppearanceSample> list = samples(component);
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).key == key) return i;
    }
    return -1;
}

void BoardAppearanceCatalog::apply(Component component, const BoardAppearanceSample& sample,
                                   BoardColors& colors, BoardVisuals& visuals)
{
    if (component == Component::Board) {
        colors.board = sample.colors.board;
        colors.grid = sample.colors.grid;
        visuals.woodGrain = sample.woodGrain;
    } else if (component == Component::Stand) {
        colors.stand = sample.colors.stand;
        visuals.standWoodGrain = sample.woodGrain;
    } else if (component == Component::Background) {
        colors.background = sample.colors.background;
    } else {
        // 対局者のカードの色（盤・駒台・背景より後ろの分類）
        for (const BoardColors::Field& field : BoardColors::fields())
            if (field.group != BoardColors::Group::Board) colors.*field.member = sample.colors.*field.member;
    }
}

bool BoardAppearanceCatalog::matches(Component component, const BoardAppearanceSample& sample,
                                     const BoardColors& colors, const BoardVisuals& visuals)
{
    auto candidateColors = colors;
    auto candidateVisuals = visuals;
    apply(component, sample, candidateColors, candidateVisuals);
    return candidateColors == colors && candidateVisuals == visuals;
}

int BoardAppearanceCatalog::selectedIndex(Component component, const AppearanceState& state)
{
    const QList<BoardAppearanceSample> list = samples(component);
    int selected = -1;
    for (int i = 0; i < list.size(); ++i)
        if (matches(component, list.at(i), state.colors, state.visuals)) selected = i;
    return selected;
}

const QList<BoardAppearanceCatalog::Combination>& BoardAppearanceCatalog::combinations()
{
    // 並びは Component（将棋盤・駒台・対局者情報・背景）
    static const QList<Combination> table = {
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "標準・榧と畳"), "standard", {"榧", "木肌", "若草", "畳"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "淡虎斑・白榧"), "torafu_light", {"白榧", "白榧", "生成り", "白緑"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "黒檀・月白"), "deep_ebony", {"月白", "月白", "銀鼠", "青鼠"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "白木・墨夜"), "wood_pale", {"墨夜", "墨夜", "墨", "墨夜"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "青磁・白磁"), "tint_celadon", {"白桐", "白桐", "白磁", "柳鼠"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "葡萄・藤鼠"), "deep_grape", {"藤鼠", "藤鼠", "藤霞", "藤鼠"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Facet（木肌）"), "chess_facet_wood", {"チェス・木肌", "チェス・木肌", "生成り", "チェス・木肌"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Facet（白）"), "chess_facet_paper", {"チェス・白", "チェス・白", "白磁", "チェス・白"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Facet（墨）"), "chess_facet_slate", {"チェス・墨", "チェス・墨", "墨", "チェス・墨"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Atelier（木肌）"), "chess_atelier_wood", {"チェス・木肌", "チェス・木肌", "生成り", "チェス・木肌"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Atelier（白）"), "chess_atelier_paper", {"チェス・白", "チェス・白", "白磁", "チェス・白"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Atelier（墨）"), "chess_atelier_slate", {"チェス・墨", "チェス・墨", "墨", "チェス・墨"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Ribbon（木肌）"), "chess_ribbon_wood", {"チェス・木肌", "チェス・木肌", "生成り", "チェス・木肌"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Ribbon（白）"), "chess_ribbon_paper", {"チェス・白", "チェス・白", "白磁", "チェス・白"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "Ribbon（墨）"), "chess_ribbon_slate", {"チェス・墨", "チェス・墨", "墨", "チェス・墨"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "端正（木肌）"), "alphabet_sei_wood", {"欧文・木肌", "欧文・木肌", "生成り", "欧文・木肌"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "端正（白）"), "alphabet_sei_paper", {"欧文・白", "欧文・白", "白磁", "欧文・白"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "端正（墨）"), "alphabet_sei_slate", {"欧文・墨", "欧文・墨", "墨", "欧文・墨"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "凛（木肌）"), "alphabet_rin_wood", {"欧文・木肌", "欧文・木肌", "生成り", "欧文・木肌"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "凛（白）"), "alphabet_rin_paper", {"欧文・白", "欧文・白", "白磁", "欧文・白"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "凛（墨）"), "alphabet_rin_slate", {"欧文・墨", "欧文・墨", "墨", "欧文・墨"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨（木肌）"), "alphabet_sumi_wood", {"欧文・木肌", "欧文・木肌", "生成り", "欧文・木肌"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨（白）"), "alphabet_sumi_paper", {"欧文・白", "欧文・白", "白磁", "欧文・白"}},
        {QT_TRANSLATE_NOOP("BoardAppearanceCatalog", "墨（墨）"), "alphabet_sumi_slate", {"欧文・墨", "欧文・墨", "墨", "欧文・墨"}},
    };
    return table;
}

QString BoardAppearanceCatalog::combinationName(const Combination& combination)
{
    return tr(combination.name);
}

AppearanceState BoardAppearanceCatalog::applied(const Combination& combination, AppearanceState state)
{
    for (int i = 0; i < kComponentCount; ++i) {
        const auto component = static_cast<Component>(i);
        const int row = indexOf(component, QString::fromUtf8(combination.samples.at(static_cast<std::size_t>(i))));
        if (row >= 0) apply(component, samples(component).at(row), state.colors, state.visuals);
    }
    state.pieceStyle = QString::fromLatin1(combination.pieceStyle);
    return state;
}

int BoardAppearanceCatalog::combinationIndex(const AppearanceState& state)
{
    int selected = -1;
    const QList<Combination>& table = combinations();
    for (int i = 0; i < table.size(); ++i) {
        const Combination& combination = table.at(i);
        bool same = state.pieceStyle == QLatin1String(combination.pieceStyle);
        for (int c = 0; same && c < kComponentCount; ++c) {
            const auto component = static_cast<Component>(c);
            same = selectedIndex(component, state)
                == indexOf(component, QString::fromUtf8(combination.samples.at(static_cast<std::size_t>(c))));
        }
        if (same) selected = i;
    }
    return selected;
}
