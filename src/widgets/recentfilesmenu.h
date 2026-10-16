#ifndef RECENTFILESMENU_H
#define RECENTFILESMENU_H

/// @file recentfilesmenu.h
/// @brief 「履歴」ボタンのメニュー（最近使ったファイルと「履歴をクリア」）

#include <QObject>
#include <QStringList>

class QMenu;

/**
 * @brief 最近使ったファイルのメニューを作る補助
 *
 * 渡されたメニューの中身を、ファイルの一覧（ファイル名を表示し、ツールチップに場所）・区切り・
 * 「履歴をクリア」で作り直す。一覧が空なら「（履歴なし）」だけを置く。選ばれた項目はシグナルで知らせる。
 * メニューの子として作られ、メニューと一緒に消える。
 */
class RecentFilesMenu : public QObject
{
    Q_OBJECT

public:
    explicit RecentFilesMenu(QMenu* menu);

    /// メニューを files で作り直す
    void setFiles(const QStringList& files);

signals:
    /// ファイルの項目が選ばれた
    void fileTriggered(const QString& filePath);
    /// 「履歴をクリア」が選ばれた
    void clearTriggered();

private slots:
    void onFileActionTriggered();

private:
    QMenu* m_menu;
};

#endif // RECENTFILESMENU_H
