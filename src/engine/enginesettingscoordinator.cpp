/// @file enginesettingscoordinator.cpp
/// @brief エンジン設定ダイアログ起動コーディネータの実装
/// @todo remove コメントスタイルガイド適用済み

#include "enginesettingscoordinator.h"
#include "engineregistrationdialog.h"

namespace EngineSettingsCoordinator {

/// @todo remove コメントスタイルガイド適用済み
void openDialog(QWidget* parent)
{
    EngineRegistrationDialog dlg(parent);
    dlg.exec();
}

} // namespace EngineSettingsCoordinator
