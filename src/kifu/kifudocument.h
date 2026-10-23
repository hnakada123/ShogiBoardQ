#ifndef KIFUDOCUMENT_H
#define KIFUDOCUMENT_H

/// @file kifudocument.h
/// @brief 開いている棋譜の文書（分岐ツリー・対局情報・対局の記録・ファイルの紐づけ）

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

#include "kifparsetypes.h"
#include "matchplayers.h"
#include "matchtimerecord.h"
#include "shogitypes.h"
#include "timecontrol.h"

class KifuBranchNode;
class KifuBranchTree;

/**
 * @brief 開いている棋譜の文書
 *
 * 書き出しと保存に使う情報をまとめて持つ。
 * - 分岐ツリー（棋譜の正本。オブジェクトは合成ルートが持ち、文書は中身を置き換える）
 * - 対局情報（ヘッダー。対局情報欄はこれを編集する画面）
 * - 対局の記録（対局者・持ち時間・開始と終了の日時）
 * - ファイルの紐づけ（上書き保存先と、保存した時点の状態）
 *
 * 未保存かどうかは「ツリーの revision・コメントの変更・対局情報が保存した時点と違う」で求める
 * （変更のたびに印を付けない）。指し手のない棋譜の対局情報だけの変更は未保存に数えない。読み込み・新規・対局の開始は文書の中身を丸ごと置き換える操作なので、
 * 前の棋譜の対局者・持ち時間・上書き保存先が残らない。
 */
class KifuDocument : public QObject
{
    Q_OBJECT

public:
    /// 上位層へのコールバック（文書を編集する画面1つの、まだ文書に入っていない入力）
    struct Hooks {
        /// 入力中の編集（対局情報欄のセル・コメント欄の未更新の本文）を文書に確定する
        /// （保存・書き出し・対局の開始の前に呼ぶ）
        std::function<void()> commitPendingEdits;
        /// 文書に入っていない入力中の編集があるか（未保存に数える）
        std::function<bool()> hasPendingEdits;
    };

    explicit KifuDocument(KifuBranchTree* tree, QObject* parent = nullptr);

    /// 文書を編集する画面を足す（対局情報欄・コメント欄。確定と「入力中か」は足した画面すべてに聞く）
    void addPendingEditor(Hooks hooks) { m_editors.append(std::move(hooks)); }
    /// 画面で入力中の内容（対局情報欄のセル・コメント欄の未更新の本文）を文書に確定する。保存・書き出しの前に呼ぶ
    void commitPendingEdits() const;

    KifuBranchTree* tree() const { return m_tree; }
    /// 開始局面（ツリーが空なら平手）
    QString startSfen() const;

    // --- 置き換え（前の棋譜の対局情報・対局の記録・保存先は残さない） ---

    /// 局面 sfen だけの新しい棋譜にする（新規・局面編集・通信対局の開始）。
    /// 対局情報は既定の項目にする（keepHeader なら今の対局情報を残す）
    void startNew(const QString& sfen, bool keepHeader = false);
    /// 読み込んだ棋譜にする。分岐元が不正ならツリーを変えずに false
    [[nodiscard]] bool replaceWithRecord(const KifParseResult& record, const QList<KifGameInfoItem>& header);
    /// 局面だけの棋譜にする（SFEN・BOD の読み込み）。対局情報は空
    void replaceWithPosition(const QString& sfen);

    // --- 対局 ---

    /**
     * @brief 対局を始める（対局者と持ち時間を記録し、開始日時を今にする）
     * @param continuesRecord 読み込んだ棋譜の途中から続ける対局（対局情報は棋譜のまま残す）
     *
     * 続ける対局でなければ、対局情報を対局日・開始日時・対局者・手合割・持ち時間で作り直す
     * （棋戦・場所・備考など利用者が入力した項目は残す）。続ける対局は対局者名だけを書き換える。
     */
    void beginMatch(const MatchPlayers& players, const TimeControl& timeControl, bool continuesRecord);
    /// 終局した（終了日時を記録して対局情報に書く）
    void recordMatchEnd();
    /// 終了日時を消す（中断した対局を再開した）
    void clearMatchEnd();

    // --- 対局情報（ヘッダー） ---

