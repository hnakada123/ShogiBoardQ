#ifndef FONTSIZEHELPER_H
#define FONTSIZEHELPER_H

#include <functional>

#include "dialogutils.h"
#include "setting.h"

/// ダイアログのフォントサイズ増減を管理するヘルパー。
/// 状態管理（clamp + save）と、ダイアログ全体への適用（applyTo）を担当する。
/// ダイアログ側の applyFontSize() は applyTo() を呼び、固有の調整だけを書く。
class FontSizeHelper {
public:
    struct Config {
        int initialSize;
        int minSize = 8;
        int maxSize = 24;
        int step = 1;
        std::function<void(int)> saveFn;
    };

    explicit FontSizeHelper(const Config& config);

    /// 設定（宣言の表の1行）から初期値を読み、変更を同じ設定に保存する
    explicit FontSizeHelper(const Setting<int>& setting, int minSize = 8, int maxSize = 24, int step = 1)
        : FontSizeHelper(Config{setting(), minSize, maxSize, step, [&setting](int size) { setting.set(size); }})
    {
    }

    /// フォントサイズを step 分増加する。変更があった場合 true を返す。
    bool increase();

    /// フォントサイズを step 分減少する。変更があった場合 true を返す。
    bool decrease();

    /// 現在のフォントサイズを返す。
    int fontSize() const;

    /// ダイアログ全体に現在の文字サイズを適用し、適用したフォントを返す（DialogUtils::applyFontSize）。
    /// 盤面など自前で文字の大きさを決めるウィジェットは excluded に渡す。
    QFont applyTo(QWidget* dialog, const QList<QWidget*>& excluded = {}) const
    {
        return DialogUtils::applyFontSize(dialog, m_fontSize, m_minSize, m_maxSize, excluded);
    }

private:
    int m_fontSize;
    int m_minSize;
    int m_maxSize;
    int m_step;
    std::function<void(int)> m_saveFn;
};

#endif // FONTSIZEHELPER_H
