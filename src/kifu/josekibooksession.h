/// @file josekibooksession.h
/// @brief 開いている定跡ファイル（データ・ファイルパス・変更状態・最近使ったファイル・非同期の読み書き）

#ifndef JOSEKIBOOKSESSION_H
#define JOSEKIBOOKSESSION_H

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "josekiioresult.h"

class JosekiRepository;

/**
 * @brief 定跡ウィンドウで開いている定跡ファイルの状態
 *
 * 定跡データ（JosekiRepository）を持ち、ファイルパス・未保存の変更・最近使ったファイル（最大5件）を管理する。
 * 読み込みと保存はワーカースレッドで行い（threading-guidelines: 値オブジェクトだけを渡す）、
 * 完了をシグナルで知らせる。画面（メッセージ・カーソル・ボタン）は扱わない。
 */
class JosekiBookSession : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxRecentFiles = 5;

    explicit JosekiBookSession(QObject *parent = nullptr);
    ~JosekiBookSession() override;

    JosekiRepository *repository() const { return m_repository.get(); }
    const QString &filePath() const { return m_filePath; }
    bool isModified() const { return m_modified; }
    bool isBusy() const { return m_busy; }

    // --- 最近使ったファイル ---
    const QStringList &recentFiles() const { return m_recentFiles; }
    void setRecentFiles(const QStringList &files);
    void addRecentFile(const QString &filePath);
    void removeRecentFile(const QString &filePath);
    void clearRecentFiles();
    /// 次回に自動で読み込むファイル。「履歴をクリア」した後は、開き直すか保存して履歴に戻るまで空
    QString lastFilePathForSettings() const;

    /// 空の定跡にする（ファイル未選択・変更なし）
    void newBook();
    /// filePath に保存できたことを記録する（変更なし・最近使ったファイルの先頭）
    void markSaved(const QString &filePath);

    /// 読み込みを始める（読み込み中・保存中は false）。完了は loaded() / loadFailed()
    bool startLoad(const QString &filePath);
    /// 今のデータの写しを保存し始める（読み込み中・保存中は false）。完了は saved() / saveFailed()
    bool startSave(const QString &filePath);

public slots:
    void setModified(bool modified);

signals:
    void modifiedChanged(bool modified);
    void busyChanged(bool busy);
    void recentFilesChanged();
    /// 読み込んだデータを適用した（ファイルパス・最近使ったファイルも更新済み）
    void loaded(const QString &filePath);
    void loadFailed(const QString &errorMessage);
    /// 保存できた。状態の記録（markSaved）は受け取った側が行う
    void saved(const QString &filePath);
    void saveFailed(const QString &errorMessage);

private slots:
    void onLoadFinished();
    void onSaveFinished();

private:
    void setBusy(bool busy);

    std::unique_ptr<JosekiRepository> m_repository;
    QString m_filePath;
    bool m_modified = false;
    QStringList m_recentFiles;
    bool m_busy = false;
    QFutureWatcher<JosekiLoadResult> m_loadWatcher;
    QFutureWatcher<JosekiSaveResult> m_saveWatcher;
    QString m_pendingLoadFilePath;
    QString m_pendingSaveFilePath;
};

#endif // JOSEKIBOOKSESSION_H
