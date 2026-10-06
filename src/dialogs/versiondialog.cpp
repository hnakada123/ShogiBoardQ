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
#include "dialogfontscale.h"
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

    // 各項目には表示する文書のファイル名を持たせる。
    auto* documents = new QComboBox(this);
    documents->setObjectName(QStringLiteral("licenseDocuments"));
    //: Bundled document filename. Use the translated NOTICE_<language>.md file.
    const QString notice = tr("NOTICE.md");
    documents->addItem(tr("ライセンス・著作権表示"), notice);
    documents->addItem(tr("GNU GPL v3"), QStringLiteral("GPL-3.0.txt"));
    documents->addItem(tr("GNU LGPL v3"), QStringLiteral("LGPL-3.0.txt"));
    documents->addItem(tr("ソースコードの入手方法"), sourceCodeDocument());
    // 配布版の licenses にだけある一覧は、文書があるときだけ選べるようにする。
    const QString directory = licenseDirectory();
    if (QFile::exists(directory + QStringLiteral("/QT-NOTICES.md"))) {
        documents->addItem(tr("Qt 内の第三者ライセンス一覧"), QStringLiteral("QT-NOTICES.md"));
    }
    if (QFile::exists(directory + QStringLiteral("/THIRD-PARTY-NOTICES.md"))) {
        documents->addItem(tr("同梱ライブラリのライセンス一覧"), QStringLiteral("THIRD-PARTY-NOTICES.md"));
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
    DialogFontScale::install(this, QStringLiteral("version"));
}

VersionDialog::~VersionDialog()
{
    AppSettings::setVersionDialogSize(size());
    AppSettings::setVersionDialogDocument(findChild<QComboBox*>(QStringLiteral("licenseDocuments"))->currentIndex());
}

QString VersionDialog::sourceCodeDocument()
{
    //: Bundled document filename. Use the translated SOURCE_CODE_<language>.md file.
    return tr("SOURCE_CODE.md");
}

void VersionDialog::showLicenseDocument(int index)
{
    const QString name = findChild<QComboBox*>(QStringLiteral("licenseDocuments"))->itemData(index).toString();
    if (name.isEmpty()) return;
    auto* browser = findChild<QTextBrowser*>(QStringLiteral("licenseBrowser"));
    QString document = QStringLiteral(":/licenses/") + name;
    const QString directory = licenseDirectory();
    if (!directory.isEmpty() && QFile::exists(directory + QLatin1Char('/') + name)) {
        document = directory + QLatin1Char('/') + name;
    }
    QFile file(document);
    if (!file.open(QIODevice::ReadOnly)) return;
    QString contents = QString::fromUtf8(file.readAll());
    if (name == sourceCodeDocument() && !directory.isEmpty()) {
        for (const auto& metadataName : {QStringLiteral("QT-SOURCE.json"), QStringLiteral("BUILD.json")}) {
            QFile metadata(directory + QLatin1Char('/') + metadataName);
            if (metadata.open(QIODevice::ReadOnly)) {
                contents += QStringLiteral("\n\n## %1\n\n```json\n%2\n```\n")
                                .arg(metadataName, QString::fromUtf8(metadata.readAll()));
            }
        }
    }
    if (document.endsWith(QStringLiteral(".md"))) browser->setMarkdown(contents);
    else browser->setPlainText(contents);
    browser->document()->setBaseUrl(directory.isEmpty()
                                       ? QUrl(QStringLiteral("qrc:/licenses/"))
                                       : QUrl::fromLocalFile(directory + QLatin1Char('/')));
}
