/// @file josekiwindow.h
/// @brief 定跡ウィンドウクラスの定義

#ifndef JOSEKIWINDOW_H
#define JOSEKIWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QLabel>
#include <QTableWidget>
#include <QVector>
#include <QMenu>
#include <QStringList>
#include <QAction>

// 前方宣言
class QVBoxLayout;
class QGroupBox;
class QCloseEvent;
class QShowEvent;
class QFrame;
class QDockWidget;
class JosekiRepository;
class JosekiPresenter;

/**
 * @brief 定跡エントリの指し手情報
 */
struct JosekiMove {
    QString move;       ///< 指し手（USI形式）
    QString nextMove;   ///< 次の指し手（USI形式）
    int value;          ///< 評価値
    int depth;          ///< 深さ
    int frequency;      ///< 出現頻度
    QString comment;    ///< コメント
};

/**
 * @brief 定跡エントリ（局面ごとの定跡データ）
 */
struct JosekiEntry {
    QString sfen;                   ///< 局面のSFEN文字列
    QVector<JosekiMove> moves;      ///< この局面での指し手リスト
};

/**
 * @brief 定跡ウィンドウクラス（薄い View 層）
 *
 * 定跡データベースの表示・編集を行うウィンドウ。
 * ビジネスロジックは JosekiPresenter、I/O は JosekiRepository に委譲する。
 */
class JosekiWindow : public QWidget
{
    Q_OBJECT

public:
    explicit JosekiWindow(QWidget *parent = nullptr);
    ~JosekiWindow() override = default;

    void setCurrentSfen(const QString &sfen);
    void setHumanCanPlay(bool canPlay);
    void setDockWidget(QDockWidget *dock);

signals:
    void josekiMoveSelected(const QString &usiMove);
    void requestKifuDataForMerge();

public slots:
    void onOpenButtonClicked();
    void onNewButtonClicked();
    void onSaveButtonClicked();
    void onSaveAsButtonClicked();
    void onAddMoveButtonClicked();
    void setKifuDataForMerge(const QStringList &sfenList,
                             const QStringList &moveList,
                             const QStringList &japaneseMoveList,
                             int currentPly);
    void onFontSizeIncrease();
    void onFontSizeDecrease();
    void onMoveResult(bool success, const QString &usiMove);

private slots:
    void onClearRecentFilesClicked();
    void onSfenDetailToggled(bool checked);
    void onPlayButtonClicked();
    void onEditButtonClicked();
    void onDeleteButtonClicked();
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    void onStopButtonClicked();
    void onRecentFileClicked();
    void onMergeFromCurrentKifu();
    void onMergeFromKifuFile();
    void onTableDoubleClicked(int row, int column);
    void onTableContextMenu(const QPoint &pos);
    void onContextMenuPlay();
    void onContextMenuEdit();
    void onContextMenuDelete();
    void onContextMenuCopyMove();
    void onMergeRegisterMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove);
    void onRestoreStatusDisplay();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    void updateJosekiDisplay();
    void clearTable();
    void applyFontSize();
    void loadSettings();
    void saveSettings();
    void updateStatusDisplay();
    void updatePositionSummary();
    void updateWindowTitle();
    void setModified(bool modified);
    bool confirmDiscardChanges();
    void addToRecentFiles(const QString &filePath);
    void updateRecentFilesMenu();
    bool loadAndApplyFile(const QString &filePath);
    bool saveToFile(const QString &filePath);
    bool ensureFilePath();
    void editMoveAt(int row);
    void deleteMoveAt(int row);
    int currentPlyNumber() const;

    // --- Repository / Presenter ---
    JosekiRepository *m_repository = nullptr;
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
    QPushButton  *m_fontIncreaseBtn = nullptr;
    QPushButton  *m_fontDecreaseBtn = nullptr;
    QCheckBox    *m_autoLoadCheckBox = nullptr;

    // === 操作グループ ===
    QPushButton  *m_stopButton = nullptr;
    QPushButton  *m_addMoveButton = nullptr;
    QToolButton  *m_mergeButton = nullptr;
    QMenu        *m_mergeMenu = nullptr;

    // === 状態表示 ===
    QLabel       *m_currentSfenLabel = nullptr;
    QLabel       *m_sfenLineLabel = nullptr;
    QLabel       *m_statusLabel = nullptr;
    QLabel       *m_positionSummaryLabel = nullptr;
    QLabel       *m_emptyGuideLabel = nullptr;
    QLabel       *m_noticeLabel = nullptr;
    QPushButton  *m_showSfenDetailBtn = nullptr;
    QWidget      *m_sfenDetailWidget = nullptr;

    // === コンテキストメニュー ===
    QMenu        *m_tableContextMenu = nullptr;
    QAction      *m_actionPlay = nullptr;
    QAction      *m_actionEdit = nullptr;
    QAction      *m_actionDelete = nullptr;
    QAction      *m_actionCopyMove = nullptr;

    // === テーブル ===
    QTableWidget *m_tableWidget = nullptr;

    // === View 状態 ===
    QString       m_currentFilePath;
    QString       m_currentSfen;
    int           m_fontSize = 10;
    bool          m_humanCanPlay = true;
    bool          m_autoLoadEnabled = true;
    bool          m_displayEnabled = true;
    bool          m_modified = false;
    QStringList   m_recentFiles;
    QVector<JosekiMove> m_currentMoves;

    /// 遅延読込用
    bool          m_pendingAutoLoad = false;
    QString       m_pendingAutoLoadPath;

    /// 親ドックウィジェット
    QDockWidget  *m_dockWidget = nullptr;
};

#endif // JOSEKIWINDOW_H
