#ifndef TSUMECOLLECTIONSESSION_H
#define TSUMECOLLECTIONSESSION_H

/// @file tsumecollectionsession.h
/// @brief 開いている詰将棋の局面集（問題・挑戦状況・絞り込み・手数の判定）

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>
#include <optional>

#include "tsumecollection.h"
#include "tsumeevaluation.h"
#include "tsumeprogressstore.h"

class TsumePositionAnalyzer;

/**
 * @brief 詰将棋問題一覧で開いている局面集
 *
 * 局面集の問題と局面の識別子、挑戦・正答の履歴（TsumeProgressStore）、手数の判定結果を持つ。
 * 手数の判定は頼まれた問題を待ち行列に積み、1問ずつ TsumePositionAnalyzer で判定する
 * （判定済みの結果は履歴の DB のキャッシュから使う）。画面（カード・ページの部品）は扱わない。
 */
class TsumeCollectionSession : public QObject
{
    Q_OBJECT

public:
    /// 挑戦状況での絞り込み（問題一覧のコンボの並び）
    enum class Filter { All, Unattempted, Unsolved, Solved };

    /// 表示するページの範囲（絞り込んだ問題の並びの [begin, end)）
    struct Page {
        int count = 0;   ///< 絞り込んだ問題の数
        int pages = 1;   ///< ページ数（問題がなくても1）
        int begin = 0;
        int end = 0;
    };

    explicit TsumeCollectionSession(QObject* parent = nullptr);
    ~TsumeCollectionSession() override;

    // --- 局面集 ---

    /// 局面集のファイルを読む。読めなければ errorMessage に理由（形式が不正な行は invalidLines）
    bool load(const QString& path, QString* errorMessage, QList<int>* invalidLines);
    const QString& filePath() const { return m_file; }
    int problemCount() const { return static_cast<int>(m_problems.size()); }
    const TsumeProblem& problem(int index) const { return m_problems.at(index); }
    /// 挑戦・正答の履歴（詰将棋対局でも使う）
    TsumeProgressStore* store() const { return m_store.get(); }

    // --- 挑戦状況 ---

    TsumeProgressStore::Progress progress(int index) const;
    /// 履歴を読み直す（対局から戻ったとき）
    void refreshProgress();
    /// 履歴を消す（この局面集の問題だけか、すべての局面集）
    bool resetProgress(bool allCollections);
    /// 絞り込んだ問題の添字（局面集の並び）
    QList<int> filtered(Filter filter) const;
    /// count 問を pageSize 問ずつに分けたときの pageNumber ページ目（1 始まり）
    static Page page(int count, int pageSize, int pageNumber);

    // --- 手数の判定 ---

    /// 判定に使うエンジン（空なら内蔵の判定）。判定結果は消す
    void setEngine(const QString& enginePath);
    bool usesEngine() const { return !m_enginePath.isEmpty(); }
    void setTimeoutMs(int milliseconds) { m_timeoutMs = milliseconds; }
    /// 結果のない問題を判定する（判定済みのキャッシュがあれば使う）
    void evaluate(const QList<int>& indexes);
    /// キャッシュを消して判定し直す
    void reevaluate(const QList<int>& indexes);
    /// 待ち行列と判定中の判定をやめる
    void cancelEvaluation();
    /// 止めている間は判定しない（詰将棋対局の間）
    void setPaused(bool paused);
    bool isPending(int index) const { return m_pending == index; }
    std::optional<TsumeEvaluation> result(int index) const;
    /// 結果を忘れる（対局で判定し直したかもしれない問題）
    void forgetResults(const QList<int>& indexes);

signals:
    /// 判定中の問題か判定の結果が変わった
    void evaluationChanged();

private slots:
    void evaluateNext();
    void onEvaluated(const TsumeEvaluation& result);

private:
    std::unique_ptr<TsumeProgressStore> m_store;
    TsumePositionAnalyzer* m_analyzer = nullptr;  ///< QObject parent 所有
    QString m_file;
    QList<TsumeProblem> m_problems;
    QStringList m_ids;  ///< m_problems と同じ並びの局面の識別子
    QHash<QString, TsumeProgressStore::Progress> m_progress;
    QHash<QString, TsumeEvaluation> m_results;  ///< 局面の識別子ごとの判定結果
    QList<int> m_queue;
    QTimer m_evaluationTimer;
    QString m_enginePath;
    int m_timeoutMs = 10000;
    int m_pending = -1;
    bool m_paused = false;
};

#endif // TSUMECOLLECTIONSESSION_H
