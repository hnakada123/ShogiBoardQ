/// @file kifudocument.cpp
/// @brief 開いている棋譜の文書の実装

#include "kifudocument.h"

#include "gameinfokeys.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifuexportmetadata.h"
#include "kifusavefile.h"
#include "sfenutils.h"

#include <QPair>
#include <QSet>
#include <algorithm>

namespace {

/// 対局を始めたときの対局情報（対局日・開始日時・対局者・手合割・持ち時間）
QList<KifGameInfoItem> matchStartItems(const QDateTime& start, const QString& blackName, const QString& whiteName,
                                       const QString& handicap, const TimeControl& timeControl)
{
    const QString handicapLabel = handicap.isEmpty() ? QStringLiteral("平手") : handicap;
    QList<KifGameInfoItem> items;
    items.append({GameInfoKeys::kGameDate, start.toString(QStringLiteral("yyyy/MM/dd"))});
    items.append({GameInfoKeys::kStartDateTime, start.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))});
    // 先手・後手（駒落ちは柿木形式の KIF と同じく下手・上手）
    items.append({KifuExportMetadataBuilder::blackPlayerKey(handicapLabel), blackName});
    items.append({KifuExportMetadataBuilder::whitePlayerKey(handicapLabel), whiteName});
    items.append({GameInfoKeys::kHandicap, handicapLabel});
    // 未開始の「未設定」と、時間制限のない対局を区別する
    items.append({GameInfoKeys::kTimeControl,
                  timeControl.isUnlimited() ? QStringLiteral("無制限")
                                            : KifuExportMetadataBuilder::timeControlText(
                                                  timeControl, KifuExportMetadataBuilder::isHandicap(handicapLabel))});
    return items;
}

/// 対局の自動の項目に、前の対局情報のうち利用者が入力した項目（棋戦・場所・備考など）を続ける
QList<KifGameInfoItem> mergeForMatch(const QList<KifGameInfoItem>& automaticItems,
                                     const QList<KifGameInfoItem>& current)
{
    QList<KifGameInfoItem> items = automaticItems;
    // 対局者の見出しは平手（先手・後手）と駒落ち（下手・上手）で変わるため、前の対局の行を残さない
    QSet<QString> replacedKeys = {GameInfoKeys::kEndDateTime, QStringLiteral("消費時間"), QStringLiteral("結果"),
                                  GameInfoKeys::kBlackPlayer, GameInfoKeys::kWhitePlayer,
                                  GameInfoKeys::kShitatePlayer, GameInfoKeys::kUwatePlayer};
    for (const auto& item : automaticItems) replacedKeys.insert(item.key);
    QSet<QString> retainedKeys;
    for (const auto& item : current) {
        const QString key = item.key.trimmed();
        if (key.isEmpty() || replacedKeys.contains(key)) continue;
        items.append(item);
        retainedKeys.insert(key);
    }
    for (const auto& key : {GameInfoKeys::kEvent, GameInfoKeys::kSite, GameInfoKeys::kNote}) {
        if (!retainedKeys.contains(key)) items.append({key, {}});
    }
    return items;
}

void setValueIn(QList<KifGameInfoItem>& items, const QString& key, const QString& value)
{
    const auto it = std::find_if(items.begin(), items.end(), [&key](const KifGameInfoItem& item) { return item.key == key; });
    if (it == items.end()) items.append({key, value});
    else it->value = value;
}

} // namespace

KifuDocument::KifuDocument(KifuBranchTree* tree, QObject* parent)
    : QObject(parent)
    , m_tree(tree)
    , m_header(defaultHeader())
{
    Q_ASSERT(m_tree);
    m_saved = {m_tree->revision(), m_annotationRevision, m_header};
}

QString KifuDocument::startSfen() const
{
    const KifuBranchNode* root = m_tree->root();
    return root ? root->sfen() : SfenUtils::hirateSfen();
}

QList<KifGameInfoItem> KifuDocument::defaultHeader()
{
    return {{GameInfoKeys::kGameDate, {}},
            {GameInfoKeys::kStartDateTime, {}},
            {GameInfoKeys::kBlackPlayer, tr("先手")},
            {GameInfoKeys::kWhitePlayer, tr("後手")},
            {GameInfoKeys::kHandicap, QStringLiteral("平手")},
            {GameInfoKeys::kTimeControl, {}},
            {GameInfoKeys::kEvent, {}},
            {GameInfoKeys::kSite, {}},
            {GameInfoKeys::kNote, {}}};
}

// ============================================================
// 置き換え
// ============================================================

void KifuDocument::startNew(const QString& sfen, bool keepHeader)
{
    m_tree->setRootSfen(sfen);
    finishReplacement(keepHeader ? header() : defaultHeader(), true);
}

bool KifuDocument::replaceWithRecord(const KifParseResult& record, const QList<KifGameInfoItem>& header)
{
    if (!KifuBranchTreeBuilder::buildFromKifParseResult(m_tree, record)) return false;
    finishReplacement(header, false);
    return true;
}

void KifuDocument::replaceWithPosition(const QString& sfen)
{
    m_tree->setRootSfen(sfen);
    finishReplacement({}, false);
}

void KifuDocument::finishReplacement(const QList<KifGameInfoItem>& header, bool beforeMatchStart)
{
    m_header = header;
    m_players = {};
    m_timeRecord.clear();
    m_keepRecordHeader = false;
    m_filePath.clear();
    m_unsavedContent = false;
    m_saved = {m_tree->revision(), m_annotationRevision, m_header};
    emit headerReset(m_header, beforeMatchStart);
    emit playersChanged();
}

