/// @file buttonstyles.h
/// @brief アプリケーション全体で使用するボタンスタイルシートの定義

#ifndef BUTTONSTYLES_H
#define BUTTONSTYLES_H

#include <QString>

namespace ButtonStyles {

/// A+/A- フォントサイズボタン（ライトブルー）
inline QString fontButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 3px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #bbdefb; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #90caf9; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0;"
        "}");
}

/// ▲▼ 棋譜ナビゲーションボタン（緑）
inline QString navigationButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #4F9272; color: white;"
        "  border: 1px solid #3d7259; border-radius: 3px;"
        "  padding: 3px 4px; font-weight: bold;"
        "  min-width: 28px; max-width: 36px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #5ba583; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #3d7259; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #b0c4b8; color: #e0e0e0; border: 1px solid #9eb3a5;"
        "}");
}

/// 横長ナビゲーションボタン（テキスト付きダイアログ用）
inline QString wideNavigationButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #4F9272; color: white;"
        "  border: 1px solid #3d7259; border-radius: 3px;"
        "  padding: 4px 8px; font-weight: bold;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #5ba583; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #3d7259; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #b0c4b8; color: #e0e0e0; border: 1px solid #9eb3a5;"
        "}");
}

/// 開始/適用/取り込み/選択ボタン（青）
inline QString primaryAction()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #1976d2; color: white;"
        "  border: 1px solid #1565c0; border-radius: 3px; padding: 2px 8px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #1e88e5; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #1565c0; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #b0bec5; color: #eceff1; border: 1px solid #90a4ae;"
        "}");
}

/// キャンセル/閉じる/回転/拡縮ボタン（グレー）
inline QString secondaryNeutral()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #e0e0e0; border: 1px solid #bdbdbd; border-radius: 3px;"
        "  padding: 4px 8px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #d0d0d0; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #bdbdbd; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0;"
        "}");
}

/// 中止/キャンセル(対局)ボタン（オレンジ）
inline QString dangerStop()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #fff3e0; border: 1px solid #ffcc80; border-radius: 3px;"
        "  padding: 4px 12px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #ffe0b2; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #ffcc80; }"
        "QPushButton:checked, QToolButton:checked { background-color: #ffcc80; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0;"
        "}");
}

/// 切取/コピー/貼付/追加/マージボタン（緑）
inline QString editOperation()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #e8f5e9; border: 1px solid #a5d6a7; border-radius: 3px;"
        "  padding: 4px 8px;"
        "}"
        "QToolButton::menu-indicator { image: none; }"
        "QPushButton:hover, QToolButton:hover { background-color: #c8e6c9; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #a5d6a7; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0;"
        "}");
}

/// 元に戻す/やり直しボタン（オレンジ）
inline QString undoRedo()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #fff3e0; border: 1px solid #ffcc80; border-radius: 3px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #ffe0b2; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #ffcc80; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0;"
        "}");
}

/// 列表示トグルボタン（ブルーグレー、checked: 濃い青）
inline QString toggleButton()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #8a9bb5; color: white;"
        "  border: 1px solid #6b7d99; border-radius: 3px;"
        "  padding: 2px 4px; min-width: 28px; max-width: 36px;"
        "}"
        "QPushButton:checked, QToolButton:checked {"
        "  background-color: #4A6FA5; border: 1px solid #3d5a80;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #5a82b8; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #3d5a80; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #c0c8d4; color: #e0e0e0; border: 1px solid #a8b4c4;"
        "}");
}

/// ファイル開く/保存/履歴ボタン（ライトブルー）
inline QString fileOperation()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        "  background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 3px;"
        "  padding: 4px 8px;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #bbdefb; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #90caf9; }"
        "QPushButton:disabled, QToolButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0;"
        "}");
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
    return QStringLiteral(
        "QPushButton {"
        "  background-color: #1976d2; color: white;"
        "  border: none; border-radius: 3px; padding: 2px 8px;"
        "}"
        "QPushButton:hover { background-color: #1e88e5; }"
        "QPushButton:pressed { background-color: #1565c0; }"
        "QPushButton:disabled {"
        "  background-color: #b0bec5; color: #eceff1;"
        "}");
}

/// テーブル内「編集」ボタン（緑、白文字）
inline QString tableEditButton()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: #e8f5e9; color: #2e7d32;"
        "  border: none; border-radius: 3px; padding: 2px 8px;"
        "}"
        "QPushButton:hover { background-color: #c8e6c9; }"
        "QPushButton:pressed { background-color: #a5d6a7; }"
        "QPushButton:disabled {"
        "  background-color: #f5f5f5; color: #9e9e9e;"
        "}");
}

/// テーブル内「削除」ボタン（赤）
inline QString tableDeleteButton()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: #f44336; color: white;"
        "  border: none; border-radius: 3px; padding: 2px 8px;"
        "}"
        "QPushButton:hover { background-color: #E53935; }"
        "QPushButton:pressed { background-color: #c62828; }"
        "QPushButton:disabled {"
        "  background-color: #ffcdd2; color: #ef9a9a;"
        "}");
}

/// テーブル内「登録」ボタン（青）
inline QString tableRegisterButton()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: #1976d2; color: white;"
        "  border: none; border-radius: 3px; padding: 4px 12px;"
        "}"
        "QPushButton:hover { background-color: #1e88e5; }"
        "QPushButton:pressed { background-color: #1565c0; }"
        "QPushButton:disabled {"
        "  background-color: #aaa; color: #666;"
        "}");
}

} // namespace ButtonStyles

#endif // BUTTONSTYLES_H
