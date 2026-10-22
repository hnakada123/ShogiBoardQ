/// @file josekibooksession.h
/// @brief 開いている定跡ファイル（データ・ファイルパス・変更状態・最近使ったファイル・読み書き）

#ifndef JOSEKIBOOKSESSION_H
#define JOSEKIBOOKSESSION_H

#include <QFutureWatcher>
#include <QList>
#include <QObject>

#include "recentfilelist.h"
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

#include "josekibook.h"
#include "josekitypes.h"

/**
 * @brief 定跡ウィンドウで開いている定跡ファイルの状態
 *
 * 定跡データ（値型の JosekiBook）を持ち、ファイルパス・未保存の変更・最近使ったファイル（最大5件）を管理する。
 * 定跡データの読み取り・変更・保存はすべてここを通す（データは外に見せない）。局面は手数付きの SFEN の
 * ままで渡してよい（定跡の局面のキーにするのはここ）。
 *
 * 保存できたら「変更なし」にし、ファイルパスと履歴を更新する。保存の経路は save（同期）・
 * startSave（非同期）・startRegisterMergeMoves（登録して非同期で保存）だけ。
 * 読み込みと非同期の保存はワーカースレッドで行い（threading-guidelines: 値オブジェクトだけを渡す）、
 * 完了をシグナルで知らせる。画面（メッセージ・カーソル・ボタン）は扱わない。
 */
class JosekiBookSession : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxRecentFiles = 5;

    /// 読み書きの最中の処理（読み込み中・保存中は定跡を変えられない）
    enum class Activity { Idle, Loading, Saving };

    explicit JosekiBookSession(QObject *parent = nullptr);
    ~JosekiBookSession() override;

    const QString &filePath() const { return m_filePath; }
    bool isModified() const { return m_modified; }
    bool isBusy() const { return m_activity != Activity::Idle; }
    Activity activity() const { return m_activity; }

    // --- 定跡データの読み取り ---

    /// 定跡の局面のキー（手数を除いた SFEN）
    static QString positionKey(const QString &sfen);
    bool isEmpty() const { return m_book.isEmpty(); }
    int positionCount() const { return m_book.positionCount(); }
    /// 局面の定跡手（無ければ空）
    QList<JosekiMove> movesAt(const QString &sfen) const { return m_book.movesAt(sfen); }
    /// 局面の登録の手数付きの SFEN（手数違いの登録があれば sfen と同じ手数のもの、無ければ最初のもの。無ければ空）
    QString registeredSfen(const QString &sfen) const;
    /// 局面を登録した手数（ファイルの順。手数違いで別々に登録していれば複数。無ければ空）
    QList<int> registeredPlies(const QString &sfen) const { return m_book.registeredPlies(sfen); }
    bool hasPosition(const QString &sfen) const { return m_book.find(sfen) != nullptr; }
    bool hasMove(const QString &sfen, const QString &usiMove) const { return m_book.hasMove(sfen, usiMove); }
    /// 棋譜からマージで登録した指し手（局面のキーと指し手の組）
    const QSet<JosekiMoveKey> &mergeRegisteredMoves() const { return m_book.mergeRegistered(); }

    // --- 定跡データの変更（変更ありにする） ---

    /// 定跡手を足す。同じ指し手があれば置き換える
    void addMove(const QString &sfen, const JosekiMove &move);
    /// move.move の定跡手の値・コメント・予想応手を書き換える。無ければ false
    bool updateMove(const QString &sfen, const JosekiMove &move);
    /// 定跡手を消す。無ければ false
    bool removeMove(const QString &sfen, const QString &usiMove);

    // --- 最近使ったファイル ---

    const QStringList &recentFiles() const { return m_recentFiles.files(); }
    void setRecentFiles(const QStringList &files);
    void addRecentFile(const QString &filePath);
    void removeRecentFile(const QString &filePath);
    void clearRecentFiles();
    /// 次回に自動で読み込むファイル。「履歴をクリア」した後は、開き直すか保存して履歴に戻るまで空
    QString lastFilePathForSettings() const;

    // --- ファイル ---

    /// 空の定跡にする（ファイル未選択・変更なし）
    void newBook();
    /// filePath にすぐ保存する（読み込み中・保存中は失敗）
    bool save(const QString &filePath, QString *errorMessage = nullptr);
    /// 読み込みを始める（読み込み中・保存中は false）。完了は loaded() / loadFailed()
    bool startLoad(const QString &filePath);
    /// 今のデータの写しを保存し始める（読み込み中・保存中は false）。完了は saved() / saveFailed()
    bool startSave(const QString &filePath);
    /**
     * @brief 棋譜の指し手を定跡に登録し、今の定跡ファイルに保存し始める（まだ登録していない手だけ）
     *
     * 大きな定跡でも画面を止めないように、保存はワーカースレッドで行う。保存できたときだけ登録を定跡に入れて
     * 登録済みの印を付け、できなければ何も変えない（失敗は saveFailed() でも知らせる）。どちらも mergeFinished()
     * で知らせる。「全て登録」でも保存は1回。読み込み中・保存中・保存先がなければ始めずに false
     */
    bool startRegisterMergeMoves(const QList<KifuMergeEntry> &entries);

public slots:
    void setModified(bool modified);

signals:
    void modifiedChanged(bool modified);
    /// 開いているファイルが替わった（新規・読み込み・別の名前で保存）
    void filePathChanged();
    void busyChanged(bool busy);
    void recentFilesChanged();
    /// 読み込んだデータを適用した（ファイルパス・最近使ったファイルも更新済み）
    void loaded(const QString &filePath);
    void loadFailed(const QString &errorMessage);
    /// 非同期の保存ができた（変更なし・ファイルパス・履歴は更新済み）
    void saved(const QString &filePath);
    void saveFailed(const QString &errorMessage);
    /// startRegisterMergeMoves() の保存が終わった（registered: 登録して保存できた）
    void mergeFinished(bool registered);

private slots:
    void onLoadFinished();
    void onSaveFinished();

private:
    /// 読み書きの最中の処理を変える（読み書き中かが変われば busyChanged を出す）
    void setActivity(Activity activity);
    /// filePath に保存できたことを記録する（変更なし・最近使ったファイルの先頭）
    void markSaved(const QString &filePath);

    JosekiBook m_book;
    QString m_filePath;
    bool m_modified = false;
    RecentFileList m_recentFiles{kMaxRecentFiles};
    Activity m_activity = Activity::Idle;
    QFutureWatcher<JosekiLoadResult> m_loadWatcher;
    QFutureWatcher<JosekiSaveResult> m_saveWatcher;
    QString m_pendingLoadFilePath;
    QString m_pendingSaveFilePath;
    /// 保存中のマージの結果（保存できたら定跡にする）
    std::optional<JosekiBook> m_pendingMerge;
};

#endif // JOSEKIBOOKSESSION_H
