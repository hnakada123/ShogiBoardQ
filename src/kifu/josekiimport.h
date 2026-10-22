#ifndef JOSEKIIMPORT_H
#define JOSEKIIMPORT_H

/// @file josekiimport.h
/// @brief 複数の棋譜ファイルから、定跡に登録する手を集める（UI に触れないので、ワーカースレッドから呼べる）

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

#include "shogitypes.h"

struct KifParseResult;
struct RecordLine;

namespace JosekiImport {

/// 登録する手の側
enum class Sides { Both, BlackOnly, WhiteOnly, WinnerOnly };

/// 集める条件
struct Options {
    int maxPly = 60;                 ///< この手数（1手目から数える）までの手を集める
    Sides sides = Sides::Both;       ///< 「勝った側だけ」は勝敗の付いた棋譜だけを使う
    bool includeVariations = false;  ///< 変化の手も集める
    int minCount = 1;                ///< 集めた棋譜のうち、この局数以上で指された手だけを登録する
    bool useComments = false;        ///< 棋譜のコメントを定跡のコメントにする（定跡にコメントの無い手だけ）
};

/// 集めた手
struct Move {
    QString sfen;     ///< 指す前の局面（最初に見つけた棋譜の手数のまま）
    QString usiMove;
    int count = 0;    ///< 指された棋譜の数（1局で何度出ても1と数える）
    QString comment;  ///< 最初に見つけたコメント（useComments のときだけ）
};

/// 集めた結果
struct Result {
    QList<Move> moves;        ///< 最小出現回数を満たす手（見つけた順）
    int gameCount = 0;        ///< 読めた棋譜の数
    QStringList failedFiles;  ///< 読めなかった棋譜
    bool canceled = false;    ///< 途中で止めた（moves は空）
};

/// 棋譜ファイルとして読む拡張子か（KIF・KI2・CSA・JKF・USI・USEN）
bool isKifuFile(const QString &path);
/// ファイルとフォルダ（サブフォルダの中まで）から棋譜ファイルを集める（重複を除き、フォルダの中は名前順）
QStringList expandPaths(const QStringList &paths);

/**
 * @brief 1局の棋譜の手を集める
 *
 * 局面（手数を除く）と指し手の組ごとに、seen に無ければ足して counts の数を1増やす（1局で1回だけ数える）。
 */
class Collector
{
public:
    explicit Collector(const Options &options);

    /// 1局の棋譜の手を集める
    void addGame(const KifParseResult &record);
    /// 最小出現回数を満たす手（見つけた順）
    QList<Move> moves() const;

private:
    /// 手順 line（startSfen から始まる）の手を集める。winner は本譜の勝った側
    void addLine(const QString &startSfen, const RecordLine &line, std::optional<Turn> winner,
                 QSet<QString> &seenInGame);

    Options m_options;
    QList<Move> m_moves;
    QHash<QString, qsizetype> m_index;  ///< 局面のキーと指し手 → m_moves の位置
};

/**
 * @brief 棋譜ファイルを読んで手を集める
 * @param progress 1つ読むたびに読んだ数を渡して呼ぶ（false を返したら止める）
 */
Result collect(const QStringList &files, const Options &options,
               const std::function<bool(int)> &progress = std::function<bool(int)>());

} // namespace JosekiImport

#endif // JOSEKIIMPORT_H
