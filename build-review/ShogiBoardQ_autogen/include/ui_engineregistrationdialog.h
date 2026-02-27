/********************************************************************************
** Form generated from reading UI file 'engineregistrationdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ENGINEREGISTRATIONDIALOG_H
#define UI_ENGINEREGISTRATIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_EngineRegistrationDialog
{
public:
    QVBoxLayout *mainLayout;
    QListWidget *engineListWidget;
    QHBoxLayout *bottomLayout;
    QPushButton *fontDecreaseButton;
    QPushButton *fontIncreaseButton;
    QSpacerItem *horizontalSpacer;
    QPushButton *addEngineButton;
    QPushButton *removeEngineButton;
    QPushButton *configureEngineButton;
    QPushButton *closeButton;

    void setupUi(QDialog *EngineRegistrationDialog)
    {
        if (EngineRegistrationDialog->objectName().isEmpty())
            EngineRegistrationDialog->setObjectName("EngineRegistrationDialog");
        EngineRegistrationDialog->resize(647, 421);
        mainLayout = new QVBoxLayout(EngineRegistrationDialog);
        mainLayout->setObjectName("mainLayout");
        engineListWidget = new QListWidget(EngineRegistrationDialog);
        engineListWidget->setObjectName("engineListWidget");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(1);
        sizePolicy.setHeightForWidth(engineListWidget->sizePolicy().hasHeightForWidth());
        engineListWidget->setSizePolicy(sizePolicy);

        mainLayout->addWidget(engineListWidget);

        bottomLayout = new QHBoxLayout();
        bottomLayout->setObjectName("bottomLayout");
        fontDecreaseButton = new QPushButton(EngineRegistrationDialog);
        fontDecreaseButton->setObjectName("fontDecreaseButton");
        fontDecreaseButton->setMinimumSize(QSize(60, 0));

        bottomLayout->addWidget(fontDecreaseButton);

        fontIncreaseButton = new QPushButton(EngineRegistrationDialog);
        fontIncreaseButton->setObjectName("fontIncreaseButton");
        fontIncreaseButton->setMinimumSize(QSize(60, 0));

        bottomLayout->addWidget(fontIncreaseButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        bottomLayout->addItem(horizontalSpacer);

        addEngineButton = new QPushButton(EngineRegistrationDialog);
        addEngineButton->setObjectName("addEngineButton");

        bottomLayout->addWidget(addEngineButton);

        removeEngineButton = new QPushButton(EngineRegistrationDialog);
        removeEngineButton->setObjectName("removeEngineButton");

        bottomLayout->addWidget(removeEngineButton);

        configureEngineButton = new QPushButton(EngineRegistrationDialog);
        configureEngineButton->setObjectName("configureEngineButton");

        bottomLayout->addWidget(configureEngineButton);

        closeButton = new QPushButton(EngineRegistrationDialog);
        closeButton->setObjectName("closeButton");

        bottomLayout->addWidget(closeButton);


        mainLayout->addLayout(bottomLayout);


        retranslateUi(EngineRegistrationDialog);

        QMetaObject::connectSlotsByName(EngineRegistrationDialog);
    } // setupUi

    void retranslateUi(QDialog *EngineRegistrationDialog)
    {
        EngineRegistrationDialog->setWindowTitle(QCoreApplication::translate("EngineRegistrationDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\347\231\273\351\214\262", nullptr));
        fontDecreaseButton->setText(QCoreApplication::translate("EngineRegistrationDialog", "A-", nullptr));
        fontIncreaseButton->setText(QCoreApplication::translate("EngineRegistrationDialog", "A+", nullptr));
        addEngineButton->setText(QCoreApplication::translate("EngineRegistrationDialog", "\350\277\275\345\212\240", nullptr));
        removeEngineButton->setText(QCoreApplication::translate("EngineRegistrationDialog", "\345\211\212\351\231\244", nullptr));
        configureEngineButton->setText(QCoreApplication::translate("EngineRegistrationDialog", "\350\250\255\345\256\232", nullptr));
        closeButton->setText(QCoreApplication::translate("EngineRegistrationDialog", "\351\226\211\343\201\230\343\202\213", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EngineRegistrationDialog: public Ui_EngineRegistrationDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ENGINEREGISTRATIONDIALOG_H
