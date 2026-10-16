#ifndef SFENCOLLECTIONDIALOG_H
#define SFENCOLLECTIONDIALOG_H

/// @file sfencollectiondialog.h
/// @brief SFEN局面集ビューアダイアログクラスの定義


#include <QDialog>
#include <QList>
#include <QStringList>

#include "recentfilelist.h"

class BoardViewerWidget;
class QPushButton;
class QLabel;
class RecentFilesMenu;

/**
 * @brief SFEN局面集ビューアダイアログ
 *
 * SFEN形式の局面集（1行1局面）をファイルから読み込み、
 * 将棋盤で閲覧・ナビゲーションし、選択した局面をメインGUIに反映する。
 */
class SfenCollectionDialog : public QDialog
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_sfen_collection.cpp がパース結果とナビゲーション状態を直接検証する
    friend class TestSfenCollection;
#endif

public:
    explicit SfenCollectionDialog(QWidget* parent = nullptr);
    ~SfenCollectionDialog() override = default;

signals:
    /// 「選択」ボタン押下時に現在表示中の局面SFENを発行
    void positionSelected(const QString& sfen);

private slots:
    /// ファイルを開くダイアログを表示
    void onOpenFileClicked();
    /// 表示する局面が変わったときに位置ラベルを更新
    void onCurrentIndexChanged(int index);
    /// 選択ボタン押下時の処理
    void onSelectClicked();
    /// 最近使ったファイルの項目が選ばれたとき
    void onRecentFileTriggered(const QString& filePath);
    /// 「履歴をクリア」が選ばれたとき
    void onClearRecentFilesTriggered();

private:
    /// UIを構築
    void buildUi();
    /// ファイルを読み込み、行ごとにパースして盤に渡す
    bool loadFromFile(const QString& filePath);
    /// テキストを行ごとにパースして局面の一覧を返す。局面として読めなかった行の数を skippedLines に返す
    static QStringList parseSfenLines(const QString& text, int* skippedLines = nullptr);
    /// 最近使ったファイルの一覧が変わったとき、メニューを作り直して保存する
    void onRecentFilesChanged();

protected:
    /// ウィンドウを閉じる際にサイズ・履歴・盤の大きさを保存
    void closeEvent(QCloseEvent* event) override;

private:
    BoardViewerWidget* m_viewer = nullptr;  ///< 盤・拡大縮小・回転・局面送り

    // 最近使ったファイル
    RecentFileList m_recentFiles;            ///< 最近使ったファイル（最大5件）
    QPushButton* m_btnRecentFiles = nullptr; ///< 「履歴」ボタン
    RecentFilesMenu* m_recentFilesMenu = nullptr; ///< 「履歴」ボタンのメニュー

    // UI要素
    QPushButton* m_btnOpenFile = nullptr;
    QPushButton* m_btnSelect = nullptr;
    QLabel* m_fileLabel = nullptr;      ///< 読み込んだファイル名表示
};

#endif // SFENCOLLECTIONDIALOG_H
