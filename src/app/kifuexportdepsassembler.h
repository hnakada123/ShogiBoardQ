#ifndef KIFUEXPORTDEPSASSEMBLER_H
#define KIFUEXPORTDEPSASSEMBLER_H

/// @file kifuexportdepsassembler.h
/// @brief KifuExportController の依存収集ロジックを担うアセンブラ

struct MainWindowRuntimeRefs;
class KifuExportController;

/**
 * @brief KifuExportController の依存収集・設定を担うアセンブラ
 *
 * updateKifuExportDependencies の依存収集ロジックを MainWindow から分離し、
 * MainWindow 側は1行の委譲呼び出しのみにする。
 *
 * RuntimeRefs 経由でアクセスするため friend 宣言は不要。
 */
class KifuExportDepsAssembler
{
public:
    /// KifuExportController に最新の依存オブジェクトを設定する
    static void assemble(KifuExportController* controller, const MainWindowRuntimeRefs& refs);
};

#endif // KIFUEXPORTDEPSASSEMBLER_H
