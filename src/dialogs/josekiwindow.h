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
#include "josekilines.h"
#include "josekitableview.h"
#include "recordline.h"

class QAction;
class QCheckBox;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QToolButton;
class JosekiBookSession;
class JosekiEditController;
class JosekiFileController;
class JosekiMergeController;
class RecentFilesMenu;
class JosekiTableModel;

/**
 * @brief 定跡ウィンドウ
 *
 * 表示中の局面の定跡手を表に出し、定跡手の追加・編集・削除・着手と棋譜からのマージを受け付ける。
 * - 開いている定跡ファイル（データ・パス・変更状態・履歴・読み書き）は JosekiBookSession（合成ルートが作り、
 *   ほかの部品とも共有する。ウィンドウは持たない）
 * - ファイルの利用者操作（開く・新規・保存・履歴）と確認ダイアログは JosekiFileController
 * - 定跡手の追加・編集・削除のダイアログは JosekiEditController、マージは JosekiMergeController
 * - 表の中身は JosekiTableModel（「着手」「編集」「削除」の列はボタンとして描く）
 * - 定跡の表示の停止・盤の手の登録・盤の矢印は、渡された QAction を正本にする（ボタンとチェックはそれに結ぶだけ）
 * - 前回のファイルの読み込みは、初めて表示したときにセッションに頼む（JosekiBookSession::startDeferredLoad）
 */
class JosekiWindow : public QWidget
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_josekiwindow.cpp が保存状態とマージ処理を直接検証する
    friend class TestJosekiWindow;
#endif

public:
    /// 定跡ウィンドウのボタンとチェックが結ぶ切り替え（どれも checkable。無いものはウィンドウが作る）
    struct Actions {
        QAction *display = nullptr;           ///< 定跡を表に出す（外すと「■ 停止」の状態。盤の矢印も出さない）
        QAction *recordBoardMoves = nullptr;  ///< 盤で指した手を登録する（「盤の手を登録」）
        QAction *boardArrows = nullptr;       ///< 盤に定跡手の矢印を出す（「表示」メニューの「盤に定跡手を表示」）
    };

    /// session は開いている定跡（必須。非所有で、ウィンドウより長く生きる）
    JosekiWindow(JosekiBookSession *session, const Actions &actions, QWidget *parent = nullptr);
    /// 切り替えの QAction をすべてウィンドウが作る
    explicit JosekiWindow(JosekiBookSession *session, QWidget *parent = nullptr);
    ~JosekiWindow() override;

    void setCurrentSfen(const QString &sfen);
    /// 定跡手を指せるかを問い合わせる関数。表示の更新や着手のたびにその場で判定する
    /// （定跡ファイルを開いた直後など、局面が変わらないときも最新の状態にするため）
    void setHumanCanPlayProvider(std::function<bool()> provider);
    /// 定跡手を指せるかを問い合わせ直し、変わっていれば表の「着手」を作り直す
    void updateHumanCanPlay();
    void setDockWidget(QDockWidget *dock);
    [[nodiscard]] bool confirmClose();

    /// 表示中の手順の手を選んで登録するダイアログを開く（requestKifuDataForMerge への応答）
    void openMergeForLine(const RecordLine &line);
    /// 定跡手を盤に指せなかったことを知らせる
    void showMoveFailed(const QString &usiMove);
    /// 表示中の手順が定跡を外れた手を出す（ply > 0 ならその手数の手 moveText、0 ならすべて定跡、負なら出さない）
    void setBookDeviation(int ply, const QString &moveText);

signals:
    void josekiMoveSelected(const QString &usiMove);
    void requestKifuDataForMerge();
    /// 「棋譜に展開...」で、表示中の局面から定跡を棋譜の変化に展開する条件を選んだ
    void expandRequested(const JosekiLines::ExpandOptions &options);
    /// 定跡を外れた手（の表示）を押した（その手数へ移る）
    void deviationPlyRequested(int ply);

