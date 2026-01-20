#include "enginesettingscoordinator.h"
#include "engineregistrationdialog.h"

namespace EngineSettingsCoordinator {

void openDialog(QWidget* parent)
{
    EngineRegistrationDialog dlg(parent);
    dlg.exec();
}

} // namespace EngineSettingsCoordinator