// ============================================================
// 対局
// ============================================================

void KifuDocument::beginMatch(const MatchPlayers& players, const TimeControl& timeControl, bool continuesRecord)
{
    m_players = players;
    m_timeRecord.begin(timeControl);
    m_keepRecordHeader = continuesRecord;
    emit playersChanged();
    if (continuesRecord) {
        // 棋譜の対局情報は残し、対局者名だけを今の対局者にする（駒落ちの見出し下手・上手も同じ）
        QList<QPair<QString, QString>> names;
        for (const auto& item : std::as_const(m_header)) {
            if (item.key == GameInfoKeys::kBlackPlayer || item.key == GameInfoKeys::kShitatePlayer)
                names.append({item.key, players.name(Turn::Black)});
            else if (item.key == GameInfoKeys::kWhitePlayer || item.key == GameInfoKeys::kUwatePlayer)
                names.append({item.key, players.name(Turn::White)});
        }
        for (const auto& name : std::as_const(names)) setHeaderValue(name.first, name.second);
        return;
    }

    flushPendingHeaderEdits();
    const QString handicap = KifuExportMetadataBuilder::handicapLabel(startSfen());
    const QList<KifGameInfoItem> header =
        mergeForMatch(matchStartItems(m_timeRecord.startDateTime(), players.name(Turn::Black),
                                      players.name(Turn::White), handicap, timeControl),
                      m_header);
    // 利用者が編集していなければ、作り直した対局情報も変更に数えない
    if (m_header == m_saved.header) m_saved.header = header;
    m_header = header;
    emit headerReset(m_header, false);
}

void KifuDocument::recordMatchEnd()
{
    m_timeRecord.recordEnd();
    if (!m_keepRecordHeader)
        setHeaderValue(GameInfoKeys::kEndDateTime, m_timeRecord.endDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));
}

void KifuDocument::clearMatchEnd()
{
    m_timeRecord.clearEnd();
    if (!m_keepRecordHeader) setHeaderValue(GameInfoKeys::kEndDateTime, QString());
}

// ============================================================
// 対局情報
// ============================================================

void KifuDocument::flushPendingHeaderEdits() const
{
    if (m_hooks.flushPendingHeaderEdits) m_hooks.flushPendingHeaderEdits();
}

void KifuDocument::commitPendingEdits() const
{
    flushPendingHeaderEdits();
    if (m_hooks.commitPendingComment) m_hooks.commitPendingComment();
}

QList<KifGameInfoItem> KifuDocument::header() const
{
    flushPendingHeaderEdits();
    return m_header;
}

void KifuDocument::editHeader(const QList<KifGameInfoItem>& items)
{
    if (items == m_header) return;
    m_header = items;
}

void KifuDocument::setHeaderValue(const QString& key, const QString& value)
{
    setValueIn(m_header, key, value);
    setValueIn(m_saved.header, key, value);
    emit headerValueSet(key, value);
}

QString KifuDocument::headerPlayerName(Turn side) const
{
    return KifuExportMetadataBuilder::playerName(header(), side);
}

bool KifuDocument::usesHandicapNames() const
{
    return KifuExportMetadataBuilder::usesHandicapNames(header(), startSfen());
}

// ============================================================
// コメント・しおり
// ============================================================

void KifuDocument::setComment(KifuBranchNode* node, const QString& comment)
{
    if (!node || node->comment() == comment) return;
    node->record().comment = comment;
    ++m_annotationRevision;
    emit commentChanged(node->nodeId(), comment);
}

void KifuDocument::setBookmark(KifuBranchNode* node, const QString& bookmark)
{
    if (!node || node->bookmark() == bookmark) return;
    node->record().bookmark = bookmark;
    ++m_annotationRevision;
    emit bookmarkChanged(node->nodeId(), bookmark);
}

// ============================================================
// ファイル
// ============================================================

bool KifuDocument::isModified() const
{
    flushPendingHeaderEdits();
    // コメント欄の未更新の本文は、確定せずに未保存に数える（確定するのは保存・書き出しのとき）
    if (m_hooks.hasPendingComment && m_hooks.hasPendingComment()) return true;
    if (m_unsavedContent || m_tree->revision() != m_saved.treeRevision
        || m_annotationRevision != m_saved.annotationRevision) {
        return true;
    }
    // 指し手もコメントもない棋譜の対局情報だけの変更（対局の前に棋戦・場所を書いた）は、保存を促さない
    return m_header != m_saved.header && hasRecordContent();
}

bool KifuDocument::hasRecordContent() const
{
    const KifuBranchNode* root = m_tree->root();
    return !m_filePath.isEmpty() || (root && (root->childCount() > 0 || !root->comment().isEmpty()));
}

void KifuDocument::markSaved(const QString& filePath)
{
    flushPendingHeaderEdits();
    m_saved = {m_tree->revision(), m_annotationRevision, m_header};
    m_unsavedContent = false;
    // 保存形式を拡張子で決められるファイルだけを上書き保存先にする。
    // それ以外（.sfen や不明な拡張子）は「上書き保存」で名前を付けて保存へ誘導する
    m_filePath = KifuSaveFile::hasKnownSaveExtension(filePath) ? filePath : QString();
    emit saved();
}

void KifuDocument::markUnsavedContent()
{
    m_unsavedContent = true;
    m_filePath.clear();
}
