#ifndef FONTSIZEHELPER_H
#define FONTSIZEHELPER_H

#include <functional>

#include "setting.h"

/// ダイアログのフォントサイズ増減を管理するヘルパー。
/// 状態管理（clamp + save）のみ担当。applyFontSize() はダイアログ側で実装する。
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

private:
    int m_fontSize;
    int m_minSize;
    int m_maxSize;
    int m_step;
    std::function<void(int)> m_saveFn;
};

#endif // FONTSIZEHELPER_H
