#ifndef JOSEKIBOOKCONTROLLER_H
#define JOSEKIBOOKCONTROLLER_H

/// @file josekibookcontroller.h
/// @brief 定跡ウィンドウからの定跡の操作（ファイル・定跡手・マージ・ドロップ）と、閉じるときの確認

#include <QObject>
#include <QString>
#include <QStringList>

#include "josekiwindow.h"

class JosekiBookSession;
class JosekiEditController;
class JosekiFileController;
class JosekiMergeController;
class KifuNavigationState;
class QDockWidget;

/**
 * @brief 定跡ウィンドウの意図を受けて、開いている定跡を操作する
 *
 * 定跡ウィンドウ（JosekiWindow）は操作をシグナルで知らせるだけで、ダイアログを出して処理するのはここ。
 * - ファイル（開く・新規・保存・履歴）は JosekiFileController、定跡手の追加・編集・削除は JosekiEditController、
 *   マージは JosekiMergeController（3つともここが持つ。ダイアログの親は定跡ウィンドウ）
 * - 「現在の棋譜から」は表示中の手順（KifuNavigationState）を渡す
 * - ドロップは、定跡ファイル（.db）1つなら開き、それ以外は複数の棋譜から登録するダイアログに並べる
 * - ドックを閉じるとき・アプリを閉じるときに、未保存の変更を確かめる。ドックの題名に未保存の印を付ける
 */
class JosekiBookController : public QObject
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_josekiwindow.cpp がファイル・マージの操作を直接呼ぶ
    friend class TestJosekiWindow;
#endif

public:
    struct Deps {
        JosekiBookSession *session = nullptr;           ///< 開いている定跡（必須・非所有）
        JosekiWindow *window = nullptr;                 ///< 定跡ウィンドウ（必須・非所有。ダイアログの親にもする）
        const KifuNavigationState *navState = nullptr;  ///< 表示中の手順（「現在の棋譜から」。必須・非所有）
    };

    explicit JosekiBookController(const Deps &deps, QObject *parent = nullptr);
    ~JosekiBookController() override;

    /// 未保存の変更があれば保存・破棄・取りやめを聞く（アプリを閉じるとき）。続けてよければ true
    [[nodiscard]] bool confirmClose();
    /// 定跡ウィンドウを入れたドックを登録する（閉じるときの確認と、題名の未保存の印）
    void attachDock(QDockWidget *dock);

protected:
    /// ドックを閉じるときに未保存の変更を確かめる
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onMergeRequested(JosekiWindow::MergeSource source);
    /// ドロップされたファイル・フォルダを開く（ドロップの処理の後で呼ぶ）
    void openDroppedPaths(const QStringList &paths);
    void onModifiedChanged(bool modified);

private:
    Deps m_deps;
    JosekiFileController *m_files = nullptr;
    JosekiMergeController *m_merge = nullptr;
    JosekiEditController *m_edit = nullptr;
    QDockWidget *m_dock = nullptr;
    /// ドックの題名（未保存の印を付ける前）
    QString m_dockTitle;
};

#endif // JOSEKIBOOKCONTROLLER_H
