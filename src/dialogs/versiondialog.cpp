/// @file versiondialog.cpp
/// @brief バージョンダイアログクラスの実装

#include <QPixmap>
#include <QCoreApplication>
#include <QFile>
#include <QTextDocument>
#include <QTextBrowser>
#include <QComboBox>
#include <QLabel>
#include "appsettings.h"
#include "versiondialog.h"
#include "ui_versiondialog.h"

namespace {
QString licenseDirectory()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList directories{appDir + QStringLiteral("/../Resources/licenses"),
                                 appDir + QStringLiteral("/licenses"),
                                 appDir + QStringLiteral("/../share/licenses/ShogiBoardQ")};
    for (const auto& directory : directories) {
        if (QFile::exists(directory + QStringLiteral("/NOTICE.md"))) return directory;
    }
    return {};
}
}

// バージョンを表示するダイアログ
VersionDialog::VersionDialog(QWidget *parent) : QDialog(parent), ui(std::make_unique<Ui::VersionDialog>())
{
    ui->setupUi(this);

    QPixmap icon(":/icons/shogiboardq.png");
    ui->iconLabel->setPixmap(icon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    ui->versionLabel->setText(QStringLiteral("Version ") + QStringLiteral(APP_VERSION));
    ui->buildDateLabel->setText(tr("ビルド日時: %1 %2").arg(__DATE__, __TIME__));

    auto* qtNotice = new QLabel(tr("Qt %1 を使用（ビルド時: %2）\n"
                                  "Copyright © The Qt Company Ltd. and other contributors.\n"
                                  "Qt Charts: GPLv3 / その他の Qt: LGPLv3・GPL")
                                   .arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)), this);
    qtNotice->setWordWrap(true);
    qtNotice->setAlignment(Qt::AlignCenter);
    ui->mainLayout->insertWidget(ui->mainLayout->count() - 1, qtNotice);

    auto* documents = new QComboBox(this);
    documents->setObjectName(QStringLiteral("licenseDocuments"));
    documents->addItems({tr("ライセンス・著作権表示"), tr("GNU GPL v3"),
                         tr("GNU LGPL v3"), tr("ソースコードの入手方法")});
    if (QFile::exists(licenseDirectory() + QStringLiteral("/QT-NOTICES.md"))) {
        documents->addItem(tr("Qt 内の第三者ライセンス一覧"));
    }
    documents->setAccessibleName(tr("ライセンス文書"));
    auto* browser = new QTextBrowser(this);
    browser->setObjectName(QStringLiteral("licenseBrowser"));
    browser->setOpenExternalLinks(true);
    browser->setMinimumHeight(200);
    ui->mainLayout->insertWidget(ui->mainLayout->count() - 1, documents);
    ui->mainLayout->insertWidget(ui->mainLayout->count() - 1, browser, 1);
    connect(documents, &QComboBox::currentIndexChanged, this, &VersionDialog::showLicenseDocument);
    showLicenseDocument(0);
    const int savedDocument = AppSettings::versionDialogDocument();
    if (savedDocument >= 0 && savedDocument < documents->count()) documents->setCurrentIndex(savedDocument);
    resize(AppSettings::versionDialogSize());
}

VersionDialog::~VersionDialog()
{
    AppSettings::setVersionDialogSize(size());
    AppSettings::setVersionDialogDocument(findChild<QComboBox*>(QStringLiteral("licenseDocuments"))->currentIndex());
}

void VersionDialog::showLicenseDocument(int index)
{
    static const QStringList names{QStringLiteral("NOTICE.md"), QStringLiteral("GPL-3.0.txt"),
                                   QStringLiteral("LGPL-3.0.txt"), QStringLiteral("SOURCE_CODE.md"),
                                   QStringLiteral("QT-NOTICES.md")};
    if (index < 0 || index >= names.size()) return;
    auto* browser = findChild<QTextBrowser*>(QStringLiteral("licenseBrowser"));
    QString document = QStringLiteral(":/licenses/") + names.at(index);
    const QString directory = licenseDirectory();
    if (!directory.isEmpty() && QFile::exists(directory + QLatin1Char('/') + names.at(index))) {
        document = directory + QLatin1Char('/') + names.at(index);
    }
    QFile file(document);
    if (!file.open(QIODevice::ReadOnly)) return;
    QString contents = QString::fromUtf8(file.readAll());
    if (index == 3 && !directory.isEmpty()) {
        for (const auto& name : {QStringLiteral("QT-SOURCE.json"), QStringLiteral("BUILD.json")}) {
            QFile metadata(directory + QLatin1Char('/') + name);
            if (metadata.open(QIODevice::ReadOnly)) {
                contents += QStringLiteral("\n\n## %1\n\n```json\n%2\n```\n")
                                .arg(name, QString::fromUtf8(metadata.readAll()));
            }
        }
    }
    if (document.endsWith(QStringLiteral(".md"))) browser->setMarkdown(contents);
    else browser->setPlainText(contents);
    browser->document()->setBaseUrl(directory.isEmpty()
                                       ? QUrl(QStringLiteral("qrc:/licenses/"))
                                       : QUrl::fromLocalFile(directory + QLatin1Char('/')));
}
