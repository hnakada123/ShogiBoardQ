/********************************************************************************
** Form generated from reading UI file 'promotedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PROMOTEDIALOG_H
#define UI_PROMOTEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSplitter>

QT_BEGIN_NAMESPACE

class Ui_PromoteDialog
{
public:
    QLabel *label;
    QSplitter *splitter;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *PromoteDialog)
    {
        if (PromoteDialog->objectName().isEmpty())
            PromoteDialog->setObjectName("PromoteDialog");
        PromoteDialog->setWindowModality(Qt::ApplicationModal);
        PromoteDialog->resize(405, 107);
        PromoteDialog->setContextMenuPolicy(Qt::DefaultContextMenu);
        PromoteDialog->setLayoutDirection(Qt::LeftToRight);
        label = new QLabel(PromoteDialog);
        label->setObjectName("label");
        label->setGeometry(QRect(12, 6, 381, 23));
        splitter = new QSplitter(PromoteDialog);
        splitter->setObjectName("splitter");
        splitter->setGeometry(QRect(-10, 50, 336, 35));
        splitter->setOrientation(Qt::Horizontal);
        buttonBox = new QDialogButtonBox(splitter);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        splitter->addWidget(buttonBox);

        retranslateUi(PromoteDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, PromoteDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, PromoteDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(PromoteDialog);
    } // setupUi

    void retranslateUi(QDialog *PromoteDialog)
    {
        PromoteDialog->setWindowTitle(QCoreApplication::translate("PromoteDialog", "\346\210\220\343\202\213\343\203\273\344\270\215\346\210\220", nullptr));
        label->setText(QCoreApplication::translate("PromoteDialog", "\346\210\220\343\202\212\343\201\276\343\201\231\343\201\213\357\274\237(OK: \346\210\220\343\202\213\343\200\201\343\202\255\343\203\243\343\203\263\343\202\273\343\203\253:\344\270\215\346\210\220)", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PromoteDialog: public Ui_PromoteDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PROMOTEDIALOG_H
