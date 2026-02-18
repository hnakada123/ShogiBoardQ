#include "dialogorchestrator.h"

#include <QObject>

#include "dialogcoordinator.h"
#include "considerationwiring.h"
#include "uistatepolicymanager.h"

void DialogOrchestrator::wireConsiderationSignals(DialogCoordinator* dialog,
                                                  ConsiderationWiring* considerationWiring)
{
    if (!dialog || !considerationWiring) {
        return;
    }

    QObject::connect(dialog, &DialogCoordinator::considerationModeStarted,
                     considerationWiring, &ConsiderationWiring::onModeStarted,
                     Qt::UniqueConnection);
    QObject::connect(dialog, &DialogCoordinator::considerationTimeSettingsReady,
                     considerationWiring, &ConsiderationWiring::onTimeSettingsReady,
                     Qt::UniqueConnection);
    QObject::connect(dialog, &DialogCoordinator::considerationMultiPVReady,
                     considerationWiring, &ConsiderationWiring::onDialogMultiPVReady,
                     Qt::UniqueConnection);
}

void DialogOrchestrator::wireUiStateSignals(DialogCoordinator* dialog,
                                            UiStatePolicyManager* uiStatePolicy)
{
    if (!dialog || !uiStatePolicy) {
        return;
    }

    QObject::connect(dialog, &DialogCoordinator::analysisModeStarted,
                     uiStatePolicy, &UiStatePolicyManager::transitionToDuringAnalysis,
                     Qt::UniqueConnection);
    QObject::connect(dialog, &DialogCoordinator::analysisModeEnded,
                     uiStatePolicy, &UiStatePolicyManager::transitionToIdle,
                     Qt::UniqueConnection);
    QObject::connect(dialog, &DialogCoordinator::tsumeSearchModeStarted,
                     uiStatePolicy, &UiStatePolicyManager::transitionToDuringTsumeSearch,
                     Qt::UniqueConnection);
    QObject::connect(dialog, &DialogCoordinator::tsumeSearchModeEnded,
                     uiStatePolicy, &UiStatePolicyManager::transitionToIdle,
                     Qt::UniqueConnection);
    QObject::connect(dialog, &DialogCoordinator::considerationModeStarted,
                     uiStatePolicy, &UiStatePolicyManager::transitionToDuringConsideration,
                     Qt::UniqueConnection);
}
