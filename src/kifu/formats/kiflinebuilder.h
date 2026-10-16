#ifndef KIFLINEBUILDER_H
#define KIFLINEBUILDER_H

/// @file kiflinebuilder.h
/// @brief 棋譜の読み込みで1本の手順を組み立てる（全形式で共通）

#include <QString>
#include <array>
#include <optional>

#include "kifmovetext.h"
#include "recordline.h"
#include "recordtime.h"
#include "shogiposition.h"
#include "terminaltype.h"
#include "usimove.h"

/**
 * @brief 棋譜の読み込みで1本の手順（本譜か変化）を組み立てる
 *
 * 各形式のパーサは、読んだ順に addMove・addTerminal・appendComment・appendBookmark を呼ぶだけにする。
 * 手を指したその場で局面を進めて RecordEntry を作るので、表示の指し手と USI の指し手を別々に積んで
 * 突き合わせることはない。手番・直前の手の移動先（「同」の基準）・両者の累計の消費時間はこの部品が持つ。
 * 棋譜欄の表記は、指し手と直前の手から KifMoveText で作る（取り込んだ原文は使わない）。
 */
class KifLineBuilder
{
public:
    /// 手順の種類（開始局面のコメント・しおりの置き場所が違う）
    enum class LineKind {
        Mainline,   ///< 本譜。最初の手より前のコメント・しおりは開始局面のもの
        Variation,  ///< 変化。最初の手より前のコメント・しおりは最初の手のもの
    };

    /**
     * @param start 手順の開始局面（本譜は棋譜の開始局面、変化は分岐する前の局面）
     * @param startPly 最初の手の手数
     * @param previous 開始局面に至った手（変化の最初の手の「同」の基準。本譜は nullopt）
     */
    explicit KifLineBuilder(const Position& start, int startPly = 1, std::optional<UsiMove> previous = std::nullopt,
                            LineKind kind = LineKind::Mainline);

    /// 今の局面（次の手を指す前）
    const Position& position() const { return m_position; }
    /// 次の手を指す側
    Turn sideToMove() const { return m_position.sideToMove(); }
    /// 次の手の手数
    int nextPly() const { return m_line.startPly + m_line.moveCount(); }
    /// 直前の手の移動先（「同」の基準。開始局面に至った手もなければ無効なマス）
    BoardSquare previousTo() const { return m_previous ? m_previous->to : BoardSquare{}; }
    /// 終局の行を足したか、stop() で打ち切ったか（以降の手は足さない）
    bool ended() const { return m_stopped || m_line.endsWithTerminal(); }
    /// 手順を打ち切る（読めない手の後。コメント・しおりは直前の手に付け続ける）
    void stop() { m_stopped = true; }

    /**
     * @brief 手番側の手を指して、棋譜の1行を足す
     * @param notes 棋譜に書かれていた修飾（相対位置・「不成」。JKF・KI2 の表記を残す）
     * @param rule 打つ駒が持ち駒にない手を許すか（既定は棋譜の読み込み用に許す）
     * @return 指せなければ（移動元が空・打つ駒がない）何も変えずに false
     */
    bool addMove(const UsiMove& move, const RecordTime& time = zeroTime(), const KifMoveText::Notes& notes = {},
                 Position::DropRule rule = Position::DropRule::Lenient);
    /// 直前の addMove で指せなかった理由（"Source square is empty: 7g7e" など）
    const QString& lastError() const { return m_lastError; }
    /// 指せなかった手の警告（"KIF: Cannot restore move 3: Source square is empty: 7g7e"。全形式で同じ文面）
    QString cannotRestore(const QString& format, int ply) const
    {
        return QStringLiteral("%1: Cannot restore move %2: %3").arg(format).arg(ply).arg(m_lastError);
    }

    /// 終局の行を足す（side は終局を記録した側。表記は終局語の表から作る）
    void addTerminal(TerminalType type, Turn side, const RecordTime& time = zeroTime());

    /// 最後の行（終局の行を含む）にコメントを足す。手がまだなければ開始局面（変化は最初の手）のもの
    void appendComment(const QString& text);
    /// 最後の行にしおりを足す。手がまだなければ開始局面（変化は最初の手）のもの
    void appendBookmark(const QString& text);

    /**
     * @brief side の1手の消費時間から、累計を足した消費時間を作る（CSA の T 行・JKF の time）
     * @param totalMs 累計が書いてあればその値（累計はこの値に置き換える）
     */
    RecordTime timeFor(Turn side, qint64 elapsedMs, std::optional<qint64> totalMs = std::nullopt);
    /// 最後の行の消費時間を置き換える（指し手の後に消費時間を書く形式）
    void setLastTime(const RecordTime& time);
    /// 最後の行を記録した側（行がなければ nullopt）
    std::optional<Turn> lastMover() const;
    /// 両者の累計の消費時間（[0] 先手、[1] 後手）。変化は分岐した局面までの累計から始める
    const std::array<qint64, 2>& totals() const { return m_totalMs; }
    void setTotals(const std::array<qint64, 2>& totals) { m_totalMs = totals; }

    /// 消費時間が書いてない行の消費時間（0秒）
    static RecordTime zeroTime() { return RecordTime::fromMilliseconds(0, 0); }

    /// 組み立てた手順
    const RecordLine& line() const { return m_line; }
    RecordLine take();

private:
    static void appendText(QString& target, const QString& text);

    RecordLine m_line;
    Position m_position;
    std::optional<UsiMove> m_previous;
    LineKind m_kind = LineKind::Mainline;
    bool m_stopped = false;
    QString m_lastError;
    QString m_pendingComment;   ///< 変化の最初の手より前のコメント
    QString m_pendingBookmark;  ///< 変化の最初の手より前のしおり
    std::array<qint64, 2> m_totalMs{0, 0};
};

#endif // KIFLINEBUILDER_H
