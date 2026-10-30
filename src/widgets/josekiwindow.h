/// @file josekiwindow.h
/// @brief 定跡ウィンドウ（ドックのパネル）の定義

#ifndef JOSEKIWINDOW_H
#define JOSEKIWINDOW_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

#include "fontsizehelper.h"
#include "josekitableview.h"
#include "josekitypes.h"

class QAction;
class QCheckBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QToolButton;
class JosekiBookSession;
class RecentFilesMenu;
class JosekiTableModel;

/**
 * @brief 定跡ウィンドウ（ドックのパネル）
 *
 * 表示中の局面の定跡手を表に出し、利用者の操作を意図のシグナルで知らせる。
 * - 開いている定跡ファイル（データ・パス・変更状態・履歴・読み書き）は JosekiBookSession（合成ルートが作り、
 *   ほかの部品とも共有する。ウィンドウは表示に読み、表の並べ替えだけを直接書く）
 * - ファイル（開く・新規・保存・履歴）・定跡手の追加・編集・削除・マージ・ドロップ・棋譜への展開は、
 *   シグナルで知らせる（ダイアログを出して処理するのは ui の JosekiBookController・JosekiWindowController）
 * - 表の中身は JosekiTableModel（「着手」「編集」「削除」の列はボタンとして描く）
 * - 定跡の表示の停止・盤の手の登録・盤の矢印は、渡された QAction を正本にする（ボタンとチェックはそれに結ぶだけ）
 * - 前回のファイルの読み込みは、初めて表示したときにセッションに頼む（JosekiBookSession::startDeferredLoad）
 */
class JosekiWindow : public QWidget
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_josekiwindow.cpp が部品と表示を直接検証する
    friend class TestJosekiWindow;
#endif

public:
    /// 定跡ウィンドウのボタンとチェックが結ぶ切り替え（どれも checkable。無いものはウィンドウが作る）
    struct Actions {
        QAction *display = nullptr;           ///< 定跡を表に出す（外すと「■ 停止」の状態。盤の矢印も出さない）
        QAction *recordBoardMoves = nullptr;  ///< 盤で指した手を登録する（「盤の手を登録」）
        QAction *boardArrows = nullptr;       ///< 盤に定跡手の矢印を出す（「表示」メニューの「盤に定跡手を表示」）
    };

    /// 「マージ ▼」の項目
    enum class MergeSource {
        CurrentKifu,  ///< 現在の棋譜から（表示中の手順）
        KifuFile,     ///< 棋譜ファイルから
        KifuFiles,    ///< 複数の棋譜から...
        BookFile,     ///< 定跡ファイルから...
    };
    Q_ENUM(MergeSource)

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

    /// 定跡手を盤に指せなかったことを知らせる
    void showMoveFailed(const QString &usiMove);
    /// 表示中の手順が定跡を外れた手を出す（ply > 0 ならその手数の手 moveText、0 ならすべて定跡、負なら出さない）
    void setBookDeviation(int ply, const QString &moveText);

public slots:
    /// 状態表示に text を少しの間出す（「読み込み中...」など）
    void showTransientStatus(const QString &text);

signals:
    // --- ファイル（「ファイル」のボタンと「履歴」のメニュー） ---
    void newBookRequested();
    void openBookRequested();
    void saveBookRequested();
    void saveBookAsRequested();
    void recentFileRequested(const QString &filePath);
    void clearRecentFilesRequested();
    /// 「マージ ▼」の項目を選んだ
    void mergeRequested(JosekiWindow::MergeSource source);
    /// ファイル・フォルダをドロップした（定跡ファイル1つなら開き、それ以外は複数の棋譜から登録する）
    void pathsDropped(const QStringList &paths);

    // --- 定跡手（sfen は操作を選んだときに表に出していた局面） ---
    void addMoveRequested(const QString &sfen);
    void editMoveRequested(const QString &sfen, const JosekiMove &move);
    void deleteMoveRequested(const QString &sfen, const QString &usiMove);
    /// 定跡手を盤に指す（指せるときだけ出す）
    void josekiMoveSelected(const QString &usiMove);

    /// 「棋譜に展開...」を押した（表示中の局面から定跡を棋譜の変化に展開する）
    void expandRequested();
    /// 定跡を外れた手（の表示）を押した（その手数へ移る）
    void deviationPlyRequested(int ply);

protected:
    /// 初めて表示したときに前回のファイルを読み込む
    void showEvent(QShowEvent *event) override;
    /// 定跡を変えられるときだけ、ファイル・フォルダのドロップを受ける
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

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
    void onMergeMenuTriggered(QAction *action);

    // --- 表示 ---
    void onSfenDetailToggled(bool checked);
    void onAutoLoadToggled(bool checked);
    /// 「■ 停止」「▶ 再開」を押した（表示の切り替えを変える）
    void onStopButtonClicked(bool stopped);
    /// 表示の切り替えが変わった（ボタンの文字と表を合わせる）
    void onDisplayToggled(bool displayed);
    void onExpandButtonClicked();
    void onDeviationClicked();
    void onRestoreStatusDisplay();
    /// 利用者が列幅を変えた（保存する）
    void saveColumnWidths();

private:
    // --- 画面 ---
    void setupUi();
    QWidget *buildToolbar();
    void applyFontSize();
    void loadSettings();
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
    /// 人間が今、定跡手を指せるか（問い合わせの関数が無ければ指せる）
    bool humanCanPlayNow() const;
    void playMoveAt(int row);
    /// 表の行の操作を今できるか（右クリックのメニューの項目を押せるか）
    bool isRowActionAvailable(JosekiTableView::RowAction action, int row) const;
    /// 先頭へ・上へ・下へ移動と、出現頻度・評価値の順に並べる
    void reorderMoveAt(JosekiTableView::RowAction action, int row);

    // --- データ ---
    JosekiBookSession *m_session = nullptr;
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
    std::function<bool()> m_humanCanPlayProvider;
    /// 表に出している局面。同じ局面が続けて設定されたときに表を作り直さないための覚え
    QString       m_displayedSfen;
    /// 新規・読み込み・保存でファイルを選んだ（選ぶまではファイル名の欄に「未選択」と出す）
    bool          m_fileChosen = false;
};

#endif // JOSEKIWINDOW_H
