/********************************************************************************
** Form generated from reading UI file 'considerationdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONSIDERATIONDIALOG_H
#define UI_CONSIDERATIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ConsiderationDialog
{
public:
    QVBoxLayout *verticalLayoutMain;
    QGroupBox *groupBoxEngine;
    QVBoxLayout *verticalLayoutEngine;
    QHBoxLayout *horizontalLayoutEngine;
    QComboBox *comboBoxEngine1;
    QPushButton *engineSetting;
    QSpacerItem *horizontalSpacerEngine;
    QGroupBox *groupBoxTime;
    QVBoxLayout *verticalLayoutTime;
    QHBoxLayout *horizontalLayoutUnlimited;
    QRadioButton *unlimitedTimeRadioButton;
    QSpacerItem *horizontalSpacerUnlimited;
    QHBoxLayout *horizontalLayoutByoyomi;
    QRadioButton *considerationTimeRadioButton;
    QSpinBox *byoyomiSec;
    QLabel *label_4;
    QSpacerItem *horizontalSpacerByoyomi;
    QGroupBox *groupBoxMultiPV;
    QVBoxLayout *verticalLayoutMultiPV;
    QHBoxLayout *horizontalLayoutMultiPV;
    QLabel *labelMultiPV;
    QSpinBox *spinBoxMultiPV;
    QLabel *labelMultiPVUnit;
    QSpacerItem *horizontalSpacerMultiPV;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayoutBottom;
    QPushButton *toolButtonFontDecrease;
    QPushButton *toolButtonFontIncrease;
    QSpacerItem *horizontalSpacerBottom;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *ConsiderationDialog)
    {
        if (ConsiderationDialog->objectName().isEmpty())
            ConsiderationDialog->setObjectName("ConsiderationDialog");
        ConsiderationDialog->resize(500, 280);
        verticalLayoutMain = new QVBoxLayout(ConsiderationDialog);
        verticalLayoutMain->setObjectName("verticalLayoutMain");
        groupBoxEngine = new QGroupBox(ConsiderationDialog);
        groupBoxEngine->setObjectName("groupBoxEngine");
        verticalLayoutEngine = new QVBoxLayout(groupBoxEngine);
        verticalLayoutEngine->setObjectName("verticalLayoutEngine");
        horizontalLayoutEngine = new QHBoxLayout();
        horizontalLayoutEngine->setObjectName("horizontalLayoutEngine");
        comboBoxEngine1 = new QComboBox(groupBoxEngine);
        comboBoxEngine1->setObjectName("comboBoxEngine1");
        comboBoxEngine1->setMinimumSize(QSize(200, 0));

        horizontalLayoutEngine->addWidget(comboBoxEngine1);

        engineSetting = new QPushButton(groupBoxEngine);
        engineSetting->setObjectName("engineSetting");

        horizontalLayoutEngine->addWidget(engineSetting);

        horizontalSpacerEngine = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutEngine->addItem(horizontalSpacerEngine);


        verticalLayoutEngine->addLayout(horizontalLayoutEngine);


        verticalLayoutMain->addWidget(groupBoxEngine);

        groupBoxTime = new QGroupBox(ConsiderationDialog);
        groupBoxTime->setObjectName("groupBoxTime");
        verticalLayoutTime = new QVBoxLayout(groupBoxTime);
        verticalLayoutTime->setObjectName("verticalLayoutTime");
        horizontalLayoutUnlimited = new QHBoxLayout();
        horizontalLayoutUnlimited->setObjectName("horizontalLayoutUnlimited");
        unlimitedTimeRadioButton = new QRadioButton(groupBoxTime);
        unlimitedTimeRadioButton->setObjectName("unlimitedTimeRadioButton");

        horizontalLayoutUnlimited->addWidget(unlimitedTimeRadioButton);

        horizontalSpacerUnlimited = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutUnlimited->addItem(horizontalSpacerUnlimited);


        verticalLayoutTime->addLayout(horizontalLayoutUnlimited);

        horizontalLayoutByoyomi = new QHBoxLayout();
        horizontalLayoutByoyomi->setObjectName("horizontalLayoutByoyomi");
        considerationTimeRadioButton = new QRadioButton(groupBoxTime);
        considerationTimeRadioButton->setObjectName("considerationTimeRadioButton");

        horizontalLayoutByoyomi->addWidget(considerationTimeRadioButton);

        byoyomiSec = new QSpinBox(groupBoxTime);
        byoyomiSec->setObjectName("byoyomiSec");
        byoyomiSec->setMinimum(0);
        byoyomiSec->setMaximum(999999);

        horizontalLayoutByoyomi->addWidget(byoyomiSec);

        label_4 = new QLabel(groupBoxTime);
        label_4->setObjectName("label_4");

        horizontalLayoutByoyomi->addWidget(label_4);

        horizontalSpacerByoyomi = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutByoyomi->addItem(horizontalSpacerByoyomi);


        verticalLayoutTime->addLayout(horizontalLayoutByoyomi);


        verticalLayoutMain->addWidget(groupBoxTime);

        groupBoxMultiPV = new QGroupBox(ConsiderationDialog);
        groupBoxMultiPV->setObjectName("groupBoxMultiPV");
        verticalLayoutMultiPV = new QVBoxLayout(groupBoxMultiPV);
        verticalLayoutMultiPV->setObjectName("verticalLayoutMultiPV");
        horizontalLayoutMultiPV = new QHBoxLayout();
        horizontalLayoutMultiPV->setObjectName("horizontalLayoutMultiPV");
        labelMultiPV = new QLabel(groupBoxMultiPV);
        labelMultiPV->setObjectName("labelMultiPV");

        horizontalLayoutMultiPV->addWidget(labelMultiPV);

        spinBoxMultiPV = new QSpinBox(groupBoxMultiPV);
        spinBoxMultiPV->setObjectName("spinBoxMultiPV");
        spinBoxMultiPV->setMinimum(1);
        spinBoxMultiPV->setMaximum(10);
        spinBoxMultiPV->setValue(1);

        horizontalLayoutMultiPV->addWidget(spinBoxMultiPV);

        labelMultiPVUnit = new QLabel(groupBoxMultiPV);
        labelMultiPVUnit->setObjectName("labelMultiPVUnit");

        horizontalLayoutMultiPV->addWidget(labelMultiPVUnit);

        horizontalSpacerMultiPV = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutMultiPV->addItem(horizontalSpacerMultiPV);


        verticalLayoutMultiPV->addLayout(horizontalLayoutMultiPV);


        verticalLayoutMain->addWidget(groupBoxMultiPV);

        verticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutMain->addItem(verticalSpacer);

        horizontalLayoutBottom = new QHBoxLayout();
        horizontalLayoutBottom->setObjectName("horizontalLayoutBottom");
        toolButtonFontDecrease = new QPushButton(ConsiderationDialog);
        toolButtonFontDecrease->setObjectName("toolButtonFontDecrease");
        toolButtonFontDecrease->setMinimumSize(QSize(40, 0));
        toolButtonFontDecrease->setMaximumSize(QSize(40, 16777215));

        horizontalLayoutBottom->addWidget(toolButtonFontDecrease);

        toolButtonFontIncrease = new QPushButton(ConsiderationDialog);
        toolButtonFontIncrease->setObjectName("toolButtonFontIncrease");
        toolButtonFontIncrease->setMinimumSize(QSize(40, 0));
        toolButtonFontIncrease->setMaximumSize(QSize(40, 16777215));

        horizontalLayoutBottom->addWidget(toolButtonFontIncrease);

        horizontalSpacerBottom = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutBottom->addItem(horizontalSpacerBottom);

        buttonBox = new QDialogButtonBox(ConsiderationDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        horizontalLayoutBottom->addWidget(buttonBox);


        verticalLayoutMain->addLayout(horizontalLayoutBottom);


        retranslateUi(ConsiderationDialog);

        QMetaObject::connectSlotsByName(ConsiderationDialog);
    } // setupUi

    void retranslateUi(QDialog *ConsiderationDialog)
    {
        ConsiderationDialog->setWindowTitle(QCoreApplication::translate("ConsiderationDialog", "\346\244\234\350\250\216", nullptr));
        groupBoxEngine->setTitle(QCoreApplication::translate("ConsiderationDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        engineSetting->setText(QCoreApplication::translate("ConsiderationDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        groupBoxTime->setTitle(QCoreApplication::translate("ConsiderationDialog", "\346\200\235\350\200\203\346\231\202\351\226\223", nullptr));
        unlimitedTimeRadioButton->setText(QCoreApplication::translate("ConsiderationDialog", "\346\231\202\351\226\223\347\204\241\345\210\266\351\231\220", nullptr));
        considerationTimeRadioButton->setText(QCoreApplication::translate("ConsiderationDialog", "\346\244\234\350\250\216\346\231\202\351\226\223", nullptr));
        label_4->setText(QCoreApplication::translate("ConsiderationDialog", "\347\247\222\343\201\276\343\201\247", nullptr));
        groupBoxMultiPV->setTitle(QCoreApplication::translate("ConsiderationDialog", "\345\200\231\350\243\234\346\211\213", nullptr));
        labelMultiPV->setText(QCoreApplication::translate("ConsiderationDialog", "\345\200\231\350\243\234\346\211\213\343\201\256\346\225\260", nullptr));
#if QT_CONFIG(tooltip)
        spinBoxMultiPV->setToolTip(QCoreApplication::translate("ConsiderationDialog", "\350\251\225\344\276\241\345\200\244\343\201\214\345\244\247\343\201\215\343\201\204\351\240\206\343\201\253\350\241\250\347\244\272\343\201\231\343\202\213\345\200\231\350\243\234\346\211\213\343\201\256\346\225\260\343\202\222\346\214\207\345\256\232\343\201\227\343\201\276\343\201\231", nullptr));
#endif // QT_CONFIG(tooltip)
        labelMultiPVUnit->setText(QCoreApplication::translate("ConsiderationDialog", "\346\211\213", nullptr));
        toolButtonFontDecrease->setText(QCoreApplication::translate("ConsiderationDialog", "A-", nullptr));
#if QT_CONFIG(tooltip)
        toolButtonFontDecrease->setToolTip(QCoreApplication::translate("ConsiderationDialog", "\343\203\225\343\202\251\343\203\263\343\203\210\343\202\265\343\202\244\343\202\272\343\202\222\345\260\217\343\201\225\343\201\217\343\201\231\343\202\213", nullptr));
#endif // QT_CONFIG(tooltip)
        toolButtonFontIncrease->setText(QCoreApplication::translate("ConsiderationDialog", "A+", nullptr));
#if QT_CONFIG(tooltip)
        toolButtonFontIncrease->setToolTip(QCoreApplication::translate("ConsiderationDialog", "\343\203\225\343\202\251\343\203\263\343\203\210\343\202\265\343\202\244\343\202\272\343\202\222\345\244\247\343\201\215\343\201\217\343\201\231\343\202\213", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class ConsiderationDialog: public Ui_ConsiderationDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONSIDERATIONDIALOG_H
