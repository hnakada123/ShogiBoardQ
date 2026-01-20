#include "promotionflow.h"
#include "promotedialog.h"
#include <QDialog>

bool PromotionFlow::askPromote(QWidget* parent)
{
    PromoteDialog dlg(parent);
    return dlg.exec() == QDialog::Accepted; // 成る=OK
}
