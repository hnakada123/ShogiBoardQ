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
class QPushButton;
class QShowEvent;
class QTableWidget;
class QToolButton;
class JosekiBookSession;
class JosekiPresenter;

/**
 * @brief 定跡ウィンドウクラス（薄い View 層）
 *
 * 定跡データベースの表示・編集を行うウィンドウ。
 * - 開いている定跡ファイル（データ・パス・変更状態・履歴・非同期の読み書き）は JosekiBookSession
 * - 定跡手の追加・編集・削除、マージ候補の作成、局面の要約は JosekiPresenter
 * 本クラスは画面の構築と表示（josekiwindowui.cpp）と、利用者の操作・確認ダイアログの仲立ちだけを担当する。
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

    /// 現在の棋譜を受け取り、マージダイアログを開く（requestKifuDataForMerge への応答）
    void setKifuDataForMerge(const QStringList &sfenList,
                             const QStringList &moveList,
                             const QStringList &japaneseMoveList,
                             int currentPly);
    /// 定跡手を盤に指した結果（指せなかったときだけ知らせる）
    void onMoveResult(bool success, const QString &usiMove);

signals:
    void josekiMoveSelected(const QString &usiMove);
    void requestKifuDataForMerge();
    void mergeRegistrationFinished(const QString &sfen, const QString &usiMove, bool success);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // --- ファイル ---
    void onOpenButtonClicked();
    void onNewButtonClicked();
    void onSaveButtonClicked();
    void onSaveAsButtonClicked();
    void onRecentFileClicked();
    void onClearRecentFilesClicked();
    void onBookLoaded(const QString &filePath);
    void onBookLoadFailed(const QString &errorMessage);
    void onBookSaved(const QString &filePath);
    void onBookSaveFailed(const QString &errorMessage);
    void onBusyChanged(bool busy);
    void onModifiedChanged(bool modified);
    void updateRecentFilesMenu();

    // --- 定跡手の操作 ---
    void onAddMoveButtonClicked();
    void onPlayButtonClicked();
    void onEditButtonClicked();
    void onDeleteButtonClicked();
    void onTableDoubleClicked(int row, int column);
    void onTableContextMenu(const QPoint &pos);
    void onContextMenuPlay();
    void onContextMenuEdit();
    void onContextMenuDelete();
    void onContextMenuCopyMove();

    // --- 表示 ---
    void onFontSizeIncrease();
    void onFontSizeDecrease();
    void onSfenDetailToggled(bool checked);
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    void onStopButtonClicked();
    void onRestoreStatusDisplay();

    // --- マージ（josekiwindowmerge.cpp） ---
    void onMergeFromCurrentKifu();
    void onMergeFromKifuFile();
    void onMergeRegisterMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove);

private:
    /// マージの「全て登録」：未登録の指し手をまとめて登録し、定跡ファイルへ1回だけ保存する
    void onMergeRegisterAllMoves(const QList<KifuMergeEntry> &entries);
    void openMergeDialog(const QList<KifuMergeEntry> &entries, const QString &title);

    // --- 画面（josekiwindowui.cpp） ---
    void setupUi();
    void applyFontSize();
    void loadSettings();
    void saveSettings();
    void updateJosekiDisplay();
    void rebuildTableRows(const QList<JosekiMove> &moves);
    void clearTable();
    void updateStatusDisplay();
    void updatePositionSummary();
    void updateWindowTitle();
    void showTransientStatus(const QString &text);
    void showFilePath();

    // --- ファイル（josekiwindowio.cpp） ---
    void setModified(bool modified);
    bool isIoBusy() const;
    bool confirmDiscardChanges();
    void loadAndApplyFileAsync(const QString &filePath);
    bool saveToFile(const QString &filePath);
    void saveToFileAsync(const QString &filePath);
    bool ensureFilePath();
    void closeMergeDialogs();
    QString selectSaveFilePath();
    void applySavedFilePath(const QString &filePath);

    // --- 定跡手の操作 ---
    void editMoveAt(int row);
    void deleteMoveAt(int row);
    void playMoveAt(int row);
    void refreshHumanCanPlay();

    // --- データとロジック ---
    JosekiBookSession *m_session = nullptr;
    JosekiPresenter  *m_presenter = nullptr;

    // === ファイルグループ ===
    QPushButton  *m_openButton = nullptr;
    QPushButton  *m_newButton = nullptr;
    QPushButton  *m_saveButton = nullptr;
    QPushButton  *m_saveAsButton = nullptr;
    QPushButton  *m_recentButton = nullptr;
    QMenu        *m_recentFilesMenu = nullptr;
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
    QTableWidget *m_tableWidget = nullptr;
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
    QList<JosekiMove> m_currentMoves;
    /// 表に出している局面。同じ局面が続けて設定されたときに表を作り直さないための覚え
    QString       m_displayedSfen;
    /// 遅延読込（初めて表示するときに前回のファイルを読み込む）。空なら読み込まない
    QString       m_pendingAutoLoadPath;

    /// 親ドックウィジェット
    QDockWidget  *m_dockWidget = nullptr;
};

#endif // JOSEKIWINDOW_H
