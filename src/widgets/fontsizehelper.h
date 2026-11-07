#ifndef FONTSIZEHELPER_H
#define FONTSIZEHELPER_H

/// @file fontsizehelper.h
/// @brief 文字の大きさ（A-/A+）の状態と保存

#include <QObject>
#include <functional>

#include "setting.h"

/**
 * @brief 文字の大きさの増減（範囲に収め、変えたら保存して知らせる）
 *
 * A-/A+ のボタンは FontSizeControl で結び、fontSizeChanged を表示を作り直す処理につなぐ。
 * ダイアログは DialogFontScale がこの状態を持つ。
 */
class FontSizeHelper : public QObject
{
    Q_OBJECT

public:
    struct Config {
        int initialSize;
        int minSize = 8;
        int maxSize = 24;
        int step = 1;
        std::function<void(int)> saveFn;
    };

    explicit FontSizeHelper(const Config& config, QObject* parent = nullptr);
    /// 設定（宣言の表の1行）から初期値と範囲を読み、変更を同じ設定に保存する
    explicit FontSizeHelper(const BoundedSetting<int>& setting, int step = 1, QObject* parent = nullptr)
        : FontSizeHelper(Config{setting(), setting.minimum(), setting.maximum(), step,
                                [&setting](int size) { setting.set(size); }},
                         parent)
    {
    }

    /// 現在のフォントサイズを返す。
    int fontSize() const;
    /// 大きさを決める（範囲に収める。変わったら保存して知らせる）
    void setFontSize(int size);
    int minSize() const { return m_minSize; }
    int maxSize() const { return m_maxSize; }

public slots:
    /// フォントサイズを step 分増加する。変更があった場合 true を返す。
    bool increase();
    /// フォントサイズを step 分減少する。変更があった場合 true を返す。
    bool decrease();

signals:
    /// 大きさが変わった（保存済み）
    void fontSizeChanged(int size);

private:
    int m_fontSize;
    int m_minSize;
    int m_maxSize;
    int m_step;
    std::function<void(int)> m_saveFn;
};

#endif // FONTSIZEHELPER_H
