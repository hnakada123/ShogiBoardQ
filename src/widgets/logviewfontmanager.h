#ifndef LOGVIEWFONTMANAGER_H
#define LOGVIEWFONTMANAGER_H

/// @file logviewfontmanager.h
/// @brief ログビューのフォントサイズ管理ヘルパークラスの定義

#include <functional>

class QWidget;

/**
 * @brief ログビュー共通のフォントサイズ管理ヘルパー
 *
 * フォントサイズの増減・クランプ（8〜24pt）・適用ロジックを共通化する。
 * QObject ではなくシンプルなヘルパークラス。
 */
class LogViewFontManager
{
public:
    using PostApplyCallback = std::function<void(int fontSize)>;

    LogViewFontManager(int& fontSize, QWidget* targetWidget);

    void setPostApplyCallback(PostApplyCallback callback);

    void increase();
    void decrease();
    void apply();

private:
    int& m_fontSize;
    QWidget* m_targetWidget;
    PostApplyCallback m_postApply;
};

#endif // LOGVIEWFONTMANAGER_H
