/********************************************************************************
** Form generated from reading UI file 'kifuanalysisdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_KIFUANALYSISDIALOG_H
#define UI_KIFUANALYSISDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_KifuAnalysisDialog
{
public:
    QVBoxLayout *mainLayout;
    QLabel *labelSectionEngine;
    QHBoxLayout *engineLayout;
    QComboBox *comboBoxEngine1;
    QPushButton *engineSetting;
    QSpacerItem *spacerAfterEngine;
    QLabel *labelSectionRange;
    QRadioButton *radioButtonInitPosition;
    QHBoxLayout *rangeInputLayout;
    QRadioButton *radioButtonRangePosition;
    QSpinBox *spinBoxStartPly;
    QLabel *labelFrom;
    QSpinBox *spinBoxEndPly;
    QLabel *labelTo;
    QSpacerItem *horizontalSpacer;
    QSpacerItem *spacerAfterRange;
    QLabel *labelSectionTime;
    QHBoxLayout *timeLayout;
    QLabel *label;
    QSpinBox *byoyomiSec;
    QLabel *label_2;
    QSpacerItem *horizontalSpacer2;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *buttonLayout;
    QPushButton *btnFontDecrease;
    QPushButton *btnFontIncrease;
    QSpacerItem *buttonSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *KifuAnalysisDialog)
    {
        if (KifuAnalysisDialog->objectName().isEmpty())
            KifuAnalysisDialog->setObjectName("KifuAnalysisDialog");
        KifuAnalysisDialog->resize(500, 340);
        KifuAnalysisDialog->setMinimumSize(QSize(400, 280));
        mainLayout = new QVBoxLayout(KifuAnalysisDialog);
        mainLayout->setSpacing(6);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(16, 16, 16, 16);
        labelSectionEngine = new QLabel(KifuAnalysisDialog);
        labelSectionEngine->setObjectName("labelSectionEngine");

        mainLayout->addWidget(labelSectionEngine);

        engineLayout = new QHBoxLayout();
        engineLayout->setObjectName("engineLayout");
        comboBoxEngine1 = new QComboBox(KifuAnalysisDialog);
        comboBoxEngine1->setObjectName("comboBoxEngine1");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(comboBoxEngine1->sizePolicy().hasHeightForWidth());
        comboBoxEngine1->setSizePolicy(sizePolicy);

        engineLayout->addWidget(comboBoxEngine1);

        engineSetting = new QPushButton(KifuAnalysisDialog);
        engineSetting->setObjectName("engineSetting");

        engineLayout->addWidget(engineSetting);


        mainLayout->addLayout(engineLayout);

        spacerAfterEngine = new QSpacerItem(0, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        mainLayout->addItem(spacerAfterEngine);

        labelSectionRange = new QLabel(KifuAnalysisDialog);
        labelSectionRange->setObjectName("labelSectionRange");

        mainLayout->addWidget(labelSectionRange);

        radioButtonInitPosition = new QRadioButton(KifuAnalysisDialog);
        radioButtonInitPosition->setObjectName("radioButtonInitPosition");

        mainLayout->addWidget(radioButtonInitPosition);

        rangeInputLayout = new QHBoxLayout();
        rangeInputLayout->setSpacing(6);
        rangeInputLayout->setObjectName("rangeInputLayout");
        radioButtonRangePosition = new QRadioButton(KifuAnalysisDialog);
        radioButtonRangePosition->setObjectName("radioButtonRangePosition");

        rangeInputLayout->addWidget(radioButtonRangePosition);

        spinBoxStartPly = new QSpinBox(KifuAnalysisDialog);
        spinBoxStartPly->setObjectName("spinBoxStartPly");
        spinBoxStartPly->setMinimumSize(QSize(70, 0));
        spinBoxStartPly->setMinimum(0);
        spinBoxStartPly->setMaximum(999);

        rangeInputLayout->addWidget(spinBoxStartPly);

        labelFrom = new QLabel(KifuAnalysisDialog);
        labelFrom->setObjectName("labelFrom");

        rangeInputLayout->addWidget(labelFrom);

        spinBoxEndPly = new QSpinBox(KifuAnalysisDialog);
        spinBoxEndPly->setObjectName("spinBoxEndPly");
        spinBoxEndPly->setMinimumSize(QSize(70, 0));
        spinBoxEndPly->setMinimum(0);
        spinBoxEndPly->setMaximum(999);

        rangeInputLayout->addWidget(spinBoxEndPly);

        labelTo = new QLabel(KifuAnalysisDialog);
        labelTo->setObjectName("labelTo");

        rangeInputLayout->addWidget(labelTo);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        rangeInputLayout->addItem(horizontalSpacer);


        mainLayout->addLayout(rangeInputLayout);

        spacerAfterRange = new QSpacerItem(0, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        mainLayout->addItem(spacerAfterRange);

        labelSectionTime = new QLabel(KifuAnalysisDialog);
        labelSectionTime->setObjectName("labelSectionTime");

        mainLayout->addWidget(labelSectionTime);

        timeLayout = new QHBoxLayout();
        timeLayout->setObjectName("timeLayout");
        label = new QLabel(KifuAnalysisDialog);
        label->setObjectName("label");

        timeLayout->addWidget(label);

        byoyomiSec = new QSpinBox(KifuAnalysisDialog);
        byoyomiSec->setObjectName("byoyomiSec");
        byoyomiSec->setMinimumSize(QSize(80, 0));
        byoyomiSec->setMinimum(1);
        byoyomiSec->setMaximum(3600);
        byoyomiSec->setValue(3);

        timeLayout->addWidget(byoyomiSec);

        label_2 = new QLabel(KifuAnalysisDialog);
        label_2->setObjectName("label_2");

        timeLayout->addWidget(label_2);

        horizontalSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        timeLayout->addItem(horizontalSpacer2);


        mainLayout->addLayout(timeLayout);

        verticalSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        mainLayout->addItem(verticalSpacer);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName("buttonLayout");
        btnFontDecrease = new QPushButton(KifuAnalysisDialog);
        btnFontDecrease->setObjectName("btnFontDecrease");
        btnFontDecrease->setMinimumSize(QSize(40, 0));
        btnFontDecrease->setMaximumSize(QSize(40, 16777215));

        buttonLayout->addWidget(btnFontDecrease);

        btnFontIncrease = new QPushButton(KifuAnalysisDialog);
        btnFontIncrease->setObjectName("btnFontIncrease");
        btnFontIncrease->setMinimumSize(QSize(40, 0));
        btnFontIncrease->setMaximumSize(QSize(40, 16777215));

        buttonLayout->addWidget(btnFontIncrease);

        buttonSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(buttonSpacer);

        buttonBox = new QDialogButtonBox(KifuAnalysisDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        buttonLayout->addWidget(buttonBox);


        mainLayout->addLayout(buttonLayout);


        retranslateUi(KifuAnalysisDialog);

        QMetaObject::connectSlotsByName(KifuAnalysisDialog);
    } // setupUi

    void retranslateUi(QDialog *KifuAnalysisDialog)
    {
        KifuAnalysisDialog->setWindowTitle(QCoreApplication::translate("KifuAnalysisDialog", "\346\243\213\350\255\234\350\247\243\346\236\220", nullptr));
        labelSectionEngine->setText(QCoreApplication::translate("KifuAnalysisDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        engineSetting->setText(QCoreApplication::translate("KifuAnalysisDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        labelSectionRange->setText(QCoreApplication::translate("KifuAnalysisDialog", "\350\247\243\346\236\220\347\257\204\345\233\262", nullptr));
        radioButtonInitPosition->setText(QCoreApplication::translate("KifuAnalysisDialog", "\351\226\213\345\247\213\345\261\200\351\235\242\343\201\213\343\202\211\346\234\200\347\265\202\346\211\213\343\201\276\343\201\247", nullptr));
        radioButtonRangePosition->setText(QString());
        labelFrom->setText(QCoreApplication::translate("KifuAnalysisDialog", "\346\211\213\347\233\256\343\201\213\343\202\211", nullptr));
        labelTo->setText(QCoreApplication::translate("KifuAnalysisDialog", "\346\211\213\347\233\256\343\201\276\343\201\247", nullptr));
        labelSectionTime->setText(QCoreApplication::translate("KifuAnalysisDialog", "\346\231\202\351\226\223\345\210\266\351\231\220", nullptr));
        label->setText(QCoreApplication::translate("KifuAnalysisDialog", "\357\274\221\346\211\213\343\201\202\343\201\237\343\202\212\343\201\256\346\200\235\350\200\203\346\231\202\351\226\223", nullptr));
        label_2->setText(QCoreApplication::translate("KifuAnalysisDialog", "\347\247\222", nullptr));
        btnFontDecrease->setText(QCoreApplication::translate("KifuAnalysisDialog", "A-", nullptr));
#if QT_CONFIG(tooltip)
        btnFontDecrease->setToolTip(QCoreApplication::translate("KifuAnalysisDialog", "\346\226\207\345\255\227\343\202\265\343\202\244\343\202\272\343\202\222\347\270\256\345\260\217", nullptr));
#endif // QT_CONFIG(tooltip)
        btnFontIncrease->setText(QCoreApplication::translate("KifuAnalysisDialog", "A+", nullptr));
#if QT_CONFIG(tooltip)
        btnFontIncrease->setToolTip(QCoreApplication::translate("KifuAnalysisDialog", "\346\226\207\345\255\227\343\202\265\343\202\244\343\202\272\343\202\222\346\213\241\345\244\247", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class KifuAnalysisDialog: public Ui_KifuAnalysisDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_KIFUANALYSISDIALOG_H
