#include "boardappearancecatalog.h"

namespace {
BoardAppearanceSample board(const QString& name, QRgb surface, QRgb grid, bool grain = true)
{
    BoardColors colors;
    colors.board = QColor(surface);
    colors.grid = QColor(grid);
    return {name, colors, grain};
}

BoardAppearanceSample background(const QString& name, QRgb color)
{
    BoardColors colors;
    colors.background = QColor(color);
    return {name, colors, false};
}

BoardAppearanceSample stand(const QString& name, QRgb surface, bool grain = true)
{
    BoardColors colors;
    colors.stand = QColor(surface);
    return {name, colors, grain};
}

BoardAppearanceSample card(const QString& name, QRgb background, QRgb text, QRgb border,
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
    return {name, colors, false};
}
}

QList<BoardAppearanceSample> BoardAppearanceCatalog::samples(Component component)
{
    if (component == Component::Board) {
        return {
            {tr("榧"), BoardColors{}, true},
            board(tr("白榧"), 0xe9d4a5, 0x65533b),
            board(tr("本榧"), 0xd5ac69, 0x594025),
            board(tr("琥珀"), 0xe3b161, 0x634321),
            board(tr("胡桃"), 0xb89a7a, 0x513e31),
            board(tr("焦茶"), 0x796452, 0xe1cdb0),
            board(tr("白桐"), 0xe8dfc9, 0x766b54),
            board(tr("生成り"), 0xddd4c0, 0x716956),
            board(tr("月白"), 0xdce2e2, 0x5d6a71, false),
            board(tr("銀鼠"), 0xc1c6c7, 0x546169, false),
            board(tr("薄墨"), 0x646c70, 0xcbd2d3, false),
            board(tr("墨夜"), 0x354047, 0x91a1ac, false),
            board(tr("青磁"), 0xbcd1c3, 0x4d6b5e, false),
            board(tr("若竹"), 0xcbd1a7, 0x596442),
            board(tr("苔庭"), 0xabb28b, 0x3e4c36),
            board(tr("藍青"), 0x91afbf, 0x39586b, false),
            board(tr("薄桜"), 0xe2c9c4, 0x7c5653, false),
            board(tr("藤鼠"), 0xc8c2d4, 0x635773, false),
            board(tr("砂岩"), 0xd9c6a9, 0x796348, false),
            board(tr("青灰"), 0xb6c7cf, 0x506877, false),
            board(tr("チェス・木肌"), 0xe4ca98, 0x9e865c, false),
            board(tr("チェス・白"), 0xe9ede7, 0xa2afa4, false),
            board(tr("チェス・墨"), 0x3c5055, 0x809294, false)};
    }
    if (component == Component::Background) {
        return {
            {tr("畳"), BoardColors{}, false},
            background(tr("白緑"), 0x8f978c), background(tr("松葉"), 0x6c7965),
            background(tr("焦茶"), 0x39352f), background(tr("胡桃"), 0x514c46),
            background(tr("炭"), 0x383c3a), background(tr("柳鼠"), 0x929a8c),
            background(tr("生成り"), 0xaaa59b), background(tr("青鼠"), 0x616e76),
            background(tr("銀鼠"), 0x545e65), background(tr("薄墨"), 0x353d43),
            background(tr("墨夜"), 0x202930), background(tr("青磁"), 0x657f74),
            background(tr("若竹"), 0x747e66), background(tr("苔庭"), 0x52604a),
            background(tr("藍青"), 0x354957), background(tr("薄桜"), 0x8d7a79),
            background(tr("藤鼠"), 0x767080), background(tr("砂岩"), 0x857c6d),
            background(tr("青灰"), 0x667983),
            background(tr("チェス・木肌"), 0xe9dcc1),
            background(tr("チェス・白"), 0xf0f2ed),
            background(tr("チェス・墨"), 0x44555b)};
    }
    if (component == Component::Stand) {
        return {
            {tr("木肌"), BoardColors{}, true},
            stand(tr("白榧"), 0xd2bc8e), stand(tr("本榧"), 0xb39460),
            stand(tr("飴色"), 0xa97640), stand(tr("胡桃"), 0x856b54),
            stand(tr("黒檀"), 0x423b32), stand(tr("白桐"), 0xd9d1ba),
            stand(tr("生成り"), 0xc7bca7), stand(tr("月白"), 0xc8d0d2, false),
            stand(tr("銀鼠"), 0x8b969c, false), stand(tr("薄墨"), 0x596167, false),
            stand(tr("墨夜"), 0x303a41, false), stand(tr("青磁"), 0x91ad9e, false),
            stand(tr("若竹"), 0x9da57d), stand(tr("苔庭"), 0x788560),
            stand(tr("藍青"), 0x4d6e84, false), stand(tr("薄桜"), 0xbea09b, false),
            stand(tr("藤鼠"), 0x9c90af, false), stand(tr("砂岩"), 0xb2a083, false),
            stand(tr("青灰"), 0x8096a2, false),
            stand(tr("チェス・木肌"), 0xe4ca98, false),
            stand(tr("チェス・白"), 0xe9ede7, false),
            stand(tr("チェス・墨"), 0x3c5055, false)};
    }
    return {
        {tr("若草"), BoardColors{}, false},
        card(tr("生成り"), 0xf0eade, 0x443f35, 0xbcb2a0, 0x746347),
        card(tr("白磁"), 0xf2f4f3, 0x344348, 0xb0bfc1, 0x496c76),
        card(tr("桜霞"), 0xf2e5e5, 0x553f45, 0xcfb0b7, 0x875260),
        card(tr("藤霞"), 0xeee8f4, 0x4b4059, 0xbdb0ce, 0x715a87),
        card(tr("青磁"), 0xe4eee8, 0x344f46, 0xa4bbaf, 0x486f5f),
        card(tr("水浅葱"), 0xe2eff0, 0x325158, 0x9ebfc2, 0x3f7079),
        card(tr("砂丘"), 0xeee4d4, 0x554531, 0xc2ae8e, 0x80613d),
        card(tr("琥珀"), 0xf1e1c5, 0x593f28, 0xc9a775, 0x865a30),
        card(tr("胡桃"), 0xe4d8ca, 0x4d3c30, 0xbca48e, 0x75563e),
        card(tr("墨"), 0x30383b, 0xeff1e9, 0x637174, 0xaabbb1, true),
        card(tr("藍夜"), 0x263b50, 0xe5eef4, 0x556e85, 0x9ac2d9, true),
        card(tr("深緑"), 0x2d443b, 0xe4eee4, 0x5c7864, 0xa9c5a0, true),
        card(tr("葡萄"), 0x49394f, 0xf3e7f1, 0x866c8a, 0xd4b7cf, true),
        card(tr("鉄紺"), 0x303c4b, 0xe9eef5, 0x64768b, 0xafbed5, true),
        card(tr("墨金"), 0x36352e, 0xf2e9cf, 0x847a59, 0xd5c18a, true),
        card(tr("銀鼠"), 0xe0e5e7, 0x3c4851, 0xa0aeb7, 0x566c7d),
        card(tr("紅殻"), 0x4b3531, 0xf5e8da, 0x947569, 0xdcb697, true),
        card(tr("石庭"), 0xdde4db, 0x3f5043, 0xa1b19f, 0x536f55),
        card(tr("月白"), 0xe8edf2, 0x384c61, 0xb1beca, 0x4d6783)};
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
        const auto members = BoardColors::members();
        for (size_t i = 4; i < members.size(); ++i) colors.*members[i] = sample.colors.*members[i];
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