    /// 対局情報（入力中のセルは含まない。保存・書き出しは commitPendingEdits で確定してから読む）
    QList<KifGameInfoItem> header() const;
    /// 利用者が対局情報を編集した（保存した時点と違えば未保存）
    void editHeader(const QList<KifGameInfoItem>& items);
    /// 対局情報の対局者名（先手・後手。駒落ちは下手・上手。なければ省略名、それもなければ空）
    QString headerPlayerName(Turn side) const;
    /// 対局者を下手・上手と呼ぶか（駒落ちで、対局情報の見出しが下手・上手のとき）
    bool usesHandicapNames() const;

    // --- 対局の記録 ---

    /// 最後に始めた対局の対局者（対局していなければ空）
    const MatchPlayers& players() const { return m_players; }
    /// 持ち時間と開始・終了の日時
    const MatchTimeRecord& timeRecord() const { return m_timeRecord; }

    // --- コメント・しおり ---

    /// node のコメントを書き換える（変わったら未保存にして commentChanged を出す）
    void setComment(KifuBranchNode* node, const QString& comment);
    /// node のしおりを書き換える（変わったら未保存にして bookmarkChanged を出す）
    void setBookmark(KifuBranchNode* node, const QString& bookmark);

    // --- ファイル ---

    /// 上書き保存先（保存形式を拡張子で決められるファイルだけ。なければ空）
    QString filePath() const { return m_filePath; }
    /// 保存していない変更があるか
    bool isModified() const;
    /// ファイルに保存した・ファイルから読み込んだ（今の内容を保存済みにし、上書き保存先にする）
    void markSaved(const QString& filePath);
    /// ファイルにない内容を読み込んだ（貼り付けた棋譜・反映した局面。上書き保存先はない）
    void markUnsavedContent();

    /// 新しい棋譜の対局情報の項目（対局者は「先手」「後手」、ほかは未設定）
    static QList<KifGameInfoItem> defaultHeader();

signals:
    /// 文書の中身を置き換えた（新規・読み込み・局面の反映・新しい棋譜での対局の開始）。
    /// ツリーから作られない表示（評価値グラフ・エンジンの表示）は、これを受けて自分で消す
    void replaced();
    /// 対局情報を置き換えた（beforeMatchStart: 対局を始める前の案内を出す）
    void headerReset(const QList<KifGameInfoItem>& items, bool beforeMatchStart);
    /// 対局情報欄で対局情報を書き換えた（盤の対局者名を合わせる）
    void headerEdited();
    /// 対局情報に自動で値を書いた（終了日時など。未保存に数えない）
    void headerValueSet(const QString& key, const QString& value);
    /// 保存した・ファイルから読み込んだ
    void saved();
    /// 対局者が変わった（対局の開始・棋譜の置き換え）
    void playersChanged();
    /// ノードのコメントが変わった
    void commentChanged(int nodeId, const QString& comment);
    /// ノードのしおりが変わった
    void bookmarkChanged(int nodeId, const QString& bookmark);

private:
    /// 文書の中身を置き換えた後の共通の後始末（対局の記録・保存先を消し、今の内容を変更なしとする）
    void finishReplacement(const QList<KifGameInfoItem>& header, bool beforeMatchStart);
    /// 対局情報の値を自動で書く（保存した時点の内容にも書くので、未保存に数えない）
    void setHeaderValue(const QString& key, const QString& value);
    /// 保存する中身がある（指し手・開始局面のコメントがある、またはファイルと結びついている）
    bool hasRecordContent() const;

    struct SavedState {
        quint64 treeRevision = 0;
        quint64 annotationRevision = 0;
        QList<KifGameInfoItem> header;
    };

    KifuBranchTree* m_tree = nullptr;   ///< 棋譜の正本（非所有）
    QList<Hooks> m_editors;   ///< 文書を編集する画面（画面がなければ空）
    QList<KifGameInfoItem> m_header;
    MatchPlayers m_players;
    MatchTimeRecord m_timeRecord;
    bool m_keepRecordHeader = false;    ///< 棋譜の途中から続けた対局（対局情報を書き換えない）
    QString m_filePath;
    quint64 m_annotationRevision = 0;   ///< コメント・しおりの変更の回数
    SavedState m_saved;
    bool m_unsavedContent = false;      ///< ファイルにない内容を読み込んだ
};

#endif // KIFUDOCUMENT_H