protected:
    /// 初めて表示したときに前回のファイルを読み込む
    void showEvent(QShowEvent *event) override;
    /// 定跡ファイル（.db）1つのドロップは開き、棋譜ファイル・フォルダは複数の棋譜から登録するダイアログに並べる
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // --- 開いている定跡の変化 ---
    void onFilePathChanged();
    void onRecentFilesChanged();
    /// 読み書き中か・前回のファイルの読み込みを待つかが変わった（定跡を変えられるかが変わる）
    void onEditableChanged();
    void onModifiedChanged(bool modified);
    /// 定跡の中身が変わった（このウィンドウの編集も、ほかの部品の登録も、表はここで作り直す）
    void onContentChanged();

    // --- 定跡手の操作 ---
    void onAddMoveButtonClicked();
    /// 表のボタン・ダブルクリック・右クリックのメニューの操作
    void onRowActionRequested(JosekiTableView::RowAction action, int row);

    // --- 表示 ---
    void onSfenDetailToggled(bool checked);
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    /// 「■ 停止」「▶ 再開」を押した（表示の切り替えを変える）
    void onStopButtonClicked(bool stopped);
    /// 表示の切り替えが変わった（ボタンの文字と表を合わせる）
    void onDisplayToggled(bool displayed);
    void onExpandButtonClicked();
    void onDeviationClicked();
    void onRestoreStatusDisplay();
    void showTransientStatus(const QString &text);
    void openDroppedPaths();


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
    void showFilePath();
    void updateRecentFilesMenu();

    // --- 定跡手の操作 ---
    /// 定跡を変えられるか（読み書き中でなく、前回のファイルの読み込みを待っていない）
    bool canEdit() const;
    bool isDisplayEnabled() const;
    void editMoveAt(int row);
    void deleteMoveAt(int row);
    void playMoveAt(int row);
    /// 表の行の操作を今できるか（右クリックのメニューの項目を押せるか）
    bool isRowActionAvailable(JosekiTableView::RowAction action, int row);
    /// 先頭へ・上へ・下へ移動と、出現頻度・評価値の順に並べる
    void reorderMoveAt(JosekiTableView::RowAction action, int row);
    void refreshHumanCanPlay();


    // --- データとファイル ---
    JosekiBookSession *m_session = nullptr;
    JosekiFileController *m_files = nullptr;
    JosekiMergeController *m_merge = nullptr;
    JosekiEditController *m_edit = nullptr;
    JosekiTableModel *m_model = nullptr;

    // === ファイルグループ ===
    QPushButton  *m_openButton = nullptr;
    QPushButton  *m_newButton = nullptr;
    QPushButton  *m_saveButton = nullptr;
    QPushButton  *m_saveAsButton = nullptr;
    QPushButton  *m_recentButton = nullptr;
    QPushButton  *m_fontDecreaseButton = nullptr;
    QPushButton  *m_fontIncreaseButton = nullptr;
    RecentFilesMenu *m_recentFilesMenu = nullptr;
    QLabel       *m_filePathLabel = nullptr;
    QLabel       *m_fileStatusLabel = nullptr;

    // === 表示設定グループ ===
    QCheckBox    *m_autoLoadCheckBox = nullptr;
    QCheckBox    *m_boardArrowsCheckBox = nullptr;

    // === 操作グループ ===
    QPushButton  *m_stopButton = nullptr;
    QPushButton  *m_addMoveButton = nullptr;
    QToolButton  *m_mergeButton = nullptr;
    /// 盤で指した手を登録する（保存しない。起動するとオフ）
    QCheckBox    *m_recordBoardMovesCheckBox = nullptr;
    QPushButton  *m_expandButton = nullptr;
    /// 表示中の手順が定跡を外れた手（押すとその手へ移る）
    QPushButton  *m_deviationButton = nullptr;
    int           m_deviationPly = -1;
    Actions       m_actions;

    // === 状態表示 ===
    QLabel       *m_currentSfenLabel = nullptr;
    QLabel       *m_sfenLineLabel = nullptr;
    QLabel       *m_statusLabel = nullptr;
    QLabel       *m_positionSummaryLabel = nullptr;
    QLabel       *m_noticeLabel = nullptr;
    QPushButton  *m_showSfenDetailBtn = nullptr;
    QWidget      *m_sfenDetailWidget = nullptr;

    // === 表とコンテキストメニュー ===
    JosekiTableView *m_tableView = nullptr;

    // === View 状態 ===
    QString       m_currentSfen;
    FontSizeHelper m_fontHelper;
    bool          m_humanCanPlay = true;
    std::function<bool()> m_humanCanPlayProvider;
    bool          m_autoLoadEnabled = true;
    /// 表に出している局面。同じ局面が続けて設定されたときに表を作り直さないための覚え
    QString       m_displayedSfen;
    /// 新規・読み込み・保存でファイルを選んだ（選ぶまではファイル名の欄に「未選択」と出す）
    bool          m_fileChosen = false;
    /// ドロップされたファイル・フォルダ（ドロップの処理の後で開く）
    QStringList   m_droppedPaths;

    /// 親ドックウィジェット
    QDockWidget  *m_dockWidget = nullptr;
};

#endif // JOSEKIWINDOW_H
