#ifndef SFENCOLLECTIONDIALOG_H
#define SFENCOLLECTIONDIALOG_H

/// @file sfencollectiondialog.h
/// @brief SFEN局面集ビューアダイアログクラスの定義


#include <QDialog>
#include <QVector>
#include <QStringList>

class ShogiView;
class ShogiBoard;
class QPushButton;
class QLabel;
class QMenu;

/**
 * @brief SFEN局面集ビューアダイアログ
 *
 * SFEN形式の局面集（1行1局面）をファイルから読み込み、
 * 将棋盤で閲覧・ナビゲーションし、選択した局面をメインGUIに反映する。
 */
class SfenCollectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SfenCollectionDialog(QWidget* parent = nullptr);
    ~SfenCollectionDialog() override;

signals:
    /// 「選択」ボタン押下時に現在表示中の局面SFENを発行
    void positionSelected(const QString& sfen);

private slots:
    /// ファイルを開くダイアログを表示
    void onOpenFileClicked();
    /// 最初の局面に移動
    void onGoFirst();
    /// 前の局面に移動
    void onGoBack();
    /// 次の局面に移動
    void onGoForward();
    /// 最後の局面に移動
    void onGoLast();
    /// 将棋盤を拡大
    void onEnlargeBoard();
    /// 将棋盤を縮小
    void onReduceBoard();
    /// 盤面の回転
    void onFlipBoard();
    /// 選択ボタン押下時の処理
    void onSelectClicked();
    /// 最近使ったファイルメニューの項目がクリックされたときのスロット
    void onRecentFileClicked();
    /// 最近使ったファイル履歴をクリアするスロット
    void onClearRecentFilesClicked();

private:
    /// UIを構築
    void buildUi();
    /// ファイルを読み込み、行ごとにパースしてm_sfenListを構築
    bool loadFromFile(const QString& filePath);
    /// テキストを行ごとにパースしてm_sfenListを構築
    void parseSfenLines(const QString& text);
    /// 盤面を現在のインデックスで更新
    void updateBoardDisplay();
    /// ボタンの有効/無効を更新
    void updateButtonStates();
    /// 時計ラベルを非表示にする
    void hideClockLabels();
    /// ウィンドウサイズを保存
    void saveWindowSize();
    /// レイアウト確定後にウィンドウサイズを調整
    void adjustWindowToContents();
    /// 最近使ったファイルリストにファイルパスを追加
    void addToRecentFiles(const QString& filePath);
    /// 最近使ったファイルメニューを更新
    void updateRecentFilesMenu();
    /// 最近使ったファイルリストを保存
    void saveRecentFiles();

protected:
    /// ウィンドウを閉じる際にサイズを保存
    void closeEvent(QCloseEvent* event) override;
    /// ShogiViewのCtrl+ホイールイベントを横取りして処理
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ShogiView* m_shogiView = nullptr;
    ShogiBoard* m_board = nullptr;

    QVector<QString> m_sfenList;    ///< パース済みSFEN局面リスト
    int m_currentIndex = 0;         ///< 現在表示中の局面インデックス (0-based)
    QString m_currentFilePath;      ///< 読み込んだファイルのパス

    // 最近使ったファイル
    QStringList m_recentFiles;            ///< 最近使ったファイルリスト（最大5件）
    QPushButton* m_btnRecentFiles = nullptr; ///< 「履歴」ボタン
    QMenu* m_recentFilesMenu = nullptr;   ///< 最近使ったファイルメニュー

    // UI要素
    QPushButton* m_btnOpenFile = nullptr;
    QPushButton* m_btnFirst = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnForward = nullptr;
    QPushButton* m_btnLast = nullptr;
    QPushButton* m_btnEnlarge = nullptr;
    QPushButton* m_btnReduce = nullptr;
    QPushButton* m_btnFlip = nullptr;
    QPushButton* m_btnSelect = nullptr;
    QLabel* m_positionLabel = nullptr;  ///< 「局面: N / Total」表示
    QLabel* m_fileLabel = nullptr;      ///< 読み込んだファイル名表示
};

#endif // SFENCOLLECTIONDIALOG_H
