#ifndef RECORDPANEAPPEARANCEMANAGER_H
#define RECORDPANEAPPEARANCEMANAGER_H

/// @file recordpaneappearancemanager.h
/// @brief 棋譜欄の外観管理クラスの定義

#include <QString>

class QTableView;

/**
 * @brief 棋譜欄の外観管理（フォントサイズ、列表示、スタイルシート）
 *
 * RecordPaneのUIロジックを分離:
 * - 文字の大きさの反映
 * - 列表示トグル（設定永続化 + リサイズモード計算）
 * - スタイルシート生成
 * - 選択パレット設定
 * - 棋譜ビューの有効/無効化
 */
class RecordPaneAppearanceManager {
public:
    explicit RecordPaneAppearanceManager(int initialFontSize);

    // 文字の大きさ（範囲と保存は RecordPane の FontSizeHelper が受け持つ）
    void setFontSize(int size) { m_fontSize = size; }
    int fontSize() const { return m_fontSize; }
    void applyFontToViews(QTableView* kifu, QTableView* branch);

    // Column management
    void toggleTimeColumn(QTableView* kifu, bool visible);
    void toggleBookmarkColumn(QTableView* kifu, bool visible);
    void toggleCommentColumn(QTableView* kifu, bool visible);
    void applyColumnVisibility(QTableView* kifu);
    static void updateColumnResizeModes(QTableView* kifu);

    // Selection appearance
    static void setupSelectionPalette(QTableView* view);

    // Stylesheet generation
    static QString kifuTableStyleSheet(int fontSize);
    static QString branchTableStyleSheet(int fontSize);

    // Kifu view enable/disable
    void setKifuViewEnabled(QTableView* kifu, bool on, bool& navigationDisabled);

private:
    int m_fontSize;
};

#endif // RECORDPANEAPPEARANCEMANAGER_H
