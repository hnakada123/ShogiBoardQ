#ifndef RECENTFILELIST_H
#define RECENTFILELIST_H

/// @file recentfilelist.h
/// @brief 最近使ったファイルの一覧（新しい順・件数の上限つき）

#include <QStringList>

/**
 * @brief 最近使ったファイルの一覧
 *
 * 新しく使ったファイルを先頭に置き、同じファイルは1つにまとめ、上限を超えた古いものは捨てる。
 * 保存とメニューは持たない（メニューは widgets の RecentFilesMenu が作る）。
 */
class RecentFileList
{
public:
    static constexpr int kDefaultLimit = 5;

    explicit RecentFileList(int limit = kDefaultLimit);

    const QStringList& files() const { return m_files; }
    bool isEmpty() const { return m_files.isEmpty(); }

    /// 保存していた一覧に置き換える（上限を超える分は捨てる）
    void setFiles(const QStringList& files);
    /// filePath を先頭に置く
    void add(const QString& filePath);
    /// filePath を除く。一覧が変わったら true
    bool remove(const QString& filePath);
    void clear();

private:
    int m_limit;
    QStringList m_files;
};

#endif // RECENTFILELIST_H
