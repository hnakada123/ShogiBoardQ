/********************************************************************************
** Form generated from reading UI file 'versiondialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VERSIONDIALOG_H
#define UI_VERSIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_VersionDialog
{
public:
    QVBoxLayout *mainLayout;
    QSpacerItem *topSpacer;
    QHBoxLayout *iconLayout;
    QSpacerItem *iconLeftSpacer;
    QLabel *iconLabel;
    QSpacerItem *iconRightSpacer;
    QHBoxLayout *titleLayout;
    QSpacerItem *titleLeftSpacer;
    QLabel *label;
    QSpacerItem *titleRightSpacer;
    QHBoxLayout *versionLayout;
    QSpacerItem *versionLeftSpacer;
    QLabel *versionLabel;
    QSpacerItem *versionRightSpacer;
    QHBoxLayout *authorLayout;
    QSpacerItem *authorLeftSpacer;
    QLabel *label_2;
    QSpacerItem *authorRightSpacer;
    QHBoxLayout *buildDateLayout;
    QSpacerItem *buildDateLeftSpacer;
    QLabel *buildDateLabel;
    QSpacerItem *buildDateRightSpacer;
    QSpacerItem *bottomSpacer;
    QHBoxLayout *buttonLayout;
    QSpacerItem *buttonLeftSpacer;
    QPushButton *pushButton;
    QSpacerItem *buttonRightSpacer;

    void setupUi(QDialog *VersionDialog)
    {
        if (VersionDialog->objectName().isEmpty())
            VersionDialog->setObjectName("VersionDialog");
        VersionDialog->resize(400, 340);
        mainLayout = new QVBoxLayout(VersionDialog);
        mainLayout->setSpacing(12);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(30, 20, 30, 20);
        topSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        mainLayout->addItem(topSpacer);

        iconLayout = new QHBoxLayout();
        iconLayout->setObjectName("iconLayout");
        iconLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        iconLayout->addItem(iconLeftSpacer);

        iconLabel = new QLabel(VersionDialog);
        iconLabel->setObjectName("iconLabel");
        iconLabel->setMinimumSize(QSize(64, 64));
        iconLabel->setMaximumSize(QSize(64, 64));
        iconLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        iconLayout->addWidget(iconLabel);

        iconRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        iconLayout->addItem(iconRightSpacer);


        mainLayout->addLayout(iconLayout);

        titleLayout = new QHBoxLayout();
        titleLayout->setObjectName("titleLayout");
        titleLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        titleLayout->addItem(titleLeftSpacer);

        label = new QLabel(VersionDialog);
        label->setObjectName("label");
        QFont font;
        font.setPointSize(26);
        font.setBold(true);
        label->setFont(font);

        titleLayout->addWidget(label);

        titleRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        titleLayout->addItem(titleRightSpacer);


        mainLayout->addLayout(titleLayout);

        versionLayout = new QHBoxLayout();
        versionLayout->setObjectName("versionLayout");
        versionLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        versionLayout->addItem(versionLeftSpacer);

        versionLabel = new QLabel(VersionDialog);
        versionLabel->setObjectName("versionLabel");
        versionLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        versionLayout->addWidget(versionLabel);

        versionRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        versionLayout->addItem(versionRightSpacer);


        mainLayout->addLayout(versionLayout);

        authorLayout = new QHBoxLayout();
        authorLayout->setObjectName("authorLayout");
        authorLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        authorLayout->addItem(authorLeftSpacer);

        label_2 = new QLabel(VersionDialog);
        label_2->setObjectName("label_2");

        authorLayout->addWidget(label_2);

        authorRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        authorLayout->addItem(authorRightSpacer);


        mainLayout->addLayout(authorLayout);

        buildDateLayout = new QHBoxLayout();
        buildDateLayout->setObjectName("buildDateLayout");
        buildDateLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buildDateLayout->addItem(buildDateLeftSpacer);

        buildDateLabel = new QLabel(VersionDialog);
        buildDateLabel->setObjectName("buildDateLabel");
        QFont font1;
        font1.setPointSize(9);
        buildDateLabel->setFont(font1);
        buildDateLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        buildDateLayout->addWidget(buildDateLabel);

        buildDateRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buildDateLayout->addItem(buildDateRightSpacer);


        mainLayout->addLayout(buildDateLayout);

        bottomSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        mainLayout->addItem(bottomSpacer);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName("buttonLayout");
        buttonLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(buttonLeftSpacer);

        pushButton = new QPushButton(VersionDialog);
        pushButton->setObjectName("pushButton");
        pushButton->setMinimumSize(QSize(125, 39));

        buttonLayout->addWidget(pushButton);

        buttonRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(buttonRightSpacer);


        mainLayout->addLayout(buttonLayout);


        retranslateUi(VersionDialog);
        QObject::connect(pushButton, &QPushButton::clicked, VersionDialog, qOverload<>(&QDialog::accept));

        QMetaObject::connectSlotsByName(VersionDialog);
    } // setupUi

    void retranslateUi(QDialog *VersionDialog)
    {
        VersionDialog->setWindowTitle(QCoreApplication::translate("VersionDialog", "\343\203\220\343\203\274\343\202\270\343\203\247\343\203\263\346\203\205\345\240\261", nullptr));
        iconLabel->setText(QString());
        label->setText(QCoreApplication::translate("VersionDialog", "\345\260\206\346\243\213\347\233\244Q", nullptr));
        versionLabel->setText(QString());
        label_2->setText(QCoreApplication::translate("VersionDialog", "Hiroshi Nakada", nullptr));
        buildDateLabel->setText(QString());
        pushButton->setText(QCoreApplication::translate("VersionDialog", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VersionDialog: public Ui_VersionDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VERSIONDIALOG_H
