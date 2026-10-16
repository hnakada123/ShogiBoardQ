#ifndef BOARDAPPEARANCECATALOG_H
#define BOARDAPPEARANCECATALOG_H

#include "appearancestate.h"
#include "boardcolors.h"
#include "boardvisuals.h"
#include <QCoreApplication>
#include <QList>
#include <array>

struct BoardAppearanceSample {
    QString key;   ///< 変わらない名前（翻訳前の名前。おすすめの組み合わせが見本を指すのに使う）
    QString name;  ///< 表示する名前
    BoardColors colors;
    bool woodGrain = true;
};

/// 色の保存値から選択状態を復元し、他の部品を変えずに見本を適用する。
class BoardAppearanceCatalog
{
    Q_DECLARE_TR_FUNCTIONS(BoardAppearanceCatalog)
public:
    enum class Component { Board, Stand, Information, Background };
    static constexpr int kComponentCount = 4;

    /// おすすめの組み合わせ（駒のセットと、部品ごとの見本の変わらない名前。並びは Component）
    struct Combination {
        const char* name;   ///< 表示名（翻訳前）
        const char* pieceStyle;
        std::array<const char*, kComponentCount> samples;
    };

    static QList<BoardAppearanceSample> samples(Component component);
    /// key の見本の並びの位置（無ければ -1）
    static int indexOf(Component component, const QString& key);
    static void apply(Component component, const BoardAppearanceSample& sample,
                      BoardColors& colors, BoardVisuals& visuals);
    static bool matches(Component component, const BoardAppearanceSample& sample,
                        const BoardColors& colors, const BoardVisuals& visuals);
    /// state に合う見本の位置（いくつも合えば最後のもの。無ければ -1）
    static int selectedIndex(Component component, const AppearanceState& state);

    static const QList<Combination>& combinations();
    static QString combinationName(const Combination& combination);
    /// state に組み合わせを当てはめた外観
    static AppearanceState applied(const Combination& combination, AppearanceState state);
    /// state が当てはまる組み合わせの位置（無ければ -1）
    static int combinationIndex(const AppearanceState& state);
};

#endif
