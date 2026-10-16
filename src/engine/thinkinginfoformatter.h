#ifndef THINKINGINFOFORMATTER_H
#define THINKINGINFOFORMATTER_H

/// @file thinkinginfoformatter.h
/// @brief エンジンの info 行を、思考の表・情報欄に出す値に整える

#include <QObject>
#include <QList>
#include <QHash>
#include <memory>

#include "enginesearchstatus.h"
#include "thinkinginforow.h"
#include "usiinfo.h"
#include "usimessages.h"
#include "usiposition.h"

class PvNotationFormatter;

/**
 * @brief エンジンの info 行を表示用の値に整えて知らせる
 *
 * 解析済みの info（UsiInfo）を思考の表の行（ThinkingInfoRow）と、情報欄の探索状況（EngineSearchStatus。
 * 探索中の手・深さ・ノード数・NPS・ハッシュ使用率）に整えて、info 1行ごとに1回ずつ知らせる。読み筋は探索の局面から漢字表記にする。
 * 表示する側（モデル・ウィジェット）には触れず、翻訳する文字列も作らない。
 */
class ThinkingInfoFormatter : public QObject
{
    Q_OBJECT

public:
    explicit ThinkingInfoFormatter(QObject* parent = nullptr);
    ~ThinkingInfoFormatter() override;

    /// info を処理してシグナルを発行（バッファを通さない）
    void processInfo(const UsiInfo& info);

public slots:
    /// まとめて処理するために溜めている info を処理する
    void flushInfoBuffer();
    /// 読み筋を表示する局面（探索の局面）。そこに至った直前の手を、読み筋の最初の手の「同」の基準と
    /// 盤の強調に使う（先読み中は予想した相手の手が直前の手）
    void setSearchPosition(const UsiPosition& position);
    /// info を受信（50ms ごとにまとめて処理する）
    void onInfoReceived(const UsiInfo& info);
    /// 探索を始めた。溜めている info を捨て、思考情報を消すよう知らせる
    void requestClearThinkingInfo();
    /// 詰み探索の結果。info を省くエンジンでも詰み手順を思考の表に残す
    void presentCheckmate(const UsiCheckmate& result);

signals:
    void thinkingInfoUpdated(const ThinkingInfoRow& row);
    void clearThinkingInfoRequested();
    /// info 1行の探索状況（含まれなかった項目は「無し」）
    void searchStatusUpdated(const EngineSearchStatus& status);

private:
    /// 探索中の手の表示（無ければ空）
    QString searchedMoveLabel();

    BoardSquare m_previousTo;           ///< 探索の局面に至った手の移動先（「同」の基準）
    QString m_baseSfen;                 ///< 探索の局面の SFEN
    QString m_lastUsiMove;              ///< 探索の局面に至った手（USI 形式）

    // 探索手の欄の表示。同じ手が何度も届く（MultiPV では候補の数だけの手が繰り返し届く）ので、
    // 局面と表記の設定が同じ間は、探索手（漢字表記と USI形式）ごとに作った表示を覚えておく
    QString m_searchedSfenSource;       ///< 覚えた表示を作ったときの局面SFEN
    int m_searchedNotation = -1;        ///< 覚えた表示を作ったときの表記（KifuPresentation::Notation）
    bool m_searchedAlwaysOrigin = false;
    QHash<QString, QString> m_searchedLabels;  ///< 探索手（漢字表記 + U+0000 + USI形式）→ 表示

    QList<UsiInfo> m_infoBuffer;        ///< infoバッファ
    bool m_flushScheduled = false;      ///< フラッシュ予約済みフラグ
    std::unique_ptr<PvNotationFormatter> m_formatter; ///< 読み筋を漢字表記にする（局面を持つので再利用）
};

#endif // THINKINGINFOFORMATTER_H
