#ifndef ENGINELISTSETTINGS_H
#define ENGINELISTSETTINGS_H

/// @file enginelistsettings.h
/// @brief 機能ごとに選んだエンジンの保存（登録エンジンの一覧と検索は RegisteredEngines）

#include <QString>

#include <optional>

#include "registeredengines.h"

namespace EngineListSettings {

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
int restoreSelection(const SelectionKeys& keys, const RegisteredEngines& engines);

/// 選んだエンジンの名前を保存する（以前の版の番号・パスは消す）
void saveSelection(const SelectionKeys& keys, const QString& engineName);

} // namespace EngineListSettings

#endif // ENGINELISTSETTINGS_H
