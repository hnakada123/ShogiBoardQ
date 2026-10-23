#ifndef REGISTEREDENGINES_H
#define REGISTEREDENGINES_H

/// @file registeredengines.h
/// @brief 登録エンジンの一覧と、名前・実行ファイルでの検索

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

/// 登録エンジン1つ（名前はエンジンの id name で、登録時に重複しない）
struct EngineEntry {
    QString name;
    QString path;
    QString author;
};

/**
 * @brief 登録エンジンの一覧（登録の順）と、名前・実行ファイルでの検索
 *
 * 保存形式は EngineProfileStore だけが知る（読むのは load()、書くのは EngineProfileStore::saveEngines）。
 * 検索はここだけで行う（GUI・CLI・自動化 API で同じ）。
 */
class RegisteredEngines
{
public:
    RegisteredEngines() = default;
    explicit RegisteredEngines(QList<EngineEntry> entries) : m_entries(std::move(entries)) {}

    /// 設定ファイルに登録された一覧
    static RegisteredEngines load();

    const QList<EngineEntry>& entries() const { return m_entries; }
    bool isEmpty() const { return m_entries.isEmpty(); }
    /// 登録の順の名前
    QStringList names() const;

    /// 名前で探した位置（見つからなければ -1）
    int indexOfName(const QString& name) const;
    /// 実行ファイルのパス（正規化して比べる）で探した位置（見つからなければ -1）
    int indexOfPath(const QString& path) const;
    std::optional<EngineEntry> findByName(const QString& name) const;
    std::optional<EngineEntry> findByPath(const QString& path) const;

private:
    QList<EngineEntry> m_entries;
};

#endif // REGISTEREDENGINES_H
