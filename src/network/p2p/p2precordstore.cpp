/// @file p2precordstore.cpp
/// @brief P2P対局の保存先の実装

#include "p2precordstore.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>

#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#endif

#include "logcategories.h"
#include "p2precordkinds.h"

namespace {

using P2p::CborValue;

constexpr QFileDevice::Permissions kOwnerFile = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
constexpr QFileDevice::Permissions kOwnerDir = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
constexpr int kChecksumBytes = 4;
/// 履歴の情報のファイルの上限
constexpr qint64 kMaxInfoBytes = 4096;

QByteArray checksumOf(const QByteArray& payload)
{
    return P2p::Cbor::sha256(payload).left(kChecksumBytes);
}

QByteArray bigEndian32(quint32 value)
{
    QByteArray out(4, '\0');
    for (int i = 0; i < 4; ++i) out[i] = static_cast<char>((value >> (24 - 8 * i)) & 0xFFu);
    return out;
}

quint32 readBigEndian32(const QByteArray& bytes, qsizetype offset)
{
    quint32 value = 0;
    for (int i = 0; i < 4; ++i) value = (value << 8) | static_cast<quint8>(bytes.at(offset + i));
    return value;
}

bool syncToDisk(QFile& file)
{
    if (!file.flush()) return false;
#ifdef Q_OS_WIN
    return _commit(file.handle()) == 0;
#else
    return ::fsync(file.handle()) == 0;
#endif
}

QByteArray infoBytes(const P2pHistoryInfo& info)
{
    return P2p::Cbor::encode(CborValue::array({CborValue::text("SBQ-HISTORY"), CborValue::uint(1),
                                               CborValue::uint(info.mySide), CborValue::text(info.opponentDisplayName),
                                               CborValue::text(P2p::settlementName(info.settlement)), CborValue::uint(info.endedAt),
                                               CborValue::text(info.kifuPath)}));
}

std::optional<P2pHistoryInfo> parseInfo(const QByteArray& bytes)
{
    const std::optional<CborValue> value = P2p::Cbor::decode(bytes);
    if (!value) return std::nullopt;
    P2p::CborReader r(*value, "SBQ-HISTORY", 7);
    P2pHistoryInfo info;
    info.mySide = static_cast<quint8>(r.uint(2, 2));
    info.opponentDisplayName = r.utf8(3, 96);
    info.settlement = P2p::settlementFromName(QString::fromLatin1(r.ascii(4, 64))).value_or(P2p::Settlement::None);
    info.endedAt = r.uint(5, 0xFFFFFFFFFFULL);
    info.kifuPath = r.utf8(6, 4096);
    if (!r.ok()) return std::nullopt;
    return info;
}

bool writeAtomically(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(kOwnerFile);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace

// ---------------------------------------------------------------- P2pJournal

bool P2pJournal::open(const QString& path)
{
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) return false;
    m_file.setPermissions(kOwnerFile);
    return true;
}

bool P2pJournal::append(const CborValue& record)
{
    if (!m_file.isOpen()) return false;
    const QByteArray payload = P2p::Cbor::encode(record);
    if (m_file.size() + payload.size() + 8 > kMaxBytes) return false;
    QByteArray bytes = bigEndian32(static_cast<quint32>(payload.size()));
    bytes.append(checksumOf(payload));
    bytes.append(payload);
    if (m_file.write(bytes) != bytes.size()) return false;
    return syncToDisk(m_file);
}

std::vector<CborValue> P2pJournal::readAll(const QString& path)
{
    std::vector<CborValue> records;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return records;
    const QByteArray data = file.read(kMaxBytes + 1);
    qsizetype pos = 0;
    while (data.size() - pos >= 8) {
        const quint32 length = readBigEndian32(data, pos);
        if (length == 0 || static_cast<qint64>(length) > kMaxBytes || data.size() - pos - 8 < static_cast<qsizetype>(length)) break;
        const QByteArray checksum = data.mid(pos + 4, kChecksumBytes);
        const QByteArray payload = data.mid(pos + 8, static_cast<qsizetype>(length));
        if (checksumOf(payload) != checksum) break;
        std::optional<CborValue> value = P2p::Cbor::decode(payload);
        if (!value) break;
        records.push_back(std::move(*value));
        pos += 8 + static_cast<qsizetype>(length);
    }
    return records;
}

// ---------------------------------------------------------------- P2pRecordStore

P2pRecordStore::P2pRecordStore(const QString& root)
    : m_root(root)
{
}

P2pRecordStore::~P2pRecordStore()
{
    unlock();
}

QString P2pRecordStore::identityDirectory() const
{
    return QDir(m_root).filePath(QStringLiteral("identity"));
}

QString P2pRecordStore::journalDirectory() const
{
    return QDir(m_root).filePath(QStringLiteral("journal"));
}

QString P2pRecordStore::evidenceDirectory() const
{
    return QDir(m_root).filePath(QStringLiteral("evidence"));
}

bool P2pRecordStore::ensureDirectory(const QString& path) const
{
    if (!QDir().mkpath(path)) return false;
    QFile::setPermissions(m_root, kOwnerDir);
    QFile::setPermissions(path, kOwnerDir);
    return true;
}

bool P2pRecordStore::lock()
{
    if (isLocked()) return true;
    if (!ensureDirectory(m_root)) return false;
    m_lock = std::make_unique<QLockFile>(QDir(m_root).filePath(QStringLiteral("p2p.lock")));
    // 前のアプリが異常終了して残したロックは、そのプロセスがもう無ければ取り直す
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0)) {
        m_lock.reset();
        return false;
    }
    // 以前の版が残した既知ピアのキャッシュ（ほかの参加者の IP アドレスが入る）は、もう使わないので消す
    QFile::remove(QDir(m_root).filePath(QStringLiteral("peers.cache")));
    return true;
}

