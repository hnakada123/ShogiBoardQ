/********************************************************************************
** Form generated from reading UI file 'changeenginesettingsdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CHANGEENGINESETTINGSDIALOG_H
#define UI_CHANGEENGINESETTINGSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ChangeEngineSettingsDialog
{
public:
    QVBoxLayout *mainDialogLayout;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QLabel *label;
    QSpacerItem *horizontalSpacer_2;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QHBoxLayout *bottomLayout;
    QPushButton *restoreButton;
    QPushButton *fontDecreaseButton;
    QPushButton *fontIncreaseButton;
    QSpacerItem *horizontalSpacer_3;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *ChangeEngineSettingsDialog)
    {
        if (ChangeEngineSettingsDialog->objectName().isEmpty())
            ChangeEngineSettingsDialog->setObjectName("ChangeEngineSettingsDialog");
        ChangeEngineSettingsDialog->resize(800, 800);
        mainDialogLayout = new QVBoxLayout(ChangeEngineSettingsDialog);
        mainDialogLayout->setObjectName("mainDialogLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        label = new QLabel(ChangeEngineSettingsDialog);
        label->setObjectName("label");

        horizontalLayout->addWidget(label);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);


        mainDialogLayout->addLayout(horizontalLayout);

        scrollArea = new QScrollArea(ChangeEngineSettingsDialog);
        scrollArea->setObjectName("scrollArea");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(1);
        sizePolicy.setHeightForWidth(scrollArea->sizePolicy().hasHeightForWidth());
        scrollArea->setSizePolicy(sizePolicy);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 777, 694));
        scrollArea->setWidget(scrollAreaWidgetContents);

        mainDialogLayout->addWidget(scrollArea);

        bottomLayout = new QHBoxLayout();
        bottomLayout->setObjectName("bottomLayout");
        restoreButton = new QPushButton(ChangeEngineSettingsDialog);
        restoreButton->setObjectName("restoreButton");

        bottomLayout->addWidget(restoreButton);

        fontDecreaseButton = new QPushButton(ChangeEngineSettingsDialog);
        fontDecreaseButton->setObjectName("fontDecreaseButton");
        fontDecreaseButton->setMaximumSize(QSize(40, 16777215));

        bottomLayout->addWidget(fontDecreaseButton);

        fontIncreaseButton = new QPushButton(ChangeEngineSettingsDialog);
        fontIncreaseButton->setObjectName("fontIncreaseButton");
        fontIncreaseButton->setMaximumSize(QSize(40, 16777215));

        bottomLayout->addWidget(fontIncreaseButton);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        bottomLayout->addItem(horizontalSpacer_3);

        buttonBox = new QDialogButtonBox(ChangeEngineSettingsDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        bottomLayout->addWidget(buttonBox);


        mainDialogLayout->addLayout(bottomLayout);


        retranslateUi(ChangeEngineSettingsDialog);

        QMetaObject::connectSlotsByName(ChangeEngineSettingsDialog);
    } // setupUi

    void retranslateUi(QDialog *ChangeEngineSettingsDialog)
    {
        ChangeEngineSettingsDialog->setWindowTitle(QCoreApplication::translate("ChangeEngineSettingsDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        label->setText(QCoreApplication::translate("ChangeEngineSettingsDialog", "TextLabel", nullptr));
        restoreButton->setText(QCoreApplication::translate("ChangeEngineSettingsDialog", "\346\227\242\345\256\232\345\200\244\343\201\253\346\210\273\343\201\231", nullptr));
        fontDecreaseButton->setText(QCoreApplication::translate("ChangeEngineSettingsDialog", "A-", nullptr));
        fontIncreaseButton->setText(QCoreApplication::translate("ChangeEngineSettingsDialog", "A+", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ChangeEngineSettingsDialog: public Ui_ChangeEngineSettingsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CHANGEENGINESETTINGSDIALOG_H
