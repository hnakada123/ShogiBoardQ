/// @file josekiimportdialog.cpp
/// @brief 複数の棋譜ファイル・フォルダから定跡に登録するダイアログの実装

#include "josekiimportdialog.h"

#include "dialogfontscale.h"
#include "josekibooksession.h"
#include "josekisettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QProgressBar>
#include <QPromise>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace {

/// ワーカースレッドで、ファイル・フォルダから棋譜を集めて読み、登録する手を集める
void runImport(QPromise<JosekiImport::Result> &promise, const QStringList &paths, const JosekiImport::Options &options)
{
    const QStringList files = JosekiImport::expandPaths(paths);
    promise.setProgressRange(0, static_cast<int>(files.size()));
    JosekiImport::Result result = JosekiImport::collect(files, options, [&promise](int done) {
        promise.setProgressValue(done);
        return !promise.isCanceled();
    });
    promise.addResult(std::move(result));
}

} // namespace

JosekiImportDialog::JosekiImportDialog(JosekiBookSession *session, const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_session(session)
{
    Q_ASSERT(m_session);
    setWindowTitle(tr("複数の棋譜から定跡に登録"));
    setAcceptDrops(true);
    auto *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(tr("棋譜ファイルとフォルダ（フォルダはサブフォルダの棋譜も読みます。ここへドロップしても足せます）:"),
                                     this));
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("josekiImportFiles"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(m_list, 1);

    auto *listButtons = new QHBoxLayout;
    m_addFiles = new QPushButton(tr("ファイルを追加..."), this);
    m_addFiles->setObjectName(QStringLiteral("josekiImportAddFiles"));
    m_addFolder = new QPushButton(tr("フォルダを追加..."), this);
    m_addFolder->setObjectName(QStringLiteral("josekiImportAddFolder"));
    m_remove = new QPushButton(tr("削除"), this);
    m_remove->setObjectName(QStringLiteral("josekiImportRemove"));
    m_clear = new QPushButton(tr("すべて削除"), this);
    m_clear->setObjectName(QStringLiteral("josekiImportClear"));
    for (QPushButton *button : {m_addFiles, m_addFolder, m_remove, m_clear}) listButtons->addWidget(button);
    listButtons->addStretch();
    mainLayout->addLayout(listButtons);

    auto *form = new QFormLayout;
    m_maxPly = new QSpinBox(this);
    m_maxPly->setObjectName(QStringLiteral("josekiImportMaxPly"));
    m_maxPly->setRange(1, 9999);
    m_maxPly->setSuffix(tr(" 手目まで"));
    form->addRow(tr("登録する手数:"), m_maxPly);
    m_sides = new QComboBox(this);
    m_sides->setObjectName(QStringLiteral("josekiImportSides"));
    // 並びは JosekiImport::Sides と同じ
    m_sides->addItem(tr("先手・後手の両方"));
    m_sides->addItem(tr("先手の手だけ"));
    m_sides->addItem(tr("後手の手だけ"));
    m_sides->addItem(tr("勝った側の手だけ"));
    m_sides->setToolTip(tr("「勝った側の手だけ」は、投了・詰み・時間切れなどで勝敗の付いた棋譜だけを使います"));
    form->addRow(tr("登録する手:"), m_sides);
    m_variations = new QCheckBox(tr("変化の手も登録する"), this);
    m_variations->setObjectName(QStringLiteral("josekiImportVariations"));
    form->addRow(QString(), m_variations);
    m_minCount = new QSpinBox(this);
    m_minCount->setObjectName(QStringLiteral("josekiImportMinCount"));
    m_minCount->setRange(1, 999999);
    m_minCount->setSuffix(tr(" 局以上"));
    m_minCount->setToolTip(tr("並べた棋譜のうち、この局数以上で指された手だけを登録します"));
    form->addRow(tr("最小出現回数:"), m_minCount);
    m_comments = new QCheckBox(tr("棋譜のコメントを定跡のコメントにする（コメントの無い定跡手だけ）"), this);
    m_comments->setObjectName(QStringLiteral("josekiImportComments"));
    form->addRow(QString(), m_comments);
    mainLayout->addLayout(form);

    auto *note = new QLabel(tr("新しい手は予想応手なし・評価値0・深さ0で登録し、出現頻度は指された局数にします。"
                               "登録済みの手は出現頻度に局数を足します。登録しても定跡ファイルには保存しないので、"
                               "定跡ウィンドウで保存してください。"),
                            this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("josekiImportProgress"));
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    mainLayout->addWidget(m_progress);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("josekiImportStatus"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(this);
    m_start = buttons->addButton(tr("登録"), QDialogButtonBox::AcceptRole);
    m_start->setObjectName(QStringLiteral("josekiImportStart"));
    m_cancel = buttons->addButton(tr("中止"), QDialogButtonBox::ActionRole);
    m_cancel->setObjectName(QStringLiteral("josekiImportCancel"));
    m_close = buttons->addButton(QDialogButtonBox::Close);
    mainLayout->addWidget(buttons);

    // ボタンの役割の既定の動き（accept で閉じる）は使わず、押されたボタンごとに処理する
    connect(m_start, &QPushButton::clicked, this, &JosekiImportDialog::start);
    connect(m_cancel, &QPushButton::clicked, this, &JosekiImportDialog::cancel);
    connect(m_close, &QPushButton::clicked, this, &JosekiImportDialog::reject);
    connect(m_addFiles, &QPushButton::clicked, this, &JosekiImportDialog::addFiles);
    connect(m_addFolder, &QPushButton::clicked, this, &JosekiImportDialog::addFolder);
    connect(m_remove, &QPushButton::clicked, this, &JosekiImportDialog::removeSelected);
    connect(m_clear, &QPushButton::clicked, this, &JosekiImportDialog::clearPaths);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &JosekiImportDialog::updateInputs);
    connect(&m_watcher, &QFutureWatcher<JosekiImport::Result>::progressRangeChanged,
            this, &JosekiImportDialog::onProgressRangeChanged);
    connect(&m_watcher, &QFutureWatcher<JosekiImport::Result>::progressValueChanged,
            this, &JosekiImportDialog::onProgressValueChanged);
    connect(&m_watcher, &QFutureWatcher<JosekiImport::Result>::finished, this, &JosekiImportDialog::onFinished);

    m_binding.add(m_maxPly, JosekiSettings::josekiImportMaxPly);
    m_binding.add(m_sides, JosekiSettings::josekiImportSides);
    m_binding.add(m_variations, JosekiSettings::josekiImportVariations);
    m_binding.add(m_minCount, JosekiSettings::josekiImportMinCount);
    m_binding.add(m_comments, JosekiSettings::josekiImportComments);
    m_binding.load();
    appendPaths(paths);
    updateInputs();

    DialogFontScale::Options fontOptions;
    fontOptions.size = &JosekiSettings::josekiImportDialogSize;
    DialogFontScale::install(this, JosekiSettings::josekiImportDialogFontSize, fontOptions);
}

JosekiImportDialog::~JosekiImportDialog()
{
    if (isRunning()) {
        m_watcher.cancel();
        m_watcher.waitForFinished();
    }
    m_binding.save();
}

QStringList JosekiImportDialog::localPaths(const QMimeData *mimeData)
{
    QStringList paths;
    if (!mimeData || !mimeData->hasUrls()) return paths;
    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    return paths;
}

QStringList JosekiImportDialog::paths() const
{
    QStringList paths;
    for (int i = 0; i < m_list->count(); ++i) paths.append(m_list->item(i)->text());
    return paths;
}

JosekiImport::Options JosekiImportDialog::options() const
{
    JosekiImport::Options options;
    options.maxPly = m_maxPly->value();
    options.sides = static_cast<JosekiImport::Sides>(qBound(0, m_sides->currentIndex(), 3));
    options.includeVariations = m_variations->isChecked();
    options.minCount = m_minCount->value();
    options.useComments = m_comments->isChecked();
    return options;
}

bool JosekiImportDialog::isRunning() const
{
    return m_watcher.isRunning();
}

void JosekiImportDialog::appendPaths(const QStringList &paths)
{
    for (const QString &path : paths) {
        const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
        if (m_list->findItems(native, Qt::MatchExactly).isEmpty()) m_list->addItem(native);
    }
    updateInputs();
}

void JosekiImportDialog::addFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("棋譜ファイルを追加"), JosekiSettings::josekiImportLastDirectory(),
        tr("棋譜ファイル (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.json *.usi *.usen);;すべてのファイル (*)"));
    if (files.isEmpty()) return;
    JosekiSettings::josekiImportLastDirectory.set(QFileInfo(files.constFirst()).absolutePath());
    appendPaths(files);
}

