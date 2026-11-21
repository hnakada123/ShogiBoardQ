/// @file p2psettingspanel.cpp
/// @brief 「設定」タブの実装

#include "p2psettingspanel.h"

#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "dialogutils.h"
#include "p2pidentitydialog.h"

P2pSettingsPanel::P2pSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // 以前はここにあった欄の行き先
    auto* movedRow = new QHBoxLayout;
    movedRow->addWidget(DialogUtils::wrappedLabel(tr("協力ノードと、相手とのつなぎ方（以前の「接続候補」「直接接続を使わない」）は、"
                                                     "「接続」タブで決めます。"),
                                                  this),
                        1);
    auto* openConnection = new QPushButton(tr("「接続」タブを開く"), this);
    openConnection->setObjectName(QStringLiteral("p2pOpenConnectionTab"));
    openConnection->setAutoDefault(false);
    connect(openConnection, &QPushButton::clicked, this, &P2pSettingsPanel::connectionTabRequested);
    movedRow->addWidget(openConnection, 0, Qt::AlignTop);
    layout->addLayout(movedRow);

    layout->addWidget(DialogUtils::headingLabel(tr("表示"), this));
    m_hideUnknown = new QCheckBox(tr("対局したことのない相手の表示名を出さない"), this);
    m_hideUnknown->setObjectName(QStringLiteral("p2pHideUnknownNames"));
    connect(m_hideUnknown, &QCheckBox::toggled, this, &P2pSettingsPanel::hideUnknownNamesToggled);
    layout->addWidget(m_hideUnknown);

    layout->addWidget(DialogUtils::headingLabel(tr("あなたのプレイヤーIDとプレイヤー鍵"), this));
    auto* identity = new QGridLayout;
    identity->setColumnStretch(1, 1);
    identity->addWidget(new QLabel(tr("プレイヤーID"), this), 0, 0, Qt::AlignLeft | Qt::AlignTop);
    m_playerId = DialogUtils::wrappedLabel(QString(), this);
    m_playerId->setObjectName(QStringLiteral("p2pPlayerId"));
    m_playerId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identity->addWidget(m_playerId, 0, 1);
    auto* copyId = new QPushButton(tr("コピー"), this);
    copyId->setObjectName(QStringLiteral("p2pCopyPlayerId"));
    copyId->setAutoDefault(false);
    connect(copyId, &QPushButton::clicked, this, &P2pSettingsPanel::copyPlayerIdRequested);
    identity->addWidget(copyId, 0, 2, Qt::AlignTop);
    m_identityInfo = DialogUtils::wrappedLabel(QString(), this);
    m_identityInfo->setObjectName(QStringLiteral("p2pIdentityInfo"));
    m_identityInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identity->addWidget(m_identityInfo, 1, 0, 1, 2);
    auto* recreate = new QPushButton(tr("プレイヤー鍵を作り直す…"), this);
    recreate->setObjectName(QStringLiteral("p2pRecreateIdentity"));
    recreate->setAutoDefault(false);
    connect(recreate, &QPushButton::clicked, this, &P2pSettingsPanel::recreateIdentityRequested);
    identity->addWidget(recreate, 1, 2, Qt::AlignTop);
    layout->addLayout(identity);

    m_buildInfo = DialogUtils::wrappedLabel(QString(), this);
    m_buildInfo->setObjectName(QStringLiteral("p2pBuildInfo"));
    layout->addWidget(m_buildInfo);

    // 防げないことは、見出しを付けて2列に並べる（1つの段落に続けて書くと、読み飛ばされる）
    auto* limits = new QFrame(this);
    limits->setObjectName(QStringLiteral("p2pKnownLimits"));
    limits->setFrameShape(QFrame::StyledPanel);
    auto* limitsLayout = new QGridLayout(limits);
    limitsLayout->setHorizontalSpacing(16);
    limitsLayout->setColumnStretch(0, 1);
    limitsLayout->setColumnStretch(1, 1);
    limitsLayout->addWidget(DialogUtils::headingLabel(tr("P2P対局で防げないこと"), limits), 0, 0, 1, 2);
    const QList<P2pIdentityDialog::KnownLimit> items = P2pIdentityDialog::knownLimits();
    for (int index = 0; index < items.size(); ++index) {
        auto* cell = new QVBoxLayout;
        cell->setSpacing(1);
        cell->addWidget(DialogUtils::headingLabel(items.at(index).title, limits));
        cell->addWidget(DialogUtils::wrappedLabel(items.at(index).text, limits));
        cell->addStretch();
        limitsLayout->addLayout(cell, 1 + index / 2, index % 2);
    }
    layout->addWidget(limits);
    layout->addStretch(1);
}

bool P2pSettingsPanel::hideUnknownNames() const { return m_hideUnknown->isChecked(); }

void P2pSettingsPanel::setHideUnknownNames(bool hide)
{
    const QSignalBlocker block(m_hideUnknown);
    m_hideUnknown->setChecked(hide);
}

void P2pSettingsPanel::setPlayerId(const QString& text)
{
    if (m_playerId->text() != text) m_playerId->setText(text);
}

void P2pSettingsPanel::setIdentityInfo(const QString& text)
{
    DialogUtils::setWrappedText(m_identityInfo, text);
}

void P2pSettingsPanel::setBuildInfo(const QString& text)
{
    DialogUtils::setWrappedText(m_buildInfo, text);
}
