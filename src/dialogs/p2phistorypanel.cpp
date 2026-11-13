/// @file p2phistorypanel.cpp
/// @brief P2P対局の記録の一覧の実装

#include "p2phistorypanel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QVBoxLayout>

#include "boundedfile.h"
#include "dialogutils.h"
#include "networksettings.h"
#include "p2pevidenceverifier.h"
#include "p2pknownpeers.h"
#include "p2precordstore.h"
#include "p2pwindowtext.h"
#include "plaintooltip.h"

namespace {

enum Column { Date, Opponent, Side, Ending, Category, Result, ColumnCount };

std::optional<P2p::EvidenceReport> verifyFile(const QString& path)
{
    QByteArray bytes;
    if (BoundedFile::read(path, P2p::Evidence::kMaxBytes, bytes) != BoundedFile::Result::Ok) return std::nullopt;
    return P2p::EvidenceVerifier::verify(bytes);
}

} // namespace

P2pHistoryPanel::P2pHistoryPanel(P2pRecordStore* store, P2pKnownPeers* knownPeers, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_knownPeers(knownPeers)
{
    auto* layout = new QVBoxLayout(this);
    auto* note = new QLabel(tr("「記録の区分」は証跡を検証した結果です。双方で確定した記録だけが、双方が同じ結果に同意したことを示します。"
                               "記録は思考時間の正しさ・人間が指したこと・対局の日時を証明しません。"),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);
    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setObjectName(QStringLiteral("p2pRecordTable"));
    m_table->setHorizontalHeaderLabels({tr("終わった日時"), tr("相手（表示名）"), tr("あなた"), tr("終わり方"), tr("記録の区分"), tr("結果")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 列は中身と見出しが切れない幅に合わせ、最後の列は残りの幅まで伸ばす（resizeColumnsToContents は最後の列の伸びを外す）
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &P2pHistoryPanel::onSelectionChanged);
    layout->addWidget(m_table, 1);
    auto* buttons = new QHBoxLayout;
    m_verify = new QPushButton(tr("検証する"), this);
    m_verify->setObjectName(QStringLiteral("p2pRecordVerify"));
    m_export = new QPushButton(tr("証跡を書き出す…"), this);
    m_export->setObjectName(QStringLiteral("p2pRecordExport"));
    m_remove = new QPushButton(tr("記録を削除"), this);
    m_remove->setObjectName(QStringLiteral("p2pRecordRemove"));
    connect(m_verify, &QPushButton::clicked, this, &P2pHistoryPanel::onVerify);
    connect(m_export, &QPushButton::clicked, this, &P2pHistoryPanel::onExport);
    connect(m_remove, &QPushButton::clicked, this, &P2pHistoryPanel::onRemove);
    buttons->addWidget(m_verify);
    buttons->addWidget(m_export);
    buttons->addStretch();
    buttons->addWidget(m_remove);
    layout->addLayout(buttons);
    onSelectionChanged();
}

void P2pHistoryPanel::reload()
{
    const QByteArray selected = selectedGameId();
    m_table->setRowCount(0);
    if (!m_store) return;
    const QList<P2pRecordStore::Entry> entries = m_store->history();
    // 証跡の検証は重いので、前に検証したときから変わった記録だけを検証し直す
    QHash<QString, VerifiedFile> seen;
    for (const P2pRecordStore::Entry& entry : entries) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* date = new QTableWidgetItem(
            QDateTime::fromSecsSinceEpoch(static_cast<qint64>(entry.info.endedAt)).toString(QStringLiteral("yyyy/MM/dd HH:mm")));
        date->setData(Qt::UserRole, entry.evidencePath);
        date->setData(Qt::UserRole + 1, entry.gameId);
        m_table->setItem(row, Date, date);
        const std::optional<P2p::EvidenceReport> report = verifiedReport(entry.evidencePath, seen);
        QString opponent = entry.info.opponentDisplayName;
        const P2pKnownPeer* known = nullptr;
        if (report && m_knownPeers) {
            const QByteArray key = entry.info.mySide == P2p::kSideBlack ? report->whitePlayerKey : report->blackPlayerKey;
            known = m_knownPeers->find(key);
            if (known && !known->localName.isEmpty()) opponent = tr("%1（手元の名前: %2）").arg(opponent, known->localName);
        }
        // 対局したことのない相手（「相手の管理」で記録を削除した相手・証跡を読めない対局）の表示名を出さない設定では、
        // ツールチップにも出さない（条件確認画面と同じ規則）
        const bool hideName = m_hideUnknownNames && !known;
        auto* opponentItem = new QTableWidgetItem(hideName ? tr("名前は非表示") : opponent);
        if (!hideName) opponentItem->setToolTip(plainToolTip(opponent));
        m_table->setItem(row, Opponent, opponentItem);
        m_table->setItem(row, Side, new QTableWidgetItem(entry.info.mySide == P2p::kSideBlack ? tr("先手") : tr("後手")));
        m_table->setItem(row, Ending, new QTableWidgetItem(P2pWindowText::settlementLabel(entry.info.settlement)));
        if (report) {
            const QString category = report->isValid() ? P2pWindowText::categoryText(report->category) : tr("検証に失敗");
            m_table->setItem(row, Category, new QTableWidgetItem(category));
            QString result;
            if (report->result) result = P2pWindowText::outcomeText(report->result->outcome, report->result->cause);
            else if (report->terminal) result = P2pWindowText::outcomeText(report->terminal->outcome, report->terminal->cause);
            m_table->setItem(row, Result, new QTableWidgetItem(result));
        } else {
            m_table->setItem(row, Category, new QTableWidgetItem(tr("読めません")));
        }
        if (!selected.isEmpty() && entry.gameId == selected) m_table->selectRow(row);
    }
    m_verified = std::move(seen);
    onSelectionChanged();
}

std::optional<P2p::EvidenceReport> P2pHistoryPanel::verifiedReport(const QString& path,
                                                                   QHash<QString, VerifiedFile>& seen) const
{
    const QFileInfo file(path);
    const auto it = m_verified.constFind(path);
    if (it != m_verified.cend() && it->size == file.size() && it->modified == file.lastModified()) {
        seen.insert(path, *it);
        return it->report;
    }
    VerifiedFile verified{file.size(), file.lastModified(), verifyFile(path)};
    seen.insert(path, verified);
    return verified.report;
}

QString P2pHistoryPanel::selectedPath() const
{
    const int row = m_table->currentRow();
    return row < 0 || !m_table->item(row, Date) ? QString() : m_table->item(row, Date)->data(Qt::UserRole).toString();
}

QByteArray P2pHistoryPanel::selectedGameId() const
{
    const int row = m_table->currentRow();
    return row < 0 || !m_table->item(row, Date) ? QByteArray() : m_table->item(row, Date)->data(Qt::UserRole + 1).toByteArray();
}

void P2pHistoryPanel::onSelectionChanged()
{
    const bool selected = !selectedPath().isEmpty();
    for (QPushButton* b : {m_verify, m_export, m_remove}) b->setEnabled(selected);
}

void P2pHistoryPanel::onVerify()
{
    const std::optional<P2p::EvidenceReport> report = verifyFile(selectedPath());
    if (!report) {
        DialogUtils::warning(this, tr("検証"), tr("証跡ファイルを読めません。"));
        return;
    }
    const auto yesNo = [](bool ok) { return ok ? tr("はい") : tr("いいえ"); };
    QStringList lines;
    lines << tr("対局ID: %1").arg(QString::fromLatin1(report->gameId.toHex()));
    lines << tr("先手のプレイヤーID: %1").arg(P2pWindowText::playerIdText(report->blackPlayerKey));
    lines << tr("後手のプレイヤーID: %1").arg(P2pWindowText::playerIdText(report->whitePlayerKey));
    lines << tr("プレイヤー鍵による確定が正しい: %1").arg(yesNo(report->signaturesValid && report->blackStartSigned && report->whiteStartSigned));
    lines << tr("棋譜がルールに適合: %1").arg(yesNo(report->recordLegal));
    lines << tr("自己申告時間の計算が一致: %1").arg(yesNo(report->clockArithmeticValid));
    lines << tr("結果が棋譜から決まる: %1").arg(report->terminal ? P2pWindowText::outcomeText(report->terminal->outcome, report->terminal->cause)
                                                                 : tr("いいえ"));
    lines << tr("記録の区分: %1").arg(P2pWindowText::categoryText(report->category));
    if (report->result) lines << tr("結果の識別子: %1").arg(QString::fromLatin1(report->resultDigest.toHex()));
    if (report->resultConflictsWithRecord) lines << tr("確定した結果が、棋譜から決まる結果と食い違っています。");
    if (!report->error.isEmpty()) lines << tr("問題: %1").arg(report->error);
    lines << tr("実際の思考時間・人間が指したこと・対局の日時は、この記録からは確かめられません。");
    DialogUtils::information(this, tr("検証"), lines.join(QLatin1Char('\n')));
}

void P2pHistoryPanel::onExport()
{
    const QString source = selectedPath();
    if (source.isEmpty()) return;
    // 証跡には双方のプレイヤーID と、対局を始めたおおよその日時が入る。人に渡すと、それがその人に伝わる
    if (!DialogUtils::confirmAction(this, tr("証跡を書き出す"),
                                    tr("証跡には、あなたと相手のプレイヤーIDと、対局を始めたおおよその日時が入っています。証跡を人に"
                                       "渡すと、それがその人に伝わります。書き出しますか？"),
                                    tr("書き出す"))) {
        return;
    }
    const QString dir = NetworkSettings::p2pExportDirectory().isEmpty() ? QDir::homePath() : NetworkSettings::p2pExportDirectory();
    const QString target = QFileDialog::getSaveFileName(this, tr("証跡を書き出す"), QDir(dir).filePath(QFileInfo(source).fileName()),
                                                        tr("ShogiBoardQ の証跡 (*.sbqmatch)"));
    if (target.isEmpty()) return;
    NetworkSettings::p2pExportDirectory.set(QFileInfo(target).absolutePath());
    // 書き出し先が元の証跡そのものなら、何もしない（消してから写すと、証跡を失う）
    const QFileInfo targetInfo(target);
    if (targetInfo.exists() && targetInfo.canonicalFilePath() == QFileInfo(source).canonicalFilePath()) return;
    // 一時ファイルに書いてから置き換える（失敗しても、書き出し先にあったファイルを壊さない）
    QFile in(source);
    QSaveFile out(target);
    const bool ok = in.open(QIODevice::ReadOnly) && out.open(QIODevice::WriteOnly)
        && out.write(in.readAll()) == in.size() && out.commit();
    if (!ok) DialogUtils::warning(this, tr("証跡を書き出す"), tr("書き出せませんでした。"));
}

void P2pHistoryPanel::onRemove()
{
    const QByteArray gameId = selectedGameId();
    if (gameId.isEmpty() || !m_store) return;
    if (!DialogUtils::confirmAction(this, tr("記録を削除"),
                                    tr("この対局の証跡と記録を削除しますか？ 削除した記録は戻せません。"), tr("削除"),
                                    QMessageBox::Warning)) {
        return;
    }
    if (!m_store->removeRecord(gameId)) DialogUtils::warning(this, tr("記録を削除"), tr("削除できないファイルがありました。"));
    reload();
}
