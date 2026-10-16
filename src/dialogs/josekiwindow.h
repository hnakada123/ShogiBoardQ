/// @file josekiwindow.h
/// @brief 定跡ウィンドウクラスの定義

#ifndef JOSEKIWINDOW_H
#define JOSEKIWINDOW_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

#include "fontsizehelper.h"
#include "josekitypes.h"

class QAction;
class QCheckBox;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QMenu;
class QModelIndex;
class QPushButton;
class QShowEvent;
class QTableView;
class QToolButton;
class JosekiBookSession;
class JosekiFileController;
class RecentFilesMenu;
class JosekiTableModel;

/**
 * @brief 定跡ウィンドウ
 *
 * 表示中の局面の定跡手を表に出し、定跡手の追加・編集・削除・着手と棋譜からのマージを受け付ける。
 * - 開いている定跡ファイル（データ・パス・変更状態・履歴・読み書き）は JosekiBookSession
 * - ファイルの利用者操作（開く・新規・保存・履歴）と確認ダイアログは JosekiFileController
 * - 表の中身は JosekiTableModel（「着手」「編集」「削除」の列はボタンとして描く）
 */
class JosekiWindow : public QWidget
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_josekiwindow.cpp が保存状態とマージ処理を直接検証する
    friend class TestJosekiWindow;
#endif

public:
    explicit JosekiWindow(QWidget *parent = nullptr);
    ~JosekiWindow() override;

    void setCurrentSfen(const QString &sfen);
    void setHumanCanPlay(bool canPlay);
    /// 定跡手を指せるかを問い合わせる関数。設定すると、表示の更新や着手のたびにその場で判定する
    /// （定跡ファイルを開いた直後など、局面が変わらないときも最新の状態にするため）
    void setHumanCanPlayProvider(std::function<bool()> provider);
    void setDockWidget(QDockWidget *dock);
    [[nodiscard]] bool confirmClose();

    /// 表示中の手順を受け取り、マージダイアログを開く（requestKifuDataForMerge への応答）
    void setKifuDataForMerge(const QStringList &sfenList,
                             const QStringList &moveList,
                             const QStringList &japaneseMoveList,
                             int currentPly);
    /// 定跡手を盤に指せなかったことを知らせる
    void showMoveFailed(const QString &usiMove);

signals:
    void josekiMoveSelected(const QString &usiMove);
    void requestKifuDataForMerge();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // --- 開いている定跡の変化 ---
    void onFilePathChanged();
    void onBookLoaded();
    void onRecentFilesChanged();
    void onBusyChanged(bool busy);
    void onModifiedChanged(bool modified);
    void closeMergeDialogs();

    // --- 定跡手の操作 ---
    void onAddMoveButtonClicked();
    void onTableClicked(const QModelIndex &index);
    void onTableDoubleClicked(const QModelIndex &index);
    void onTableContextMenu(const QPoint &pos);
    void onContextMenuPlay();
    void onContextMenuEdit();
    void onContextMenuDelete();
    void onContextMenuCopyMove();

    // --- 表示 ---
    void onSfenDetailToggled(bool checked);
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    void onStopButtonClicked();
    void onRestoreStatusDisplay();
    void showTransientStatus(const QString &text);

    // --- マージ ---
    void onMergeFromCurrentKifu();
    void onMergeFromKifuFile();

private:
    // --- 画面 ---
    void setupUi();
    QWidget *buildToolbar();
    void applyFontSize();
    void loadSettings();
    void saveSettings();
    void updateJosekiDisplay();
    void clearTable();
    void updateStatusDisplay();
    void updatePositionSummary();
    void updateWindowTitle();
    void showFilePath();
    void updateRecentFilesMenu();

    // --- 定跡手の操作 ---
    bool isIoBusy() const;
    void editMoveAt(int row);
    void deleteMoveAt(int row);
    void playMoveAt(int row);
    void refreshHumanCanPlay();

    // --- マージ ---
    void openMergeDialog(const QList<KifuMergeEntry> &entries, const QString &title);
    /// マージダイアログの「登録」「全て登録」: 登録して定跡ファイルへ1回だけ保存する
    bool registerMergeEntries(const QList<KifuMergeEntry> &entries);

    // --- データとファイル ---
    JosekiBookSession *m_session = nullptr;
    JosekiFileController *m_files = nullptr;
    JosekiTableModel *m_model = nullptr;

    // === ファイルグループ ===
    QPushButton  *m_openButton = nullptr;
    QPushButton  *m_newButton = nullptr;
    QPushButton  *m_saveButton = nullptr;
    QPushButton  *m_saveAsButton = nullptr;
    QPushButton  *m_recentButton = nullptr;
    RecentFilesMenu *m_recentFilesMenu = nullptr;
    QLabel       *m_filePathLabel = nullptr;
    QLabel       *m_fileStatusLabel = nullptr;

    // === 表示設定グループ ===
    QCheckBox    *m_autoLoadCheckBox = nullptr;

    // === 操作グループ ===
    QPushButton  *m_stopButton = nullptr;
    QPushButton  *m_addMoveButton = nullptr;
    QToolButton  *m_mergeButton = nullptr;

    // === 状態表示 ===
    QLabel       *m_currentSfenLabel = nullptr;
    QLabel       *m_sfenLineLabel = nullptr;
    QLabel       *m_statusLabel = nullptr;
    QLabel       *m_positionSummaryLabel = nullptr;
    QLabel       *m_emptyGuideLabel = nullptr;
    QLabel       *m_noticeLabel = nullptr;
    QPushButton  *m_showSfenDetailBtn = nullptr;
    QWidget      *m_sfenDetailWidget = nullptr;

    // === 表とコンテキストメニュー ===
    QTableView   *m_tableView = nullptr;
    QMenu        *m_tableContextMenu = nullptr;
    QAction      *m_actionPlay = nullptr;
    QAction      *m_actionEdit = nullptr;
    QAction      *m_actionDelete = nullptr;

    // === View 状態 ===
    QString       m_currentSfen;
    FontSizeHelper m_fontHelper;
    bool          m_humanCanPlay = true;
    std::function<bool()> m_humanCanPlayProvider;
    bool          m_autoLoadEnabled = true;
    bool          m_displayEnabled = true;
    /// 表に出している局面。同じ局面が続けて設定されたときに表を作り直さないための覚え
    QString       m_displayedSfen;
    /// 遅延読込（初めて表示するときに前回のファイルを読み込む）。空なら読み込まない
    QString       m_pendingAutoLoadPath;

    /// 親ドックウィジェット
    QDockWidget  *m_dockWidget = nullptr;
};

#endif // JOSEKIWINDOW_H
