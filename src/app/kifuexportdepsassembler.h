#ifndef KIFUEXPORTDEPSASSEMBLER_H
#define KIFUEXPORTDEPSASSEMBLER_H

/// @file kifuexportdepsassembler.h
/// @brief KifuExportController の依存収集ロジックを担うアセンブラ

class MainWindow;

/**
 * @brief KifuExportController の依存収集・設定を担うアセンブラ
 *
 * updateKifuExportDependencies の依存収集ロジックを MainWindow から分離し、
 * MainWindow 側は1行の委譲呼び出しのみにする。
 *
 * MainWindow の private メンバへアクセスするため friend 宣言が必要。
 */
class KifuExportDepsAssembler
{
public:
    /// KifuExportController に最新の依存オブジェクトを設定する
    static void assemble(MainWindow& mw);
};

#endif // KIFUEXPORTDEPSASSEMBLER_H
