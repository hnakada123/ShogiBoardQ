#ifndef RECENTFILELIST_H
#define RECENTFILELIST_H

/// @file recentfilelist.h
/// @brief 最近使ったものの一覧（新しい順・重複なし・件数の上限つき）

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
 * @brief 最近使ったものの一覧
 *
 * 新しく使ったものを先頭に置き、同じものは1つにまとめ、上限を超えた古いものは捨てる。
 * 既定値（空の文字列など）は足さない。保存とメニューは持たない。
 * ファイル（RecentFileList）と通信対局の接続先の履歴で使う。
 */
template <typename T>
class RecentList
{
public:
    explicit RecentList(int limit) : m_limit(qMax(1, limit)) {}

    const QList<T>& items() const { return m_items; }
    bool isEmpty() const { return m_items.isEmpty(); }

    /// 保存していた一覧に置き換える（重複・既定値は除き、上限を超える分は捨てる）
    void setItems(const QList<T>& items)
    {
        m_items.clear();
        for (const T& item : items) {
            if (item == T{} || m_items.contains(item)) continue;
            m_items.append(item);
            if (m_items.size() >= m_limit) break;
        }
    }
    /// item を先頭に置く
    void add(const T& item)
    {
        if (item == T{}) return;
        m_items.removeAll(item);
        m_items.prepend(item);
        while (m_items.size() > m_limit) m_items.removeLast();
    }
    /// item を除く。一覧が変わったら true
    bool remove(const T& item) { return m_items.removeAll(item) > 0; }
    void clear() { m_items.clear(); }

private:
    int m_limit;
    QList<T> m_items;
};

/// 最近使ったファイルの一覧（定跡の JosekiBookSession と局面集ビューア。メニューは widgets の RecentFilesMenu）
class RecentFileList : public RecentList<QString>
{
public:
    static constexpr int kDefaultLimit = 5;

    explicit RecentFileList(int limit = kDefaultLimit) : RecentList<QString>(limit) {}

    const QStringList& files() const { return items(); }
    void setFiles(const QStringList& files) { setItems(files); }
};

#endif // RECENTFILELIST_H
