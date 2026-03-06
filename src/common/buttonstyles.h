/// @file buttonstyles.h
/// @brief アプリケーション全体で使用するボタンスタイルシートの定義

#ifndef BUTTONSTYLES_H
#define BUTTONSTYLES_H

#include <QString>

namespace ButtonStyles {

namespace detail {

/// 共通スタイルシート生成（QPushButton + QToolButton 両対応）
inline QString buildDualStyle(const char* bg, const char* fg, const char* border,
                              const char* hoverBg, const char* pressedBg,
                              const char* disabledBg, const char* disabledFg,
                              const char* disabledBorder,
                              const char* extra = "",
                              const char* additionalRules = "")
{
    return QStringLiteral("QPushButton, QToolButton {"
                          "  background-color: %1; color: %2;"
                          "  border: 1px solid %3; border-radius: 3px;"
                          "  %4"
                          "}"
                          "%5"
                          "QPushButton:hover, QToolButton:hover { background-color: %6; }"
                          "QPushButton:pressed, QToolButton:pressed { background-color: %7; }"
                          "QPushButton:disabled, QToolButton:disabled {"
                          "  background-color: %8; color: %9; border: 1px solid %10;"
                          "}")
        .arg(QLatin1String(bg), QLatin1String(fg), QLatin1String(border),
             QLatin1String(extra), QLatin1String(additionalRules),
             QLatin1String(hoverBg), QLatin1String(pressedBg),
             QLatin1String(disabledBg), QLatin1String(disabledFg))
        .arg(QLatin1String(disabledBorder));
}

/// QPushButton 専用スタイルシート生成
inline QString buildPushStyle(const char* bg, const char* fg,
                              const char* hoverBg, const char* pressedBg,
                              const char* disabledBg, const char* disabledFg,
                              const char* extra = "")
{
    return QStringLiteral("QPushButton {"
                          "  background-color: %1; color: %2;"
                          "  border: none; border-radius: 3px;"
                          "  %3"
                          "}"
                          "QPushButton:hover { background-color: %4; }"
                          "QPushButton:pressed { background-color: %5; }"
                          "QPushButton:disabled {"
                          "  background-color: %6; color: %7;"
                          "}")
        .arg(QLatin1String(bg), QLatin1String(fg), QLatin1String(extra),
             QLatin1String(hoverBg), QLatin1String(pressedBg),
             QLatin1String(disabledBg), QLatin1String(disabledFg));
}

} // namespace detail

/// A+/A- フォントサイズボタン（ライトブルー）
inline QString fontButton()
{
    return detail::buildDualStyle(
        "#e3f2fd", "", "#90caf9",
        "#bbdefb", "#90caf9",
        "#f5f5f5", "#9e9e9e", "#e0e0e0");
}

/// ▲▼ 棋譜ナビゲーションボタン（緑）
inline QString navigationButton()
{
    return detail::buildDualStyle(
        "#4F9272", "white", "#3d7259",
        "#5ba583", "#3d7259",
        "#b0c4b8", "#e0e0e0", "#9eb3a5",
        "padding: 3px 4px; font-weight: bold; min-width: 28px; max-width: 36px;");
}

/// 横長ナビゲーションボタン（テキスト付きダイアログ用）
inline QString wideNavigationButton()
{
    return detail::buildDualStyle(
        "#4F9272", "white", "#3d7259",
        "#5ba583", "#3d7259",
        "#b0c4b8", "#e0e0e0", "#9eb3a5",
        "padding: 4px 8px; font-weight: bold;");
}

/// 開始/適用/取り込み/選択ボタン（青）
inline QString primaryAction()
{
    return detail::buildDualStyle(
        "#1976d2", "white", "#1565c0",
        "#1e88e5", "#1565c0",
        "#b0bec5", "#eceff1", "#90a4ae",
        "padding: 2px 8px;");
}

/// キャンセル/閉じる/回転/拡縮ボタン（グレー）
inline QString secondaryNeutral()
{
    return detail::buildDualStyle(
        "#e0e0e0", "", "#bdbdbd",
        "#d0d0d0", "#bdbdbd",
        "#f5f5f5", "#9e9e9e", "#e0e0e0",
        "padding: 4px 8px;");
}

/// 中止/キャンセル(対局)ボタン（オレンジ）
inline QString dangerStop()
{
    return detail::buildDualStyle(
        "#fff3e0", "", "#ffcc80",
        "#ffe0b2", "#ffcc80",
        "#f5f5f5", "#9e9e9e", "#e0e0e0",
        "padding: 4px 12px;",
        "QPushButton:checked, QToolButton:checked { background-color: #ffcc80; }");
}

/// 切取/コピー/貼付/追加/マージボタン（緑）
inline QString editOperation()
{
    return detail::buildDualStyle(
        "#e8f5e9", "", "#a5d6a7",
        "#c8e6c9", "#a5d6a7",
        "#f5f5f5", "#9e9e9e", "#e0e0e0",
        "padding: 4px 8px;",
        "QToolButton::menu-indicator { image: none; }");
}

/// 元に戻す/やり直しボタン（オレンジ）
inline QString undoRedo()
{
    return detail::buildDualStyle(
        "#fff3e0", "", "#ffcc80",
        "#ffe0b2", "#ffcc80",
        "#f5f5f5", "#9e9e9e", "#e0e0e0");
}

/// 列表示トグルボタン（ブルーグレー、checked: 濃い青）
inline QString toggleButton()
{
    return detail::buildDualStyle(
        "#8a9bb5", "white", "#6b7d99",
        "#5a82b8", "#3d5a80",
        "#c0c8d4", "#e0e0e0", "#a8b4c4",
        "padding: 2px 4px; min-width: 28px; max-width: 36px;",
        "QPushButton:checked, QToolButton:checked {"
        "  background-color: #4A6FA5; border: 1px solid #3d5a80;"
        "}");
}

/// ファイル開く/保存/履歴ボタン（ライトブルー）
inline QString fileOperation()
{
    return detail::buildDualStyle(
        "#e3f2fd", "", "#90caf9",
        "#bbdefb", "#90caf9",
        "#f5f5f5", "#9e9e9e", "#e0e0e0",
        "padding: 4px 8px;");
}

/// カスタマイズ/歯車ボタン（オレンジ）
inline QString customizeSettings()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  font-size: 16px; font-weight: bold;"
        "  border: none; border-radius: 4px;"
        "  padding: 4px 6px; min-width: 28px;"
        "  background-color: #FF9800; color: white;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #F57C00; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #E65100; }"
        "QPushButton:checked, QToolButton:checked {"
        "  background-color: #E65100; color: white;"
        "}"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #ffe0b2; color: #bdbdbd;"
        "}");
}

