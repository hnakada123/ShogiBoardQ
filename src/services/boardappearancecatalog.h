#ifndef BOARDAPPEARANCECATALOG_H
#define BOARDAPPEARANCECATALOG_H

#include "boardcolors.h"
#include "boardvisuals.h"
#include <QCoreApplication>
#include <QList>

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
    static QList<BoardAppearanceSample> samples(Component component);
    /// key の見本の並びの位置（無ければ -1）
    static int indexOf(Component component, const QString& key);
    static void apply(Component component, const BoardAppearanceSample& sample,
                      BoardColors& colors, BoardVisuals& visuals);
    static bool matches(Component component, const BoardAppearanceSample& sample,
                        const BoardColors& colors, const BoardVisuals& visuals);
};

#endif
