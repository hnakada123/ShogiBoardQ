#ifndef DIALOGORCHESTRATOR_H
#define DIALOGORCHESTRATOR_H

class DialogCoordinator;
class ConsiderationWiring;
class UiStatePolicyManager;

class DialogOrchestrator
{
public:
    static void wireConsiderationSignals(DialogCoordinator* dialog,
                                         ConsiderationWiring* considerationWiring);
    static void wireUiStateSignals(DialogCoordinator* dialog,
                                   UiStatePolicyManager* uiStatePolicy);
};

#endif // DIALOGORCHESTRATOR_H
