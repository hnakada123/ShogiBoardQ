/********************************************************************************
** Form generated from reading UI file 'csagamedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CSAGAMEDIALOG_H
#define UI_CSAGAMEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_CsaGameDialog
{
public:
    QVBoxLayout *verticalLayoutMain;
    QGroupBox *groupBoxPlayer;
    QVBoxLayout *verticalLayoutPlayer;
    QHBoxLayout *horizontalLayoutHuman;
    QRadioButton *radioButtonHuman;
    QSpacerItem *horizontalSpacerHuman;
    QHBoxLayout *horizontalLayoutEngine;
    QRadioButton *radioButtonEngine;
    QComboBox *comboBoxEngine;
    QPushButton *pushButtonEngineSettings;
    QSpacerItem *horizontalSpacerEngine;
    QGroupBox *groupBoxServer;
    QFormLayout *formLayoutServer;
    QLabel *labelHistory;
    QComboBox *comboBoxHistory;
    QLabel *labelVersion;
    QComboBox *comboBoxVersion;
    QLabel *labelHost;
    QLineEdit *lineEditHost;
    QLabel *labelPort;
    QSpinBox *spinBoxPort;
    QLabel *labelId;
    QLineEdit *lineEditId;
    QLabel *labelPassword;
    QLineEdit *lineEditPassword;
    QCheckBox *checkBoxShowPassword;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayoutButtons;
    QToolButton *toolButtonFontDecrease;
    QToolButton *toolButtonFontIncrease;
    QSpacerItem *horizontalSpacerButtons;
    QPushButton *pushButtonStart;
    QPushButton *pushButtonCancel;

    void setupUi(QDialog *CsaGameDialog)
    {
        if (CsaGameDialog->objectName().isEmpty())
            CsaGameDialog->setObjectName("CsaGameDialog");
        CsaGameDialog->resize(450, 380);
        verticalLayoutMain = new QVBoxLayout(CsaGameDialog);
        verticalLayoutMain->setObjectName("verticalLayoutMain");
        groupBoxPlayer = new QGroupBox(CsaGameDialog);
        groupBoxPlayer->setObjectName("groupBoxPlayer");
        verticalLayoutPlayer = new QVBoxLayout(groupBoxPlayer);
        verticalLayoutPlayer->setObjectName("verticalLayoutPlayer");
        horizontalLayoutHuman = new QHBoxLayout();
        horizontalLayoutHuman->setObjectName("horizontalLayoutHuman");
        radioButtonHuman = new QRadioButton(groupBoxPlayer);
        radioButtonHuman->setObjectName("radioButtonHuman");
        radioButtonHuman->setChecked(true);

        horizontalLayoutHuman->addWidget(radioButtonHuman);

        horizontalSpacerHuman = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutHuman->addItem(horizontalSpacerHuman);


        verticalLayoutPlayer->addLayout(horizontalLayoutHuman);

        horizontalLayoutEngine = new QHBoxLayout();
        horizontalLayoutEngine->setObjectName("horizontalLayoutEngine");
        radioButtonEngine = new QRadioButton(groupBoxPlayer);
        radioButtonEngine->setObjectName("radioButtonEngine");

        horizontalLayoutEngine->addWidget(radioButtonEngine);

        comboBoxEngine = new QComboBox(groupBoxPlayer);
        comboBoxEngine->setObjectName("comboBoxEngine");
        comboBoxEngine->setMinimumSize(QSize(200, 0));

        horizontalLayoutEngine->addWidget(comboBoxEngine);

        pushButtonEngineSettings = new QPushButton(groupBoxPlayer);
        pushButtonEngineSettings->setObjectName("pushButtonEngineSettings");

        horizontalLayoutEngine->addWidget(pushButtonEngineSettings);

        horizontalSpacerEngine = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutEngine->addItem(horizontalSpacerEngine);


        verticalLayoutPlayer->addLayout(horizontalLayoutEngine);


        verticalLayoutMain->addWidget(groupBoxPlayer);

        groupBoxServer = new QGroupBox(CsaGameDialog);
        groupBoxServer->setObjectName("groupBoxServer");
        formLayoutServer = new QFormLayout(groupBoxServer);
        formLayoutServer->setObjectName("formLayoutServer");
        labelHistory = new QLabel(groupBoxServer);
        labelHistory->setObjectName("labelHistory");

        formLayoutServer->setWidget(0, QFormLayout::ItemRole::LabelRole, labelHistory);

        comboBoxHistory = new QComboBox(groupBoxServer);
        comboBoxHistory->setObjectName("comboBoxHistory");
        comboBoxHistory->setMinimumSize(QSize(280, 0));

        formLayoutServer->setWidget(0, QFormLayout::ItemRole::FieldRole, comboBoxHistory);

        labelVersion = new QLabel(groupBoxServer);
        labelVersion->setObjectName("labelVersion");

        formLayoutServer->setWidget(1, QFormLayout::ItemRole::LabelRole, labelVersion);

        comboBoxVersion = new QComboBox(groupBoxServer);
        comboBoxVersion->addItem(QString());
        comboBoxVersion->addItem(QString());
        comboBoxVersion->addItem(QString());
        comboBoxVersion->setObjectName("comboBoxVersion");
        comboBoxVersion->setMinimumSize(QSize(280, 0));

        formLayoutServer->setWidget(1, QFormLayout::ItemRole::FieldRole, comboBoxVersion);

        labelHost = new QLabel(groupBoxServer);
        labelHost->setObjectName("labelHost");

        formLayoutServer->setWidget(2, QFormLayout::ItemRole::LabelRole, labelHost);

        lineEditHost = new QLineEdit(groupBoxServer);
        lineEditHost->setObjectName("lineEditHost");

        formLayoutServer->setWidget(2, QFormLayout::ItemRole::FieldRole, lineEditHost);

        labelPort = new QLabel(groupBoxServer);
        labelPort->setObjectName("labelPort");

        formLayoutServer->setWidget(3, QFormLayout::ItemRole::LabelRole, labelPort);

        spinBoxPort = new QSpinBox(groupBoxServer);
        spinBoxPort->setObjectName("spinBoxPort");
        spinBoxPort->setMinimum(1);
        spinBoxPort->setMaximum(65535);
        spinBoxPort->setValue(4081);

        formLayoutServer->setWidget(3, QFormLayout::ItemRole::FieldRole, spinBoxPort);

        labelId = new QLabel(groupBoxServer);
        labelId->setObjectName("labelId");

        formLayoutServer->setWidget(4, QFormLayout::ItemRole::LabelRole, labelId);

        lineEditId = new QLineEdit(groupBoxServer);
        lineEditId->setObjectName("lineEditId");

        formLayoutServer->setWidget(4, QFormLayout::ItemRole::FieldRole, lineEditId);

        labelPassword = new QLabel(groupBoxServer);
        labelPassword->setObjectName("labelPassword");

        formLayoutServer->setWidget(5, QFormLayout::ItemRole::LabelRole, labelPassword);

        lineEditPassword = new QLineEdit(groupBoxServer);
        lineEditPassword->setObjectName("lineEditPassword");
        lineEditPassword->setEchoMode(QLineEdit::EchoMode::Password);

        formLayoutServer->setWidget(5, QFormLayout::ItemRole::FieldRole, lineEditPassword);

        checkBoxShowPassword = new QCheckBox(groupBoxServer);
        checkBoxShowPassword->setObjectName("checkBoxShowPassword");

        formLayoutServer->setWidget(6, QFormLayout::ItemRole::FieldRole, checkBoxShowPassword);


        verticalLayoutMain->addWidget(groupBoxServer);

        verticalSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutMain->addItem(verticalSpacer);

        horizontalLayoutButtons = new QHBoxLayout();
        horizontalLayoutButtons->setObjectName("horizontalLayoutButtons");
        toolButtonFontDecrease = new QToolButton(CsaGameDialog);
        toolButtonFontDecrease->setObjectName("toolButtonFontDecrease");

        horizontalLayoutButtons->addWidget(toolButtonFontDecrease);

        toolButtonFontIncrease = new QToolButton(CsaGameDialog);
        toolButtonFontIncrease->setObjectName("toolButtonFontIncrease");

        horizontalLayoutButtons->addWidget(toolButtonFontIncrease);

        horizontalSpacerButtons = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutButtons->addItem(horizontalSpacerButtons);

        pushButtonStart = new QPushButton(CsaGameDialog);
        pushButtonStart->setObjectName("pushButtonStart");

        horizontalLayoutButtons->addWidget(pushButtonStart);

        pushButtonCancel = new QPushButton(CsaGameDialog);
        pushButtonCancel->setObjectName("pushButtonCancel");

        horizontalLayoutButtons->addWidget(pushButtonCancel);


        verticalLayoutMain->addLayout(horizontalLayoutButtons);


        retranslateUi(CsaGameDialog);

        pushButtonStart->setDefault(true);


        QMetaObject::connectSlotsByName(CsaGameDialog);
    } // setupUi

    void retranslateUi(QDialog *CsaGameDialog)
    {
        CsaGameDialog->setWindowTitle(QCoreApplication::translate("CsaGameDialog", "\351\200\232\344\277\241\345\257\276\345\261\200\357\274\210CSA\357\274\211", nullptr));
        groupBoxPlayer->setTitle(QCoreApplication::translate("CsaGameDialog", "\343\201\223\343\201\241\343\202\211\345\201\264\343\201\256\345\257\276\345\261\200\350\200\205", nullptr));
        radioButtonHuman->setText(QCoreApplication::translate("CsaGameDialog", "\344\272\272\351\226\223", nullptr));
        radioButtonEngine->setText(QCoreApplication::translate("CsaGameDialog", "\343\202\250\343\203\263\343\202\270\343\203\263", nullptr));
        pushButtonEngineSettings->setText(QCoreApplication::translate("CsaGameDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232...", nullptr));
        groupBoxServer->setTitle(QCoreApplication::translate("CsaGameDialog", "\343\202\265\343\203\274\343\203\220\343\203\274", nullptr));
        labelHistory->setText(QCoreApplication::translate("CsaGameDialog", "\345\261\245\346\255\264\343\201\213\343\202\211\351\201\270\343\201\266", nullptr));
        labelVersion->setText(QCoreApplication::translate("CsaGameDialog", "\343\203\220\343\203\274\343\202\270\343\203\247\343\203\263", nullptr));
        comboBoxVersion->setItemText(0, QCoreApplication::translate("CsaGameDialog", "CSA\343\203\227\343\203\255\343\203\210\343\202\263\343\203\2531.2.1 \350\252\255\343\201\277\347\255\213\343\202\263\343\203\241\343\203\263\343\203\210\345\207\272\345\212\233\343\201\202\343\202\212", nullptr));
        comboBoxVersion->setItemText(1, QCoreApplication::translate("CsaGameDialog", "CSA\343\203\227\343\203\255\343\203\210\343\202\263\343\203\2531.2.1", nullptr));
        comboBoxVersion->setItemText(2, QCoreApplication::translate("CsaGameDialog", "CSA\343\203\227\343\203\255\343\203\210\343\202\263\343\203\2531.1", nullptr));

        labelHost->setText(QCoreApplication::translate("CsaGameDialog", "\346\216\245\347\266\232\345\205\210\343\203\233\343\202\271\343\203\210", nullptr));
        lineEditHost->setPlaceholderText(QCoreApplication::translate("CsaGameDialog", "\344\276\213: 172.17.0.2", nullptr));
        labelPort->setText(QCoreApplication::translate("CsaGameDialog", "\343\203\235\343\203\274\343\203\210\347\225\252\345\217\267", nullptr));
        labelId->setText(QCoreApplication::translate("CsaGameDialog", "ID", nullptr));
        lineEditId->setPlaceholderText(QCoreApplication::translate("CsaGameDialog", "\343\203\255\343\202\260\343\202\244\343\203\263ID", nullptr));
        labelPassword->setText(QCoreApplication::translate("CsaGameDialog", "\343\203\221\343\202\271\343\203\257\343\203\274\343\203\211", nullptr));
        lineEditPassword->setPlaceholderText(QCoreApplication::translate("CsaGameDialog", "\343\203\221\343\202\271\343\203\257\343\203\274\343\203\211", nullptr));
        checkBoxShowPassword->setText(QCoreApplication::translate("CsaGameDialog", "\343\203\221\343\202\271\343\203\257\343\203\274\343\203\211\343\202\222\350\241\250\347\244\272\343\201\231\343\202\213", nullptr));
        toolButtonFontDecrease->setText(QCoreApplication::translate("CsaGameDialog", "A-", nullptr));
#if QT_CONFIG(tooltip)
        toolButtonFontDecrease->setToolTip(QCoreApplication::translate("CsaGameDialog", "\343\203\225\343\202\251\343\203\263\343\203\210\343\202\265\343\202\244\343\202\272\343\202\222\345\260\217\343\201\225\343\201\217\343\201\231\343\202\213", nullptr));
#endif // QT_CONFIG(tooltip)
        toolButtonFontIncrease->setText(QCoreApplication::translate("CsaGameDialog", "A+", nullptr));
#if QT_CONFIG(tooltip)
        toolButtonFontIncrease->setToolTip(QCoreApplication::translate("CsaGameDialog", "\343\203\225\343\202\251\343\203\263\343\203\210\343\202\265\343\202\244\343\202\272\343\202\222\345\244\247\343\201\215\343\201\217\343\201\231\343\202\213", nullptr));
#endif // QT_CONFIG(tooltip)
        pushButtonStart->setText(QCoreApplication::translate("CsaGameDialog", "\345\257\276\345\261\200\351\226\213\345\247\213", nullptr));
        pushButtonCancel->setText(QCoreApplication::translate("CsaGameDialog", "\343\202\255\343\203\243\343\203\263\343\202\273\343\203\253", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CsaGameDialog: public Ui_CsaGameDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CSAGAMEDIALOG_H
