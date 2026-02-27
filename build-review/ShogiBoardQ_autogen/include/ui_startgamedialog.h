/********************************************************************************
** Form generated from reading UI file 'startgamedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STARTGAMEDIALOG_H
#define UI_STARTGAMEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_StartGameDialog
{
public:
    QVBoxLayout *mainLayout;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *scrollLayout;
    QHBoxLayout *playerLayout;
    QGroupBox *groupBox;
    QGridLayout *gridLayoutPlayer1;
    QLabel *labelPlayerType1;
    QComboBox *comboBoxPlayer1;
    QStackedWidget *stackedWidgetLabel1;
    QWidget *pageLabelHuman1;
    QHBoxLayout *layoutLabelHuman1;
    QLabel *labelPlayer1;
    QWidget *pageLabelEngine1;
    QHBoxLayout *layoutLabelEngine1;
    QStackedWidget *stackedWidgetPlayer1;
    QWidget *pageHuman1;
    QHBoxLayout *layoutPageHuman1;
    QLineEdit *lineEditHumanName1;
    QWidget *pageEngine1;
    QHBoxLayout *layoutPageEngine1;
    QPushButton *pushButtonEngineSettings1;
    QSpacerItem *horizontalSpacerEngine1;
    QGroupBox *groupBox_2;
    QGridLayout *gridLayoutPlayer2;
    QLabel *labelPlayerType2;
    QComboBox *comboBoxPlayer2;
    QStackedWidget *stackedWidgetLabel2;
    QWidget *pageLabelHuman2;
    QHBoxLayout *layoutLabelHuman2;
    QLabel *labelPlayer2;
    QWidget *pageLabelEngine2;
    QHBoxLayout *layoutLabelEngine2;
    QStackedWidget *stackedWidgetPlayer2;
    QWidget *pageHuman2;
    QHBoxLayout *layoutPageHuman2;
    QLineEdit *lineEditHumanName2;
    QWidget *pageEngine2;
    QHBoxLayout *layoutPageEngine2;
    QPushButton *pushButtonEngineSettings2;
    QSpacerItem *horizontalSpacerEngine2;
    QHBoxLayout *timeSettingsLayout;
    QGroupBox *groupBox_7;
    QVBoxLayout *verticalLayout_8;
    QHBoxLayout *horizontalLayout_14;
    QLabel *label_2;
    QSpinBox *basicTimeHour1;
    QLabel *label_10;
    QSpinBox *basicTimeMinutes1;
    QLabel *label_11;
    QSpacerItem *horizontalSpacer_10;
    QHBoxLayout *horizontalLayout_15;
    QLabel *label_12;
    QSpinBox *byoyomiSec1;
    QLabel *label_13;
    QSpacerItem *horizontalSpacer_11;
    QLabel *label_8;
    QSpacerItem *horizontalSpacer_14;
    QHBoxLayout *horizontalLayout_16;
    QLabel *label_15;
    QSpinBox *addEachMoveSec1;
    QLabel *label_16;
    QSpacerItem *horizontalSpacer_12;
    QGroupBox *groupBoxSecondPlayerTimeSettings;
    QVBoxLayout *verticalLayout_6;
    QHBoxLayout *horizontalLayout_7;
    QLabel *label;
    QSpinBox *basicTimeHour2;
    QLabel *label_4;
    QSpinBox *basicTimeMinutes2;
    QLabel *label_5;
    QSpacerItem *horizontalSpacer_3;
    QHBoxLayout *horizontalLayout_9;
    QLabel *label_3;
    QSpinBox *byoyomiSec2;
    QLabel *label_7;
    QSpacerItem *horizontalSpacer_6;
    QHBoxLayout *horizontalLayout_8;
    QLabel *label_14;
    QSpinBox *addEachMoveSec2;
    QLabel *label_6;
    QSpacerItem *horizontalSpacer_7;
    QGroupBox *groupBox_5;
    QVBoxLayout *settingsOuterLayout;
    QHBoxLayout *settingsMainLayout;
    QVBoxLayout *verticalLayout_5;
    QHBoxLayout *horizontalLayout;
    QLabel *label_9;
    QComboBox *comboBoxStartingPosition;
    QSpacerItem *horizontalSpacer_8;
    QHBoxLayout *horizontalLayout_10;
    QLabel *label_17;
    QSpinBox *spinBoxMaxMoves;
    QSpacerItem *horizontalSpacer_5;
    QHBoxLayout *horizontalLayout_19;
    QLabel *label_19;
    QSpinBox *spinBoxConsecutiveGames;
    QSpacerItem *horizontalSpacer_17;
    QHBoxLayout *horizontalLayoutJishogi;
    QLabel *labelJishogi;
    QComboBox *comboBoxJishogi;
    QSpacerItem *horizontalSpacerJishogi;
    QVBoxLayout *verticalLayout;
    QCheckBox *checkBoxShowHumanInFront;
    QCheckBox *checkBoxLoseOnTimeOut;
    QCheckBox *checkBoxSwitchTurnEachGame;
    QSpacerItem *settingsRightSpacer;
    QHBoxLayout *horizontalLayoutAutoSaveKifu;
    QCheckBox *checkBoxAutoSaveKifu;
    QLineEdit *lineEditKifuSaveDir;
    QPushButton *pushButtonSelectKifuDir;
    QHBoxLayout *buttonLayout;
    QPushButton *pushButtonSwapSides;
    QPushButton *pushButtonSaveSettingsOnly;
    QPushButton *pushButtonResetToDefault;
    QPushButton *pushButtonFontSizeDown;
    QPushButton *pushButtonFontSizeUp;
    QSpacerItem *buttonSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *StartGameDialog)
    {
        if (StartGameDialog->objectName().isEmpty())
            StartGameDialog->setObjectName("StartGameDialog");
        StartGameDialog->resize(1000, 580);
        StartGameDialog->setMinimumSize(QSize(800, 300));
        mainLayout = new QVBoxLayout(StartGameDialog);
        mainLayout->setSpacing(8);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(10, 10, 10, 10);
        scrollArea = new QScrollArea(StartGameDialog);
        scrollArea->setObjectName("scrollArea");
        scrollArea->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 980, 520));
        scrollLayout = new QVBoxLayout(scrollAreaWidgetContents);
        scrollLayout->setSpacing(8);
        scrollLayout->setObjectName("scrollLayout");
        scrollLayout->setContentsMargins(0, 0, 0, 0);
        playerLayout = new QHBoxLayout();
        playerLayout->setSpacing(10);
        playerLayout->setObjectName("playerLayout");
        groupBox = new QGroupBox(scrollAreaWidgetContents);
        groupBox->setObjectName("groupBox");
        gridLayoutPlayer1 = new QGridLayout(groupBox);
        gridLayoutPlayer1->setObjectName("gridLayoutPlayer1");
        gridLayoutPlayer1->setHorizontalSpacing(6);
        gridLayoutPlayer1->setVerticalSpacing(4);
        labelPlayerType1 = new QLabel(groupBox);
        labelPlayerType1->setObjectName("labelPlayerType1");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(labelPlayerType1->sizePolicy().hasHeightForWidth());
        labelPlayerType1->setSizePolicy(sizePolicy);

        gridLayoutPlayer1->addWidget(labelPlayerType1, 0, 0, 1, 1);

        comboBoxPlayer1 = new QComboBox(groupBox);
        comboBoxPlayer1->setObjectName("comboBoxPlayer1");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(comboBoxPlayer1->sizePolicy().hasHeightForWidth());
        comboBoxPlayer1->setSizePolicy(sizePolicy1);
        comboBoxPlayer1->setMinimumSize(QSize(200, 0));

        gridLayoutPlayer1->addWidget(comboBoxPlayer1, 0, 1, 1, 1);

        stackedWidgetLabel1 = new QStackedWidget(groupBox);
        stackedWidgetLabel1->setObjectName("stackedWidgetLabel1");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(stackedWidgetLabel1->sizePolicy().hasHeightForWidth());
        stackedWidgetLabel1->setSizePolicy(sizePolicy2);
        pageLabelHuman1 = new QWidget();
        pageLabelHuman1->setObjectName("pageLabelHuman1");
        layoutLabelHuman1 = new QHBoxLayout(pageLabelHuman1);
        layoutLabelHuman1->setObjectName("layoutLabelHuman1");
        layoutLabelHuman1->setContentsMargins(0, 0, 0, 0);
        labelPlayer1 = new QLabel(pageLabelHuman1);
        labelPlayer1->setObjectName("labelPlayer1");

        layoutLabelHuman1->addWidget(labelPlayer1);

        stackedWidgetLabel1->addWidget(pageLabelHuman1);
        pageLabelEngine1 = new QWidget();
        pageLabelEngine1->setObjectName("pageLabelEngine1");
        layoutLabelEngine1 = new QHBoxLayout(pageLabelEngine1);
        layoutLabelEngine1->setObjectName("layoutLabelEngine1");
        layoutLabelEngine1->setContentsMargins(0, 0, 0, 0);
        stackedWidgetLabel1->addWidget(pageLabelEngine1);

        gridLayoutPlayer1->addWidget(stackedWidgetLabel1, 1, 0, 1, 1);

        stackedWidgetPlayer1 = new QStackedWidget(groupBox);
        stackedWidgetPlayer1->setObjectName("stackedWidgetPlayer1");
        sizePolicy1.setHeightForWidth(stackedWidgetPlayer1->sizePolicy().hasHeightForWidth());
        stackedWidgetPlayer1->setSizePolicy(sizePolicy1);
        pageHuman1 = new QWidget();
        pageHuman1->setObjectName("pageHuman1");
        layoutPageHuman1 = new QHBoxLayout(pageHuman1);
        layoutPageHuman1->setObjectName("layoutPageHuman1");
        layoutPageHuman1->setContentsMargins(0, 0, 0, 0);
        lineEditHumanName1 = new QLineEdit(pageHuman1);
        lineEditHumanName1->setObjectName("lineEditHumanName1");

        layoutPageHuman1->addWidget(lineEditHumanName1);

        stackedWidgetPlayer1->addWidget(pageHuman1);
        pageEngine1 = new QWidget();
        pageEngine1->setObjectName("pageEngine1");
        layoutPageEngine1 = new QHBoxLayout(pageEngine1);
        layoutPageEngine1->setObjectName("layoutPageEngine1");
        layoutPageEngine1->setContentsMargins(0, 0, 0, 0);
        pushButtonEngineSettings1 = new QPushButton(pageEngine1);
        pushButtonEngineSettings1->setObjectName("pushButtonEngineSettings1");

        layoutPageEngine1->addWidget(pushButtonEngineSettings1);

        horizontalSpacerEngine1 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        layoutPageEngine1->addItem(horizontalSpacerEngine1);

        stackedWidgetPlayer1->addWidget(pageEngine1);

        gridLayoutPlayer1->addWidget(stackedWidgetPlayer1, 1, 1, 1, 1);


        playerLayout->addWidget(groupBox);

        groupBox_2 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_2->setObjectName("groupBox_2");
        gridLayoutPlayer2 = new QGridLayout(groupBox_2);
        gridLayoutPlayer2->setObjectName("gridLayoutPlayer2");
        gridLayoutPlayer2->setHorizontalSpacing(6);
        gridLayoutPlayer2->setVerticalSpacing(4);
        labelPlayerType2 = new QLabel(groupBox_2);
        labelPlayerType2->setObjectName("labelPlayerType2");
        sizePolicy.setHeightForWidth(labelPlayerType2->sizePolicy().hasHeightForWidth());
        labelPlayerType2->setSizePolicy(sizePolicy);

        gridLayoutPlayer2->addWidget(labelPlayerType2, 0, 0, 1, 1);

        comboBoxPlayer2 = new QComboBox(groupBox_2);
        comboBoxPlayer2->setObjectName("comboBoxPlayer2");
        sizePolicy1.setHeightForWidth(comboBoxPlayer2->sizePolicy().hasHeightForWidth());
        comboBoxPlayer2->setSizePolicy(sizePolicy1);
        comboBoxPlayer2->setMinimumSize(QSize(200, 0));

        gridLayoutPlayer2->addWidget(comboBoxPlayer2, 0, 1, 1, 1);

        stackedWidgetLabel2 = new QStackedWidget(groupBox_2);
        stackedWidgetLabel2->setObjectName("stackedWidgetLabel2");
        sizePolicy2.setHeightForWidth(stackedWidgetLabel2->sizePolicy().hasHeightForWidth());
        stackedWidgetLabel2->setSizePolicy(sizePolicy2);
        pageLabelHuman2 = new QWidget();
        pageLabelHuman2->setObjectName("pageLabelHuman2");
        layoutLabelHuman2 = new QHBoxLayout(pageLabelHuman2);
        layoutLabelHuman2->setObjectName("layoutLabelHuman2");
        layoutLabelHuman2->setContentsMargins(0, 0, 0, 0);
        labelPlayer2 = new QLabel(pageLabelHuman2);
        labelPlayer2->setObjectName("labelPlayer2");

        layoutLabelHuman2->addWidget(labelPlayer2);

        stackedWidgetLabel2->addWidget(pageLabelHuman2);
        pageLabelEngine2 = new QWidget();
        pageLabelEngine2->setObjectName("pageLabelEngine2");
        layoutLabelEngine2 = new QHBoxLayout(pageLabelEngine2);
        layoutLabelEngine2->setObjectName("layoutLabelEngine2");
        layoutLabelEngine2->setContentsMargins(0, 0, 0, 0);
        stackedWidgetLabel2->addWidget(pageLabelEngine2);

        gridLayoutPlayer2->addWidget(stackedWidgetLabel2, 1, 0, 1, 1);

        stackedWidgetPlayer2 = new QStackedWidget(groupBox_2);
        stackedWidgetPlayer2->setObjectName("stackedWidgetPlayer2");
        sizePolicy1.setHeightForWidth(stackedWidgetPlayer2->sizePolicy().hasHeightForWidth());
        stackedWidgetPlayer2->setSizePolicy(sizePolicy1);
        pageHuman2 = new QWidget();
        pageHuman2->setObjectName("pageHuman2");
        layoutPageHuman2 = new QHBoxLayout(pageHuman2);
        layoutPageHuman2->setObjectName("layoutPageHuman2");
        layoutPageHuman2->setContentsMargins(0, 0, 0, 0);
        lineEditHumanName2 = new QLineEdit(pageHuman2);
        lineEditHumanName2->setObjectName("lineEditHumanName2");

        layoutPageHuman2->addWidget(lineEditHumanName2);

        stackedWidgetPlayer2->addWidget(pageHuman2);
        pageEngine2 = new QWidget();
        pageEngine2->setObjectName("pageEngine2");
        layoutPageEngine2 = new QHBoxLayout(pageEngine2);
        layoutPageEngine2->setObjectName("layoutPageEngine2");
        layoutPageEngine2->setContentsMargins(0, 0, 0, 0);
        pushButtonEngineSettings2 = new QPushButton(pageEngine2);
        pushButtonEngineSettings2->setObjectName("pushButtonEngineSettings2");

        layoutPageEngine2->addWidget(pushButtonEngineSettings2);

        horizontalSpacerEngine2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        layoutPageEngine2->addItem(horizontalSpacerEngine2);

        stackedWidgetPlayer2->addWidget(pageEngine2);

        gridLayoutPlayer2->addWidget(stackedWidgetPlayer2, 1, 1, 1, 1);


        playerLayout->addWidget(groupBox_2);


        scrollLayout->addLayout(playerLayout);

        timeSettingsLayout = new QHBoxLayout();
        timeSettingsLayout->setSpacing(10);
        timeSettingsLayout->setObjectName("timeSettingsLayout");
        groupBox_7 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_7->setObjectName("groupBox_7");
        verticalLayout_8 = new QVBoxLayout(groupBox_7);
        verticalLayout_8->setObjectName("verticalLayout_8");
        horizontalLayout_14 = new QHBoxLayout();
        horizontalLayout_14->setObjectName("horizontalLayout_14");
        label_2 = new QLabel(groupBox_7);
        label_2->setObjectName("label_2");
        label_2->setMinimumSize(QSize(60, 0));

        horizontalLayout_14->addWidget(label_2);

        basicTimeHour1 = new QSpinBox(groupBox_7);
        basicTimeHour1->setObjectName("basicTimeHour1");
        basicTimeHour1->setMinimumSize(QSize(60, 0));

        horizontalLayout_14->addWidget(basicTimeHour1);

        label_10 = new QLabel(groupBox_7);
        label_10->setObjectName("label_10");

        horizontalLayout_14->addWidget(label_10);

        basicTimeMinutes1 = new QSpinBox(groupBox_7);
        basicTimeMinutes1->setObjectName("basicTimeMinutes1");
        basicTimeMinutes1->setMinimumSize(QSize(60, 0));
        basicTimeMinutes1->setMaximum(59);

        horizontalLayout_14->addWidget(basicTimeMinutes1);

        label_11 = new QLabel(groupBox_7);
        label_11->setObjectName("label_11");

        horizontalLayout_14->addWidget(label_11);

        horizontalSpacer_10 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_14->addItem(horizontalSpacer_10);


        verticalLayout_8->addLayout(horizontalLayout_14);

        horizontalLayout_15 = new QHBoxLayout();
        horizontalLayout_15->setObjectName("horizontalLayout_15");
        label_12 = new QLabel(groupBox_7);
        label_12->setObjectName("label_12");
        label_12->setMinimumSize(QSize(60, 0));

        horizontalLayout_15->addWidget(label_12);

        byoyomiSec1 = new QSpinBox(groupBox_7);
        byoyomiSec1->setObjectName("byoyomiSec1");
        byoyomiSec1->setMinimumSize(QSize(60, 0));
        byoyomiSec1->setMaximum(59);

        horizontalLayout_15->addWidget(byoyomiSec1);

        label_13 = new QLabel(groupBox_7);
        label_13->setObjectName("label_13");

        horizontalLayout_15->addWidget(label_13);

        horizontalSpacer_11 = new QSpacerItem(20, 20, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        horizontalLayout_15->addItem(horizontalSpacer_11);

        label_8 = new QLabel(groupBox_7);
        label_8->setObjectName("label_8");

        horizontalLayout_15->addWidget(label_8);

        horizontalSpacer_14 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_15->addItem(horizontalSpacer_14);


        verticalLayout_8->addLayout(horizontalLayout_15);

        horizontalLayout_16 = new QHBoxLayout();
        horizontalLayout_16->setObjectName("horizontalLayout_16");
        label_15 = new QLabel(groupBox_7);
        label_15->setObjectName("label_15");
        label_15->setMinimumSize(QSize(60, 0));

        horizontalLayout_16->addWidget(label_15);

        addEachMoveSec1 = new QSpinBox(groupBox_7);
        addEachMoveSec1->setObjectName("addEachMoveSec1");
        addEachMoveSec1->setMinimumSize(QSize(60, 0));
        addEachMoveSec1->setMaximum(59);

        horizontalLayout_16->addWidget(addEachMoveSec1);

        label_16 = new QLabel(groupBox_7);
        label_16->setObjectName("label_16");

        horizontalLayout_16->addWidget(label_16);

        horizontalSpacer_12 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_16->addItem(horizontalSpacer_12);


        verticalLayout_8->addLayout(horizontalLayout_16);


        timeSettingsLayout->addWidget(groupBox_7);

        groupBoxSecondPlayerTimeSettings = new QGroupBox(scrollAreaWidgetContents);
        groupBoxSecondPlayerTimeSettings->setObjectName("groupBoxSecondPlayerTimeSettings");
        groupBoxSecondPlayerTimeSettings->setCheckable(true);
        groupBoxSecondPlayerTimeSettings->setChecked(false);
        verticalLayout_6 = new QVBoxLayout(groupBoxSecondPlayerTimeSettings);
        verticalLayout_6->setObjectName("verticalLayout_6");
        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        label = new QLabel(groupBoxSecondPlayerTimeSettings);
        label->setObjectName("label");
        label->setMinimumSize(QSize(60, 0));

        horizontalLayout_7->addWidget(label);

        basicTimeHour2 = new QSpinBox(groupBoxSecondPlayerTimeSettings);
        basicTimeHour2->setObjectName("basicTimeHour2");
        basicTimeHour2->setMinimumSize(QSize(60, 0));

        horizontalLayout_7->addWidget(basicTimeHour2);

        label_4 = new QLabel(groupBoxSecondPlayerTimeSettings);
        label_4->setObjectName("label_4");

        horizontalLayout_7->addWidget(label_4);

        basicTimeMinutes2 = new QSpinBox(groupBoxSecondPlayerTimeSettings);
        basicTimeMinutes2->setObjectName("basicTimeMinutes2");
        basicTimeMinutes2->setMinimumSize(QSize(60, 0));
        basicTimeMinutes2->setMaximum(59);

        horizontalLayout_7->addWidget(basicTimeMinutes2);

        label_5 = new QLabel(groupBoxSecondPlayerTimeSettings);
        label_5->setObjectName("label_5");

        horizontalLayout_7->addWidget(label_5);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_7->addItem(horizontalSpacer_3);


        verticalLayout_6->addLayout(horizontalLayout_7);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setObjectName("horizontalLayout_9");
        label_3 = new QLabel(groupBoxSecondPlayerTimeSettings);
        label_3->setObjectName("label_3");
        label_3->setMinimumSize(QSize(60, 0));

        horizontalLayout_9->addWidget(label_3);

        byoyomiSec2 = new QSpinBox(groupBoxSecondPlayerTimeSettings);
        byoyomiSec2->setObjectName("byoyomiSec2");
        byoyomiSec2->setMinimumSize(QSize(60, 0));
        byoyomiSec2->setMaximum(59);

        horizontalLayout_9->addWidget(byoyomiSec2);

        label_7 = new QLabel(groupBoxSecondPlayerTimeSettings);
        label_7->setObjectName("label_7");

        horizontalLayout_9->addWidget(label_7);

        horizontalSpacer_6 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_9->addItem(horizontalSpacer_6);


        verticalLayout_6->addLayout(horizontalLayout_9);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        label_14 = new QLabel(groupBoxSecondPlayerTimeSettings);
        label_14->setObjectName("label_14");
        label_14->setMinimumSize(QSize(60, 0));

        horizontalLayout_8->addWidget(label_14);

        addEachMoveSec2 = new QSpinBox(groupBoxSecondPlayerTimeSettings);
        addEachMoveSec2->setObjectName("addEachMoveSec2");
        addEachMoveSec2->setMinimumSize(QSize(60, 0));
        addEachMoveSec2->setMaximum(59);

        horizontalLayout_8->addWidget(addEachMoveSec2);

        label_6 = new QLabel(groupBoxSecondPlayerTimeSettings);
        label_6->setObjectName("label_6");

        horizontalLayout_8->addWidget(label_6);

        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_8->addItem(horizontalSpacer_7);


        verticalLayout_6->addLayout(horizontalLayout_8);


        timeSettingsLayout->addWidget(groupBoxSecondPlayerTimeSettings);


        scrollLayout->addLayout(timeSettingsLayout);

        groupBox_5 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_5->setObjectName("groupBox_5");
        settingsOuterLayout = new QVBoxLayout(groupBox_5);
        settingsOuterLayout->setSpacing(8);
        settingsOuterLayout->setObjectName("settingsOuterLayout");
        settingsMainLayout = new QHBoxLayout();
        settingsMainLayout->setObjectName("settingsMainLayout");
        verticalLayout_5 = new QVBoxLayout();
        verticalLayout_5->setObjectName("verticalLayout_5");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label_9 = new QLabel(groupBox_5);
        label_9->setObjectName("label_9");

        horizontalLayout->addWidget(label_9);

        comboBoxStartingPosition = new QComboBox(groupBox_5);
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->addItem(QString());
        comboBoxStartingPosition->setObjectName("comboBoxStartingPosition");

        horizontalLayout->addWidget(comboBoxStartingPosition);

        horizontalSpacer_8 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_8);


        verticalLayout_5->addLayout(horizontalLayout);

        horizontalLayout_10 = new QHBoxLayout();
        horizontalLayout_10->setObjectName("horizontalLayout_10");
        label_17 = new QLabel(groupBox_5);
        label_17->setObjectName("label_17");

        horizontalLayout_10->addWidget(label_17);

        spinBoxMaxMoves = new QSpinBox(groupBox_5);
        spinBoxMaxMoves->setObjectName("spinBoxMaxMoves");
        spinBoxMaxMoves->setMinimumSize(QSize(80, 0));
        spinBoxMaxMoves->setMaximum(9999);

        horizontalLayout_10->addWidget(spinBoxMaxMoves);

        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_10->addItem(horizontalSpacer_5);


        verticalLayout_5->addLayout(horizontalLayout_10);

        horizontalLayout_19 = new QHBoxLayout();
        horizontalLayout_19->setObjectName("horizontalLayout_19");
        label_19 = new QLabel(groupBox_5);
        label_19->setObjectName("label_19");

        horizontalLayout_19->addWidget(label_19);

        spinBoxConsecutiveGames = new QSpinBox(groupBox_5);
        spinBoxConsecutiveGames->setObjectName("spinBoxConsecutiveGames");
        spinBoxConsecutiveGames->setMinimumSize(QSize(80, 0));
        spinBoxConsecutiveGames->setMaximum(9999);

        horizontalLayout_19->addWidget(spinBoxConsecutiveGames);

        horizontalSpacer_17 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_19->addItem(horizontalSpacer_17);


        verticalLayout_5->addLayout(horizontalLayout_19);

        horizontalLayoutJishogi = new QHBoxLayout();
        horizontalLayoutJishogi->setObjectName("horizontalLayoutJishogi");
        labelJishogi = new QLabel(groupBox_5);
        labelJishogi->setObjectName("labelJishogi");

        horizontalLayoutJishogi->addWidget(labelJishogi);

        comboBoxJishogi = new QComboBox(groupBox_5);
        comboBoxJishogi->addItem(QString());
        comboBoxJishogi->addItem(QString());
        comboBoxJishogi->addItem(QString());
        comboBoxJishogi->setObjectName("comboBoxJishogi");
        comboBoxJishogi->setMinimumSize(QSize(80, 0));

        horizontalLayoutJishogi->addWidget(comboBoxJishogi);

        horizontalSpacerJishogi = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutJishogi->addItem(horizontalSpacerJishogi);


        verticalLayout_5->addLayout(horizontalLayoutJishogi);


        settingsMainLayout->addLayout(verticalLayout_5);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        checkBoxShowHumanInFront = new QCheckBox(groupBox_5);
        checkBoxShowHumanInFront->setObjectName("checkBoxShowHumanInFront");

        verticalLayout->addWidget(checkBoxShowHumanInFront);

        checkBoxLoseOnTimeOut = new QCheckBox(groupBox_5);
        checkBoxLoseOnTimeOut->setObjectName("checkBoxLoseOnTimeOut");

        verticalLayout->addWidget(checkBoxLoseOnTimeOut);

        checkBoxSwitchTurnEachGame = new QCheckBox(groupBox_5);
        checkBoxSwitchTurnEachGame->setObjectName("checkBoxSwitchTurnEachGame");

        verticalLayout->addWidget(checkBoxSwitchTurnEachGame);


        settingsMainLayout->addLayout(verticalLayout);

        settingsRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        settingsMainLayout->addItem(settingsRightSpacer);


        settingsOuterLayout->addLayout(settingsMainLayout);

        horizontalLayoutAutoSaveKifu = new QHBoxLayout();
        horizontalLayoutAutoSaveKifu->setSpacing(6);
        horizontalLayoutAutoSaveKifu->setObjectName("horizontalLayoutAutoSaveKifu");
        checkBoxAutoSaveKifu = new QCheckBox(groupBox_5);
        checkBoxAutoSaveKifu->setObjectName("checkBoxAutoSaveKifu");

        horizontalLayoutAutoSaveKifu->addWidget(checkBoxAutoSaveKifu);

        lineEditKifuSaveDir = new QLineEdit(groupBox_5);
        lineEditKifuSaveDir->setObjectName("lineEditKifuSaveDir");
        sizePolicy1.setHeightForWidth(lineEditKifuSaveDir->sizePolicy().hasHeightForWidth());
        lineEditKifuSaveDir->setSizePolicy(sizePolicy1);
        lineEditKifuSaveDir->setMinimumSize(QSize(200, 0));
        lineEditKifuSaveDir->setReadOnly(true);

        horizontalLayoutAutoSaveKifu->addWidget(lineEditKifuSaveDir);

        pushButtonSelectKifuDir = new QPushButton(groupBox_5);
        pushButtonSelectKifuDir->setObjectName("pushButtonSelectKifuDir");
        pushButtonSelectKifuDir->setMaximumSize(QSize(30, 16777215));

        horizontalLayoutAutoSaveKifu->addWidget(pushButtonSelectKifuDir);


        settingsOuterLayout->addLayout(horizontalLayoutAutoSaveKifu);


        scrollLayout->addWidget(groupBox_5);

        scrollArea->setWidget(scrollAreaWidgetContents);

        mainLayout->addWidget(scrollArea);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName("buttonLayout");
        pushButtonSwapSides = new QPushButton(StartGameDialog);
        pushButtonSwapSides->setObjectName("pushButtonSwapSides");

        buttonLayout->addWidget(pushButtonSwapSides);

        pushButtonSaveSettingsOnly = new QPushButton(StartGameDialog);
        pushButtonSaveSettingsOnly->setObjectName("pushButtonSaveSettingsOnly");

        buttonLayout->addWidget(pushButtonSaveSettingsOnly);

        pushButtonResetToDefault = new QPushButton(StartGameDialog);
        pushButtonResetToDefault->setObjectName("pushButtonResetToDefault");

        buttonLayout->addWidget(pushButtonResetToDefault);

        pushButtonFontSizeDown = new QPushButton(StartGameDialog);
        pushButtonFontSizeDown->setObjectName("pushButtonFontSizeDown");
        pushButtonFontSizeDown->setMinimumSize(QSize(40, 28));
        pushButtonFontSizeDown->setMaximumSize(QSize(40, 28));

        buttonLayout->addWidget(pushButtonFontSizeDown);

        pushButtonFontSizeUp = new QPushButton(StartGameDialog);
        pushButtonFontSizeUp->setObjectName("pushButtonFontSizeUp");
        pushButtonFontSizeUp->setMinimumSize(QSize(40, 28));
        pushButtonFontSizeUp->setMaximumSize(QSize(40, 28));

        buttonLayout->addWidget(pushButtonFontSizeUp);

        buttonSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(buttonSpacer);

        buttonBox = new QDialogButtonBox(StartGameDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        buttonLayout->addWidget(buttonBox);


        mainLayout->addLayout(buttonLayout);


        retranslateUi(StartGameDialog);

        stackedWidgetLabel1->setCurrentIndex(0);
        stackedWidgetPlayer1->setCurrentIndex(0);
        stackedWidgetLabel2->setCurrentIndex(0);
        stackedWidgetPlayer2->setCurrentIndex(0);
        comboBoxStartingPosition->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(StartGameDialog);
    } // setupUi

    void retranslateUi(QDialog *StartGameDialog)
    {
        StartGameDialog->setWindowTitle(QCoreApplication::translate("StartGameDialog", "\345\257\276\345\261\200", nullptr));
        groupBox->setTitle(QCoreApplication::translate("StartGameDialog", "\345\205\210\346\211\213\357\274\217\344\270\213\346\211\213", nullptr));
        labelPlayerType1->setText(QCoreApplication::translate("StartGameDialog", "\345\257\276\345\261\200\350\200\205", nullptr));
        labelPlayer1->setText(QCoreApplication::translate("StartGameDialog", "\345\220\215\345\211\215", nullptr));
        lineEditHumanName1->setText(QCoreApplication::translate("StartGameDialog", "You", nullptr));
        pushButtonEngineSettings1->setText(QCoreApplication::translate("StartGameDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("StartGameDialog", "\345\276\214\346\211\213\357\274\217\344\270\212\346\211\213", nullptr));
        labelPlayerType2->setText(QCoreApplication::translate("StartGameDialog", "\345\257\276\345\261\200\350\200\205", nullptr));
        labelPlayer2->setText(QCoreApplication::translate("StartGameDialog", "\345\220\215\345\211\215", nullptr));
        lineEditHumanName2->setText(QCoreApplication::translate("StartGameDialog", "You", nullptr));
        pushButtonEngineSettings2->setText(QCoreApplication::translate("StartGameDialog", "\343\202\250\343\203\263\343\202\270\343\203\263\350\250\255\345\256\232", nullptr));
        groupBox_7->setTitle(QCoreApplication::translate("StartGameDialog", "\346\231\202\351\226\223\350\250\255\345\256\232", nullptr));
        label_2->setText(QCoreApplication::translate("StartGameDialog", "\346\214\201\343\201\241\346\231\202\351\226\223", nullptr));
        label_10->setText(QCoreApplication::translate("StartGameDialog", "\346\231\202\351\226\223", nullptr));
        label_11->setText(QCoreApplication::translate("StartGameDialog", "\345\210\206", nullptr));
        label_12->setText(QCoreApplication::translate("StartGameDialog", "\347\247\222\350\252\255\343\201\277", nullptr));
        label_13->setText(QCoreApplication::translate("StartGameDialog", "\347\247\222", nullptr));
        label_8->setText(QCoreApplication::translate("StartGameDialog", "\342\200\273 \347\247\222\350\252\255\343\201\277\343\201\250\345\242\227\345\212\240\343\201\257\344\275\265\347\224\250\343\201\247\343\201\215\343\201\276\343\201\233\343\202\223", nullptr));
        label_15->setText(QCoreApplication::translate("StartGameDialog", "\345\242\227\345\212\240", nullptr));
        label_16->setText(QCoreApplication::translate("StartGameDialog", "\347\247\222", nullptr));
        groupBoxSecondPlayerTimeSettings->setTitle(QCoreApplication::translate("StartGameDialog", "\345\276\214\346\211\213\357\274\217\344\270\212\346\211\213\343\201\253\347\225\260\343\201\252\343\202\213\346\231\202\351\226\223\343\202\222\350\250\255\345\256\232", nullptr));
        label->setText(QCoreApplication::translate("StartGameDialog", "\346\214\201\343\201\241\346\231\202\351\226\223", nullptr));
        label_4->setText(QCoreApplication::translate("StartGameDialog", "\346\231\202\351\226\223", nullptr));
        label_5->setText(QCoreApplication::translate("StartGameDialog", "\345\210\206", nullptr));
        label_3->setText(QCoreApplication::translate("StartGameDialog", "\347\247\222\350\252\255\343\201\277", nullptr));
        label_7->setText(QCoreApplication::translate("StartGameDialog", "\347\247\222", nullptr));
        label_14->setText(QCoreApplication::translate("StartGameDialog", "\345\242\227\345\212\240", nullptr));
        label_6->setText(QCoreApplication::translate("StartGameDialog", "\347\247\222", nullptr));
        groupBox_5->setTitle(QCoreApplication::translate("StartGameDialog", "\350\250\255\345\256\232", nullptr));
        label_9->setText(QCoreApplication::translate("StartGameDialog", "\351\226\213\345\247\213\345\261\200\351\235\242", nullptr));
        comboBoxStartingPosition->setItemText(0, QCoreApplication::translate("StartGameDialog", "\347\217\276\345\234\250\343\201\256\345\261\200\351\235\242", nullptr));
        comboBoxStartingPosition->setItemText(1, QCoreApplication::translate("StartGameDialog", "\345\271\263\346\211\213", nullptr));
        comboBoxStartingPosition->setItemText(2, QCoreApplication::translate("StartGameDialog", "\351\246\231\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(3, QCoreApplication::translate("StartGameDialog", "\345\217\263\351\246\231\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(4, QCoreApplication::translate("StartGameDialog", "\350\247\222\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(5, QCoreApplication::translate("StartGameDialog", "\351\243\233\350\273\212\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(6, QCoreApplication::translate("StartGameDialog", "\351\243\233\351\246\231\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(7, QCoreApplication::translate("StartGameDialog", "\344\272\214\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(8, QCoreApplication::translate("StartGameDialog", "\344\270\211\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(9, QCoreApplication::translate("StartGameDialog", "\345\233\233\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(10, QCoreApplication::translate("StartGameDialog", "\344\272\224\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(11, QCoreApplication::translate("StartGameDialog", "\345\267\246\344\272\224\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(12, QCoreApplication::translate("StartGameDialog", "\345\205\255\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(13, QCoreApplication::translate("StartGameDialog", "\345\205\253\346\236\232\350\220\275\343\201\241", nullptr));
        comboBoxStartingPosition->setItemText(14, QCoreApplication::translate("StartGameDialog", "\345\215\201\346\236\232\350\220\275\343\201\241", nullptr));

        label_17->setText(QCoreApplication::translate("StartGameDialog", "\346\234\200\345\244\247\346\211\213\346\225\260", nullptr));
        label_19->setText(QCoreApplication::translate("StartGameDialog", "\351\200\243\347\266\232\345\257\276\345\261\200", nullptr));
        labelJishogi->setText(QCoreApplication::translate("StartGameDialog", "\346\214\201\345\260\206\346\243\213", nullptr));
        comboBoxJishogi->setItemText(0, QCoreApplication::translate("StartGameDialog", "\343\201\252\343\201\227", nullptr));
        comboBoxJishogi->setItemText(1, QCoreApplication::translate("StartGameDialog", "24\347\202\271\346\263\225", nullptr));
        comboBoxJishogi->setItemText(2, QCoreApplication::translate("StartGameDialog", "27\347\202\271\346\263\225", nullptr));

        checkBoxShowHumanInFront->setText(QCoreApplication::translate("StartGameDialog", "\344\272\272\343\202\222\346\211\213\345\211\215\343\201\253\350\241\250\347\244\272\343\201\231\343\202\213", nullptr));
        checkBoxLoseOnTimeOut->setText(QCoreApplication::translate("StartGameDialog", "\346\231\202\351\226\223\345\210\207\343\202\214\343\202\222\350\262\240\343\201\221\343\201\253\343\201\231\343\202\213", nullptr));
        checkBoxSwitchTurnEachGame->setText(QCoreApplication::translate("StartGameDialog", "1\345\261\200\343\201\224\343\201\250\343\201\253\346\211\213\347\225\252\343\202\222\345\205\245\343\202\214\346\233\277\343\201\210\343\202\213", nullptr));
        checkBoxAutoSaveKifu->setText(QCoreApplication::translate("StartGameDialog", "\346\243\213\350\255\234\350\207\252\345\213\225\344\277\235\345\255\230", nullptr));
        lineEditKifuSaveDir->setPlaceholderText(QCoreApplication::translate("StartGameDialog", "\344\277\235\345\255\230\345\205\210\343\202\222\351\201\270\346\212\236...", nullptr));
        pushButtonSelectKifuDir->setText(QCoreApplication::translate("StartGameDialog", "...", nullptr));
        pushButtonSwapSides->setText(QCoreApplication::translate("StartGameDialog", "\342\206\224 \345\205\210\345\276\214\345\205\245\343\202\214\346\233\277\343\201\210", nullptr));
        pushButtonSaveSettingsOnly->setText(QCoreApplication::translate("StartGameDialog", "\350\250\255\345\256\232\343\201\256\343\201\277\344\277\235\345\255\230", nullptr));
        pushButtonResetToDefault->setText(QCoreApplication::translate("StartGameDialog", "\345\210\235\346\234\237\350\250\255\345\256\232\343\201\253\346\210\273\343\201\231", nullptr));
        pushButtonFontSizeDown->setText(QCoreApplication::translate("StartGameDialog", "A-", nullptr));
        pushButtonFontSizeUp->setText(QCoreApplication::translate("StartGameDialog", "A+", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StartGameDialog: public Ui_StartGameDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STARTGAMEDIALOG_H
