/// @file enginesettingscoordinator.cpp
/// @brief エンジン設定ダイアログ起動コーディネータの実装

#include "enginesettingscoordinator.h"
#include "engineregistrationdialog.h"

namespace EngineSettingsCoordinator {

void openDialog(QWidget* parent)
{
    EngineRegistrationDialog dlg(parent);
    dlg.exec();
}

} // namespace EngineSettingsCoordinator