void JosekiImportDialog::addFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("棋譜のフォルダを追加"),
                                                             JosekiSettings::josekiImportLastDirectory());
    if (folder.isEmpty()) return;
    JosekiSettings::josekiImportLastDirectory.set(folder);
    appendPaths({folder});
}

void JosekiImportDialog::removeSelected()
{
    qDeleteAll(m_list->selectedItems());
    updateInputs();
}

void JosekiImportDialog::clearPaths()
{
    m_list->clear();
    updateInputs();
}

void JosekiImportDialog::start()
{
    if (isRunning() || m_list->count() == 0) return;
    m_binding.save();
    m_progress->setRange(0, 0);
    m_status->setText(tr("棋譜を探しています..."));
    m_status->setToolTip(QString());
    m_watcher.setFuture(QtConcurrent::run(&runImport, paths(), options()));
    updateInputs();
}

void JosekiImportDialog::cancel()
{
    if (isRunning()) m_watcher.cancel();
}

void JosekiImportDialog::reject()
{
    // 集めている最中に閉じたら止める（棋譜1つを読み終わるまで待つ）
    if (isRunning()) {
        m_watcher.cancel();
        m_watcher.waitForFinished();
    }
    QDialog::reject();
}

void JosekiImportDialog::onProgressRangeChanged(int minimum, int maximum)
{
    m_progress->setRange(minimum, qMax(minimum + 1, maximum));
}

