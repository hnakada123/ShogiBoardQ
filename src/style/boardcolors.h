#ifndef BOARDCOLORS_H
#define BOARDCOLORS_H

#include <QColor>
#include <QtGlobal>
#include <array>
#include <cstddef>

/// 盤面の配色。初期値は「榧と畳」。
struct BoardColors {
    QColor background{137, 146, 129};
    QColor board{224, 187, 121};
    QColor stand{168, 126, 82};
    QColor grid{84, 65, 39};
    QColor cardBackground{236, 237, 223};
    QColor cardBorder{166, 177, 153};
    QColor activeCardBorder{75, 101, 79};
    QColor turnBackground{75, 101, 79};
    QColor turnBorder{75, 101, 79};
    QColor turnText{255, 255, 255};
    QColor nameBackground{Qt::transparent};
    QColor nameBorder{Qt::transparent};
    QColor nameText{53, 64, 51};
    QColor clockBackground{Qt::transparent};
    QColor clockBorder{Qt::transparent};
    QColor clockText{53, 64, 51};
    QColor clockWarningText{135, 93, 33};
    QColor clockCriticalText{178, 59, 50};

    using Member = QColor BoardColors::*;

    /// 色の分類（配色ダイアログのタブ。盤・駒台より後ろは対局者のカード）
    enum class Group { Board, Card, Turn, Name, Clock };

    /// 1色の情報（設定のキー・配色ダイアログの表示名とボタン・透明にできるか・分類）
    struct Field {
        Member member;
        const char* key;         ///< 設定のキー
        const char* label;       ///< 配色ダイアログの表示名（翻訳の文脈は "BoardAppearanceDialog"）
        const char* objectName;  ///< 配色ダイアログのボタンの objectName
        Group group;
        bool transparent;        ///< アルファ値を使う（0 で透明）
    };

    /// 全部の色（配色ダイアログに並べる順）。色を足すときはメンバとこの表に1行ずつ足す
    static const std::array<Field, 18>& fields();
    static std::array<Member, 18> members();
    static bool supportsTransparency(Member member);

    BoardColors normalized() const
    {
        const BoardColors defaults;
        BoardColors result = *this;
        for (const auto member : members()) {
            QColor& color = result.*member;
            if (!color.isValid()) color = defaults.*member;
            color = QColor(color.red(), color.green(), color.blue(),
                           supportsTransparency(member) ? color.alpha() : 255);
        }
        return result;
    }

    bool sameBoardPalette(const BoardColors& other) const
    {
        return background == other.background && board == other.board
            && stand == other.stand && grid == other.grid;
    }
    bool operator==(const BoardColors& other) const
    {
        for (const auto member : members()) {
            if (this->*member != other.*member) return false;
        }
        return true;
    }
    bool operator!=(const BoardColors& other) const { return !(*this == other); }
};

inline const std::array<BoardColors::Field, 18>& BoardColors::fields()
{
    using G = Group;
    static const std::array<Field, 18> table{{
        {&BoardColors::background, "BoardColors/background", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "将棋盤の背景"),
         "backgroundColorButton", G::Board, false},
        {&BoardColors::board, "BoardColors/board", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "将棋盤"),
         "boardColorButton", G::Board, false},
        {&BoardColors::stand, "BoardColors/stand", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "駒台"),
         "standColorButton", G::Board, false},
        {&BoardColors::grid, "BoardColors/grid", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "マス罫線"),
         "gridColorButton", G::Board, false},
        {&BoardColors::cardBackground, "BoardColors/cardBackground", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "カードの背景"),
         "cardBackgroundColorButton", G::Card, false},
        {&BoardColors::cardBorder, "BoardColors/cardBorder", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "通常の枠線"),
         "cardBorderColorButton", G::Card, false},
        {&BoardColors::activeCardBorder, "BoardColors/activeCardBorder", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "手番側の枠線"),
         "activeCardBorderColorButton", G::Card, false},
        {&BoardColors::turnBackground, "BoardColors/turnBackground", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "手番の背景"),
         "turnBackgroundColorButton", G::Turn, true},
        {&BoardColors::turnBorder, "BoardColors/turnBorder", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "手番の枠線"),
         "turnBorderColorButton", G::Turn, true},
        {&BoardColors::turnText, "BoardColors/turnText", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "手番の文字"),
         "turnTextColorButton", G::Turn, false},
        {&BoardColors::nameBackground, "BoardColors/nameBackground", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "対局者名の背景"),
         "nameBackgroundColorButton", G::Name, true},
        {&BoardColors::nameBorder, "BoardColors/nameBorder", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "対局者名の枠線"),
         "nameBorderColorButton", G::Name, true},
        {&BoardColors::nameText, "BoardColors/nameText", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "対局者名の文字"),
         "nameTextColorButton", G::Name, false},
        {&BoardColors::clockBackground, "BoardColors/clockBackground", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "持ち時間の背景"),
         "clockBackgroundColorButton", G::Clock, true},
        {&BoardColors::clockBorder, "BoardColors/clockBorder", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "持ち時間の枠線"),
         "clockBorderColorButton", G::Clock, true},
        {&BoardColors::clockText, "BoardColors/clockText", QT_TRANSLATE_NOOP("BoardAppearanceDialog", "持ち時間の文字"),
         "clockTextColorButton", G::Clock, false},
        {&BoardColors::clockWarningText, "BoardColors/clockWarningText",
         QT_TRANSLATE_NOOP("BoardAppearanceDialog", "残り10秒以下の文字"), "clockWarningTextColorButton", G::Clock, false},
        {&BoardColors::clockCriticalText, "BoardColors/clockCriticalText",
         QT_TRANSLATE_NOOP("BoardAppearanceDialog", "秒読み・残り5秒以下の文字"), "clockCriticalTextColorButton", G::Clock, false},
    }};
    return table;
}

inline std::array<BoardColors::Member, 18> BoardColors::members()
{
    std::array<Member, 18> result{};
    for (size_t i = 0; i < result.size(); ++i) result[i] = fields()[i].member;
    return result;
}

inline bool BoardColors::supportsTransparency(Member member)
{
    for (const Field& field : fields())
        if (field.member == member) return field.transparent;
    return false;
}

#endif // BOARDCOLORS_H