/// MenuButtonWidget メインボタン（青）
inline QString menuMainButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  border: none; border-radius: 6px;"
        "  background-color: #42A5F5; color: white; padding: 4px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #1E88E5; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #1565C0; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #B0BEC5; color: #ECEFF1;"
        "}");
}

/// + 追加ボタン（緑）
inline QString menuAddButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  border: 1px solid #4CAF50; border-radius: 9px;"
        "  background-color: #4CAF50; color: white;"
        "  font-weight: bold; font-size: 12px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #43A047; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #c8e6c9; color: #a5d6a7; border: 1px solid #c8e6c9;"
        "}");
}

/// × 削除ボタン（赤）
inline QString menuRemoveButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  border: 1px solid #f44336; border-radius: 9px;"
        "  background-color: #f44336; color: white;"
        "  font-weight: bold; font-size: 12px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #E53935; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #ffcdd2; color: #ef9a9a; border: 1px solid #ffcdd2;"
        "}");
}

/// テーブル内「着手」ボタン（青、パディング小）
inline QString tablePlayButton()
{
    return detail::buildPushStyle(
        "#1976d2", "white",
        "#1e88e5", "#1565c0",
        "#b0bec5", "#eceff1",
        "padding: 2px 8px;");
}

/// テーブル内「編集」ボタン（緑、白文字）
inline QString tableEditButton()
{
    return detail::buildPushStyle(
        "#e8f5e9", "#2e7d32",
        "#c8e6c9", "#a5d6a7",
        "#f5f5f5", "#9e9e9e",
        "padding: 2px 8px;");
}

/// テーブル内「削除」ボタン（赤）
inline QString tableDeleteButton()
{
    return detail::buildPushStyle(
        "#f44336", "white",
        "#E53935", "#c62828",
        "#ffcdd2", "#ef9a9a",
        "padding: 2px 8px;");
}

/// テーブル内「登録」ボタン（青）
inline QString tableRegisterButton()
{
    return detail::buildPushStyle(
        "#1976d2", "white",
        "#1e88e5", "#1565c0",
        "#aaa", "#666",
        "padding: 4px 12px;");
}

} // namespace ButtonStyles

#endif // BUTTONSTYLES_H
