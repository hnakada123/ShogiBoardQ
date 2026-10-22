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

class QCheckBox;
class QCloseEvent;
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
 */
class JosekiWindow : public QWidget
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_josekiwindow.cpp が保存状態とマージ処理を直接検証する
    friend class TestJosekiWindow;
#endif

public:
    /// session は開いている定跡（必須。非所有で、ウィンドウより長く生きる）
    explicit JosekiWindow(JosekiBookSession *session, QWidget *parent = nullptr);
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
    /// 定跡を表に出しているか（「■ 停止」で止めていなければ true。盤の矢印も止めているときは出さない）
    bool isDisplayEnabled() const { return m_displayEnabled; }
    /// 定跡に手を登録できるか（前回のファイルの読み込みを待っておらず、読み書き中でない）。
    /// 前回のファイルは初めて表示したときに読み込み、それまでのデータを置き換えるので、その前は登録させない
    bool canRegisterMoves() const;
    /// 「盤の手を登録」がオンか
    bool isRecordingBoardMoves() const;
    /// 表示中の手順が定跡を外れた手を出す（ply > 0 ならその手数の手 moveText、0 ならすべて定跡、負なら出さない）
    void setBookDeviation(int ply, const QString &moveText);

public slots:
    /// 「盤に矢印」のチェック（「表示」メニューの「盤に定跡手を表示」と同じ状態にする）
    void setBoardArrowsChecked(bool checked);
    /// 盤で指した手・対局で指された手（sfen は指す前の局面）。「盤の手を登録」がオンなら、
    /// まだ無い手を予想応手なし・評価値0・深さ0・出現頻度1で登録する（ある手は変えない）
    void recordBoardMove(const QString &sfen, const QString &usiMove);

signals:
    void josekiMoveSelected(const QString &usiMove);
    void requestKifuDataForMerge();
    /// 「盤に矢印」のチェックを変えた
    void boardArrowsToggled(bool checked);
    /// 「■ 停止」「▶ 再開」で定跡の表示を止めた・再開した
    void displayEnabledChanged(bool enabled);
    /// 「棋譜に展開...」で、表示中の局面から定跡を棋譜の変化に展開する条件を選んだ
    void expandRequested(const JosekiLines::ExpandOptions &options);
    /// 定跡を外れた手（の表示）を押した（その手数へ移る）
    void deviationPlyRequested(int ply);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    /// 定跡ファイル（.db）1つのドロップは開き、棋譜ファイル・フォルダは複数の棋譜から登録するダイアログに並べる
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // --- 開いている定跡の変化 ---
    void onFilePathChanged();
    void onBookLoaded();
    void onRecentFilesChanged();
    void onBusyChanged(bool busy);
    void onModifiedChanged(bool modified);
    /// マージの登録の保存が終わった（定跡が変わったので表を作り直す）
    void onMergeFinished();
    /// 定跡の中身が変わった（ほかの部品が登録したときも、表を作り直す）
    void onContentChanged();

    // --- 定跡手の操作 ---
    void onAddMoveButtonClicked();
    /// 表のボタン・ダブルクリック・右クリックのメニューの操作
    void onRowActionRequested(JosekiTableView::RowAction action, int row);

    // --- 表示 ---
    void onSfenDetailToggled(bool checked);
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    void onStopButtonClicked();
    void onRecordBoardMovesToggled(bool checked);
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
    void updateWindowTitle();
    void showFilePath();
    void updateRecentFilesMenu();

    // --- 定跡手の操作 ---
    bool isIoBusy() const;
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
    bool          m_displayEnabled = true;
    /// 表に出している局面。同じ局面が続けて設定されたときに表を作り直さないための覚え
    QString       m_displayedSfen;
    /// 遅延読込（初めて表示するときに前回のファイルを読み込む）。空なら読み込まない
    QString       m_pendingAutoLoadPath;
    /// ドロップされたファイル・フォルダ（ドロップの処理の後で開く）
    QStringList   m_droppedPaths;

    /// 親ドックウィジェット
    QDockWidget  *m_dockWidget = nullptr;
};

#endif // JOSEKIWINDOW_H
