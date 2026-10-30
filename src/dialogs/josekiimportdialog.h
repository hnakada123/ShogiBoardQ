#ifndef JOSEKIIMPORTDIALOG_H
#define JOSEKIIMPORTDIALOG_H

/// @file josekiimportdialog.h
/// @brief 複数の棋譜ファイル・フォルダから定跡に登録するダイアログ（「マージ ▼」→「複数の棋譜から...」・ドロップ）

#include <QDialog>
#include <QFutureWatcher>
#include <QStringList>

#include "josekiimport.h"
#include "settingsformbinding.h"

class JosekiBookSession;
class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;

/**
 * @brief 棋譜ファイルとフォルダを並べ、条件に合う手をまとめて定跡に登録する
 *
 * 棋譜の読み込みと手の集計（JosekiImport::collect）はワーカースレッドで行い、進み具合と「中止」を出す。
 * 集め終わったら JosekiBookSession::importMoves で登録し（保存はしない）、結果をダイアログに出す。
 * ファイル・フォルダはダイアログへのドロップでも足せる。条件は保存して次に開いたときに戻す。
 */
class JosekiImportDialog : public QDialog
{
    Q_OBJECT

public:
    /// paths は初めに並べるファイル・フォルダ（定跡ウィンドウへのドロップ）
    JosekiImportDialog(JosekiBookSession *session, const QStringList &paths, QWidget *parent = nullptr);
    ~JosekiImportDialog() override;

    /// 並べたファイル・フォルダ
    QStringList paths() const;
    /// 選んだ条件
    JosekiImport::Options options() const;
    /// 集めている最中か
    bool isRunning() const;

    void reject() override;

public slots:
    /// 「登録」: 集め始める
    void start();
    /// 「中止」
    void cancel();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void addFiles();
    void addFolder();
    void removeSelected();
    void clearPaths();
    void onProgressRangeChanged(int minimum, int maximum);
    void onProgressValueChanged(int value);
    void onFinished();
    void updateInputs();

private:
    void appendPaths(const QStringList &paths);

    JosekiBookSession *m_session = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_addFiles = nullptr;
    QPushButton *m_addFolder = nullptr;
    QPushButton *m_remove = nullptr;
    QPushButton *m_clear = nullptr;
    QSpinBox *m_maxPly = nullptr;
    QComboBox *m_sides = nullptr;
    QCheckBox *m_variations = nullptr;
    QSpinBox *m_minCount = nullptr;
    QCheckBox *m_comments = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_close = nullptr;
    SettingsFormBinding m_binding;
    QFutureWatcher<JosekiImport::Result> m_watcher;
};

#endif // JOSEKIIMPORTDIALOG_H
