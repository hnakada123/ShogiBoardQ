/// @file kifudocument.cpp
/// @brief 開いている棋譜の文書の実装

#include "kifudocument.h"
#include "timecontroltext.h"

#include "gameinfo.h"
#include "gameinfokeys.h"
#include "handicaps.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifusavefile.h"
#include "sfenutils.h"

#include <QPair>
#include <QSet>
#include <algorithm>

namespace {

/// 対局を始めたときの対局情報（対局日・開始日時・対局者・手合割・持ち時間）
QList<KifGameInfoItem> matchStartItems(const QDateTime& start, const MatchPlayers& players, const QString& startSfen,
                                       const TimeControl& timeControl)
{
    const bool handicap = GameInfo::isHandicap(startSfen);
    QList<KifGameInfoItem> items;
    items.append({GameInfoKeys::kGameDate, start.toString(QStringLiteral("yyyy/MM/dd"))});
    items.append({GameInfoKeys::kStartDateTime, start.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))});
    // 先手・後手（駒落ちは柿木形式の KIF と同じく下手・上手）
    for (const Turn side : {Turn::Black, Turn::White})
        items.append({GameInfo::playerKey(side, handicap), players.name(side)});
    items.append({GameInfoKeys::kHandicap, GameInfo::handicapLabel(startSfen)});
    // 時間制限のない対局は「無制限」と書き、未開始の「未設定」（空）と区別する
    items.append({GameInfoKeys::kTimeControl, TimeControlText::format(timeControl, handicap)});
    return items;
}

/// 対局の自動の項目に、前の対局情報のうち利用者が入力した項目（棋戦・場所・備考など）を続ける
QList<KifGameInfoItem> mergeForMatch(const QList<KifGameInfoItem>& automaticItems,
                                     const QList<KifGameInfoItem>& current)
{
    QList<KifGameInfoItem> items = automaticItems;
    QSet<QString> replacedKeys = {GameInfoKeys::kEndDateTime, GameInfoKeys::kConsumedTime, GameInfoKeys::kResult};
    for (const auto& item : automaticItems) replacedKeys.insert(item.key);
    QSet<QString> retainedKeys;
    for (const auto& item : current) {
        const QString key = item.key.trimmed();
        // 対局者の見出しは平手（先手・後手）と駒落ち（下手・上手）で変わるため、前の対局の行を残さない
        if (key.isEmpty() || replacedKeys.contains(key) || GameInfo::playerSide(key)) continue;
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
            {GameInfoKeys::kBlackPlayer, {}},
            {GameInfoKeys::kWhitePlayer, {}},
            {GameInfoKeys::kHandicap, Handicaps::labelOf(Handicaps::kHirate)},
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
    // 残す対局情報には、対局情報欄で入力中のセルも入れる
    if (keepHeader) commitPendingEdits();
    m_tree->setRootSfen(sfen);
    finishReplacement(keepHeader ? m_header : defaultHeader(), true);
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
    emit replaced();
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
            if (const std::optional<Turn> side = GameInfo::playerSide(item.key))
                names.append({item.key, players.name(*side)});
        }
        for (const auto& name : std::as_const(names)) setHeaderValue(name.first, name.second);
        return;
    }

    commitPendingEdits();
    const QList<KifGameInfoItem> header =
        mergeForMatch(matchStartItems(m_timeRecord.startDateTime(), players, startSfen(), timeControl), m_header);
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

void KifuDocument::commitPendingEdits() const
{
    for (const Hooks& editor : m_editors)
        if (editor.commitPendingEdits) editor.commitPendingEdits();
}

QList<KifGameInfoItem> KifuDocument::header() const
{
    return m_header;
}

void KifuDocument::editHeader(const QList<KifGameInfoItem>& items)
{
    if (items == m_header) return;
    m_header = items;
    emit headerEdited();
}

void KifuDocument::setHeaderValue(const QString& key, const QString& value)
{
    setValueIn(m_header, key, value);
    setValueIn(m_saved.header, key, value);
    emit headerValueSet(key, value);
}

QString KifuDocument::headerPlayerName(Turn side) const
{
    return GameInfo::playerName(header(), side);
}

bool KifuDocument::usesHandicapNames() const
{
    return GameInfo::usesHandicapNames(header(), startSfen());
}

// ============================================================
// コメント・しおり
// ============================================================

void KifuDocument::setComment(KifuBranchNode* node, const QString& comment)
{
    if (!node || node->comment() == comment) return;
    node->mutableRecord().comment = comment;
    ++m_annotationRevision;
    emit commentChanged(node->nodeId(), comment);
}

void KifuDocument::setBookmark(KifuBranchNode* node, const QString& bookmark)
{
    if (!node || node->bookmark() == bookmark) return;
    node->mutableRecord().bookmark = bookmark;
    ++m_annotationRevision;
    emit bookmarkChanged(node->nodeId(), bookmark);
}

// ============================================================
// ファイル
// ============================================================

bool KifuDocument::isModified() const
{
    // 入力中の編集は、確定せずに未保存に数える（確定するのは保存・書き出し・対局の開始のとき）
    for (const Hooks& editor : m_editors)
        if (editor.hasPendingEdits && editor.hasPendingEdits()) return true;
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
    commitPendingEdits();
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