void P2pRecordStore::unlock()
{
    if (m_lock) m_lock->unlock();
    m_lock.reset();
}

std::unique_ptr<P2pJournal> P2pRecordStore::createJournal(const QByteArray& gameId)
{
    if (!ensureDirectory(journalDirectory())) return nullptr;
    auto journal = std::make_unique<P2pJournal>();
    const QString path = QDir(journalDirectory()).filePath(QString::fromLatin1(gameId.toHex()) + QStringLiteral(".journal"));
    if (!journal->open(path)) return nullptr;
    return journal;
}

QString P2pRecordStore::saveEvidence(const P2p::Evidence& evidence, const P2pHistoryInfo& info, bool keepJournal)
{
    if (!ensureDirectory(evidenceDirectory())) return QString();
    const QString base = QString::fromLatin1(evidence.gameId().toHex());
    const QString path = QDir(evidenceDirectory()).filePath(base + QStringLiteral(".") + QLatin1String(P2p::Evidence::kSuffix));
    if (!writeAtomically(path, evidence.encode())) return QString();
    if (!writeAtomically(QDir(evidenceDirectory()).filePath(base + QStringLiteral(".info")), infoBytes(info))) {
        return QString();
    }
    if (!keepJournal) QFile::remove(QDir(journalDirectory()).filePath(base + QStringLiteral(".journal")));
    return path;
}

int P2pRecordStore::recoverJournals()
{
    int recovered = 0;
    const QDir dir(journalDirectory());
    const QStringList files = dir.entryList({QStringLiteral("*.journal")}, QDir::Files);
    for (const QString& name : files) {
        const QString path = dir.filePath(name);
        const std::optional<P2p::JournalReplay> replay = P2p::RecordKinds::replay(P2pJournal::readAll(path));
        if (!replay) continue;
        P2pHistoryInfo info;
        info.mySide = replay->mySide;
        info.opponentDisplayName = replay->opponentDisplayName;
        info.settlement = replay->settlement.value_or(P2p::Settlement::Interrupted);
        info.endedAt = static_cast<quint64>(QFileInfo(path).lastModified().toSecsSinceEpoch());
        if (!saveEvidence(replay->evidence, info, replay->hasConflictProof).isEmpty()) ++recovered;
    }
    if (recovered > 0) qCInfo(lcGame) << "P2P: recovered" << recovered << "journal(s) as unconfirmed records";
    return recovered;
}

QList<P2pRecordStore::Entry> P2pRecordStore::history() const
{
    QList<Entry> entries;
    const QDir dir(evidenceDirectory());
    const QFileInfoList files =
        dir.entryInfoList({QStringLiteral("*.") + QLatin1String(P2p::Evidence::kSuffix)}, QDir::Files, QDir::Time);
    for (const QFileInfo& file : files) {
        Entry entry;
        entry.gameId = QByteArray::fromHex(file.completeBaseName().toLatin1());
        if (entry.gameId.size() != P2p::kHashSize) continue;
        entry.evidencePath = file.absoluteFilePath();
        entry.bytes = file.size();
        QFile infoFile(dir.filePath(file.completeBaseName() + QStringLiteral(".info")));
        if (infoFile.open(QIODevice::ReadOnly)) {
            if (const std::optional<P2pHistoryInfo> info = parseInfo(infoFile.read(kMaxInfoBytes))) entry.info = *info;
        }
        entry.hasJournal = QFile::exists(QDir(journalDirectory()).filePath(file.completeBaseName() + QStringLiteral(".journal")));
        entries.append(entry);
    }
    return entries;
}

bool P2pRecordStore::removeRecord(const QByteArray& gameId)
{
    const QString base = QString::fromLatin1(gameId.toHex());
    bool ok = true;
    for (const QString& path : {QDir(evidenceDirectory()).filePath(base + QStringLiteral(".") + QLatin1String(P2p::Evidence::kSuffix)),
                                QDir(evidenceDirectory()).filePath(base + QStringLiteral(".info")),
                                QDir(journalDirectory()).filePath(base + QStringLiteral(".journal"))}) {
        if (QFile::exists(path) && !QFile::remove(path)) ok = false;
    }
    return ok;
}

qint64 P2pRecordStore::totalBytes() const
{
    qint64 total = 0;
    QDirIterator it(m_root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}