void JosekiImportDialog::onProgressValueChanged(int value)
{
    m_progress->setValue(value);
    m_status->setText(tr("棋譜を読んでいます: %1 / %2").arg(value).arg(m_progress->maximum()));
}

void JosekiImportDialog::onFinished()
{
    const bool canceled = m_watcher.isCanceled() || m_watcher.future().resultCount() == 0;
    const JosekiImport::Result result = canceled ? JosekiImport::Result{} : m_watcher.result();
    m_progress->setRange(0, 1);
    m_progress->setValue(canceled || result.canceled ? 0 : 1);
    updateInputs();
    if (canceled || result.canceled) {
        m_status->setText(tr("中止しました（定跡は変えていません）"));
        return;
    }
    if (m_session->isBusy()) {
        m_status->setText(tr("定跡の読み書き中のため、登録できませんでした"));
        return;
    }
    const JosekiBookSession::ImportStats stats = m_session->importMoves(result.moves);
    // 登録済みの手が無ければ、出現頻度を増やした手の数は書かない
    QString text = stats.updated > 0 ? tr("棋譜 %1 局から、定跡手を %2 手足し、%3 手の出現頻度を増やしました。")
                                           .arg(result.gameCount).arg(stats.added).arg(stats.updated)
                                     : tr("棋譜 %1 局から、定跡手を %2 手足しました。").arg(result.gameCount).arg(stats.added);
    if (!result.failedFiles.isEmpty()) text += QLatin1Char('\n') + tr("読めなかった棋譜: %1 件").arg(result.failedFiles.size());
    if (stats.added + stats.updated > 0) text += QLatin1Char('\n') + tr("定跡ウィンドウで保存してください。");
    m_status->setText(text);
    m_status->setToolTip(result.failedFiles.join(QLatin1Char('\n')));
}

void JosekiImportDialog::updateInputs()
{
    const bool running = isRunning();
    for (QWidget *input : std::initializer_list<QWidget *>{m_list, m_addFiles, m_addFolder, m_maxPly, m_sides,
                                                           m_variations, m_minCount, m_comments}) {
        input->setEnabled(!running);
    }
    m_remove->setEnabled(!running && !m_list->selectedItems().isEmpty());
    m_clear->setEnabled(!running && m_list->count() > 0);
    m_start->setEnabled(!running && m_list->count() > 0);
    m_cancel->setEnabled(running);
}

void JosekiImportDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (!isRunning() && !localPaths(event->mimeData()).isEmpty()) event->acceptProposedAction();
}

void JosekiImportDialog::dropEvent(QDropEvent *event)
{
    const QStringList dropped = localPaths(event->mimeData());
    if (isRunning() || dropped.isEmpty()) return;
    appendPaths(dropped);
    event->acceptProposedAction();
}
