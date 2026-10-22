#ifndef JOSEKIMERGECONTROLLER_H
#define JOSEKIMERGECONTROLLER_H

/// @file josekimergecontroller.h
/// @brief 定跡ウィンドウの「マージ」（棋譜の指し手を定跡に登録するダイアログを開き、登録を保存する）

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "josekibook.h"
#include "josekitypes.h"

class JosekiBookSession;
class JosekiFileController;
class QWidget;

/**
 * @brief 棋譜からのマージの利用者操作
 *
 * 「現在の棋譜から」「棋譜ファイルから」でマージのダイアログを開き、ダイアログの「登録」「全て登録」を
 * JosekiBookSession の非同期の保存につなぐ。保存が終わったら、開いているダイアログに登録済みの印を配る。
 * 読み書き中はダイアログを使えなくし、開いている定跡を替えるときはダイアログを閉じる。
 * 「複数の棋譜から」（定跡ウィンドウへのドロップも）は JosekiImportDialog で、「定跡ファイルから」は
 * JosekiBookMergeDialog で条件を選び、どちらも定跡を変えるだけで保存しない。
 */
class JosekiMergeController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        JosekiBookSession *session = nullptr;   ///< 開いている定跡（必須）
        JosekiFileController *files = nullptr;  ///< マージの前に保存先を決める（必須）
        QWidget *parentWidget = nullptr;        ///< ダイアログとメッセージの親（必須）
    };

    explicit JosekiMergeController(const Deps &deps, QObject *parent = nullptr);

    /// 表示中の手順を受け取り、マージのダイアログを開く（kifuDataRequested への応答）
    void setKifuData(const QStringList &sfenList, const QStringList &moveList,
                     const QStringList &japaneseMoveList, int currentPly);
    /// ダイアログの「登録」「全て登録」: 登録して定跡ファイルへ1回だけ保存し始める（始めたら true）
    bool registerEntries(const QList<KifuMergeEntry> &entries);
    /// paths（棋譜ファイル・フォルダ）を並べて、複数の棋譜から登録するダイアログを開く（定跡ウィンドウへのドロップ）
    void importPaths(const QStringList &paths);

public slots:
    /// 「マージ ▼」→「現在の棋譜から」
    void mergeFromCurrentKifu();
    /// 「マージ ▼」→「棋譜ファイルから」
    void mergeFromKifuFile();
    /// 「マージ ▼」→「複数の棋譜から...」
    void mergeFromKifuFiles();
    /// 「マージ ▼」→「定跡ファイルから...」
    void mergeFromBookFile();
    /// 開いている定跡を替える直前（古いダイアログからは登録させない）
    void closeDialogs();

signals:
    /// 表示中の手順を求める（応答は setKifuData）
    void kifuDataRequested();

private slots:
    /// 読み書き中はダイアログを使えなくする
    void onBusyChanged(bool busy);
    /// 登録の保存が終わった（ダイアログに登録済みの印を配る）
    void onMergeFinished(bool registered);
    /// 「定跡ファイルから」で足し終えた・足せなかった
    void onBookMerged(const QString &filePath, const JosekiBook::MergeStats &stats);
    void onBookMergeFailed(const QString &errorMessage);

private:
    void openDialog(const QList<KifuMergeEntry> &entries, const QString &title);

    Deps m_deps;
};

#endif // JOSEKIMERGECONTROLLER_H
