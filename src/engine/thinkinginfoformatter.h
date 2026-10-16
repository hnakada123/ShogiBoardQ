#ifndef THINKINGINFOFORMATTER_H
#define THINKINGINFOFORMATTER_H

/// @file thinkinginfoformatter.h
/// @brief エンジンの info 行を、思考の表・情報欄に出す値に整える

#include <QObject>
#include <QLocale>
#include <QList>
#include <QHash>
#include <memory>

#include "thinkinginforow.h"
#include "usiinfo.h"
#include "usimessages.h"

class PvNotationFormatter;

/**
 * @brief エンジンの info 行を表示用の値に整えて知らせる
 *
 * 解析済みの info（UsiInfo）を思考の表の行（ThinkingInfoRow）と、情報欄の値（探索中の手・深さ・
 * ノード数・NPS・ハッシュ使用率）に整えてシグナルで知らせる。読み筋は探索の局面から漢字表記にする。
 * 表示する側（モデル・ウィジェット）には触れず、翻訳する文字列も作らない。
 */
class ThinkingInfoFormatter : public QObject
{
    Q_OBJECT

public:
    explicit ThinkingInfoFormatter(QObject* parent = nullptr);
    ~ThinkingInfoFormatter() override;

    /// 直前の指し手の行き先（読み筋の最初の手の「同」の判定）
    void setPreviousMove(int fileTo, int rankTo);
    /// 読み筋を表示する局面に至った指し手（USI 形式。読み筋の盤の強調）
    void setLastUsiMove(const QString& move) { m_lastUsiMove = move; }

    /// info を処理してシグナルを発行（バッファを通さない）
    void processInfo(const UsiInfo& info);

public slots:
    /// まとめて処理するために溜めている info を処理する
    void flushInfoBuffer();
    /// 読み筋を表示する局面（探索の局面）
    void setBaseSfen(const QString& sfen);
    /// info を受信（50ms ごとにまとめて処理する）
    void onInfoReceived(const UsiInfo& info);
    /// 探索を始めた。溜めている info を捨て、思考情報を消すよう知らせる
    void requestClearThinkingInfo();
    /// 詰み探索の結果。info を省くエンジンでも詰み手順を思考の表に残す
    void presentCheckmate(const UsiCheckmate& result);

signals:
    void thinkingInfoUpdated(const ThinkingInfoRow& row);
    void clearThinkingInfoRequested();
    void searchedMoveUpdated(const QString& move);
    void searchDepthUpdated(const QString& depth);
    void nodeCountUpdated(const QString& nodes);
    void npsUpdated(const QString& nps);
    void hashUsageUpdated(const QString& hashUsage);

private:
    void emitSearchedHand();
    void emitStatus(const UsiInfo& info);

    int m_previousFileTo = 0;           ///< 前回の指し手の筋
    int m_previousRankTo = 0;           ///< 前回の指し手の段
    QString m_baseSfen;                 ///< 思考開始時の局面SFEN
    QString m_lastUsiMove;
    QLocale m_locale;                   ///< 3桁区切りの表示（English）

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
