#ifndef ENGINELISTSETTINGS_H
#define ENGINELISTSETTINGS_H

/// @file enginelistsettings.h
/// @brief 登録エンジンの一覧の検索と、機能ごとに選んだエンジンの保存

#include <QList>
#include <QString>

#include <optional>

namespace EngineListSettings {

struct EngineEntry {
    QString name;
    QString path;
    QString author;
};

/// 登録済みエンジンの一覧（保存形式は EngineProfileStore が知る）
QList<EngineEntry> loadEngines();

/// 名前（エンジンの id name）で探した位置。見つからなければ -1
int indexOfName(const QList<EngineEntry>& engines, const QString& name);

/// 実行ファイルのパス（正規化して比べる）で探した位置。見つからなければ -1
int indexOfPath(const QList<EngineEntry>& engines, const QString& path);

/// 登録済みエンジンの名前から実行ファイルのパス（未登録なら空）
QString pathForName(const QString& name);

/// 実行ファイルのパスから登録済みエンジンの名前（未登録なら空）
QString nameForPath(const QString& path);

/**
 * @brief 機能ごとに選んだエンジンを保存する場所
 *
 * エンジンは名前（id name、登録時に重複しない）で保存する。以前の版は番号やパスで保存していたので、
 * 名前が無いときはそれを読んで同じエンジンを選ぶ。
 */
struct SelectionKeys {
    const char* name = nullptr;         ///< 名前を保存するキー
    const char* legacyIndex = nullptr;  ///< 以前の版の番号のキー（無ければ nullptr）
    const char* legacyPath = nullptr;   ///< 以前の版のパスのキー（無ければ nullptr）
};

/// 機能ごとの保存場所（検討・棋譜解析・詰み探索・詰将棋生成・通信対局・詰将棋問題一覧の判定）
SelectionKeys considerationSelection();
SelectionKeys kifuAnalysisSelection();
SelectionKeys tsumeSearchSelection();
SelectionKeys tsumeshogiGeneratorSelection();
SelectionKeys csaGameSelection();
SelectionKeys tsumeCollectionSelection();

/// 保存した名前（保存していなければ std::nullopt。空の文字列は登録エンジン以外を選んだ印）
std::optional<QString> savedSelectionName(const SelectionKeys& keys);

/// 保存した選択を一覧の位置にする（名前を優先し、無ければ以前の番号・パス）。見つからなければ -1
int restoreSelection(const SelectionKeys& keys, const QList<EngineEntry>& engines);

/// 選んだエンジンの名前を保存する（以前の版の番号・パスは消す）
void saveSelection(const SelectionKeys& keys, const QString& engineName);

} // namespace EngineListSettings

#endif // ENGINELISTSETTINGS_H
