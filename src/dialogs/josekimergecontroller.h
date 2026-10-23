#ifndef JOSEKIMERGECONTROLLER_H
#define JOSEKIMERGECONTROLLER_H

/// @file josekimergecontroller.h
/// @brief 定跡ウィンドウの「マージ」（棋譜・別の定跡ファイルから定跡に登録するダイアログを開く）

#include <QObject>
#include <QString>
#include <QStringList>

#include "josekibook.h"
#include "recordline.h"

class JosekiBookSession;
class QWidget;

/**
 * @brief 棋譜・定跡ファイルからのマージの利用者操作
 *
 * 「現在の棋譜から」「棋譜ファイルから」は手順の手を選んで登録するダイアログ（JosekiMergeDialog）を、
 * 「複数の棋譜から」（定跡ウィンドウへのドロップも）は JosekiImportDialog を、「定跡ファイルから」は
 * JosekiBookMergeDialog を開く。どれも定跡を変えるだけで保存しない（定跡ウィンドウの「保存」で保存する）。
 * 定跡を変えられないとき（読み書き中・前回のファイルの読み込み待ち）は開かない。
 */
class JosekiMergeController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        JosekiBookSession *session = nullptr;   ///< 開いている定跡（必須）
        QWidget *parentWidget = nullptr;        ///< ダイアログとメッセージの親（必須）
    };

    explicit JosekiMergeController(const Deps &deps, QObject *parent = nullptr);

    /// 表示中の手順の手を選んで登録するダイアログを開く（kifuDataRequested への応答）
    void openForLine(const RecordLine &line);
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

signals:
    /// 表示中の手順を求める（応答は openForLine）
    void kifuDataRequested();

private slots:
    /// 「定跡ファイルから」で足し終えた・足せなかった
    void onBookMerged(const QString &filePath, const JosekiBook::MergeStats &stats);
    void onBookMergeFailed(const QString &errorMessage);

private:
    void openDialog(const RecordLine &line, const QString &title);

    Deps m_deps;
};

#endif // JOSEKIMERGECONTROLLER_H
