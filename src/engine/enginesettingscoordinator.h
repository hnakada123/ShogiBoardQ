#ifndef ENGINESETTINGSCOORDINATOR_H
#define ENGINESETTINGSCOORDINATOR_H

/// @file enginesettingscoordinator.h
/// @brief エンジン設定ダイアログ起動コーディネータの定義


class QWidget;

/**
 * @brief エンジン設定ダイアログの表示を担当する名前空間
 *
 */
namespace EngineSettingsCoordinator {

/// エンジン登録ダイアログをモーダルで表示する
void openDialog(QWidget* parent);

} // namespace EngineSettingsCoordinator

#endif // ENGINESETTINGSCOORDINATOR_H
