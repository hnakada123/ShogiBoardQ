#ifndef ENGINEPONDERSETTINGS_H
#define ENGINEPONDERSETTINGS_H

/// @file enginepondersettings.h
/// @brief エンジンごとのGUI先読み設定とUSIオプション通知方針

#include <QString>

namespace EnginePonderSettings {

struct Preferences {
    bool enabled = false;                 ///< GUIが先読みを開始するか
    bool sendUnreportedOption = true;      ///< 未報告でも予約オプションUSI_Ponderを通知するか
    bool defaultEnabled = false;          ///< エンジンが報告した既定値（未報告ならfalse）
};

/// GUI設定が未保存なら、従来のUSI_Ponderの保存値を引き継ぐ。
Preferences load(const QString& engineName);

/// GUI設定を保存し、既存のUSI_Ponderの保存値も同期する。
void save(const QString& engineName, bool enabled, bool sendUnreportedOption);

} // namespace EnginePonderSettings

#endif // ENGINEPONDERSETTINGS_H
