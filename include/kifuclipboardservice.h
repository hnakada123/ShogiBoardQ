#ifndef KIFUCLIPBOARDSERVICE_H
#define KIFUCLIPBOARDSERVICE_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "playmode.h"

class QWidget;
class QTableWidget;
class KifuRecordListModel;
class GameRecordModel;
struct ShogiMove;

/**
 * @brief 棋譜のクリップボード操作を担当するサービスクラス
 *
 * MainWindowから分離された責務:
 * - KIF/KI2/CSA/USI/JKF/USEN/SFEN/BOD形式での棋譜コピー
 * - クリップボードからの棋譜貼り付け
 * - USI指し手リストの生成
 */
namespace KifuClipboardService {

/// エクスポートに必要なコンテキスト情報
struct ExportContext {
    QTableWidget*       gameInfoTable = nullptr;
    KifuRecordListModel* recordModel   = nullptr;
    GameRecordModel*    gameRecord    = nullptr;
    QString             startSfen;
    PlayMode            playMode      = NotStarted;
    QString             human1;
    QString             human2;
    QString             engine1;
    QString             engine2;
    QStringList         usiMoves;
    QStringList*        sfenRecord    = nullptr;
    int                 currentPly    = 0;
    bool                isPlaying     = false;
};

/// KIF形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyKif(const ExportContext& ctx);

/// KI2形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyKi2(const ExportContext& ctx);

/// CSA形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyCsa(const ExportContext& ctx);

/// USI形式（全て）で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyUsi(const ExportContext& ctx);

/// USI形式（現在の指し手まで）で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyUsiCurrent(const ExportContext& ctx, int currentMoveIndex);

/// JKF形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyJkf(const ExportContext& ctx);

/// USEN形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
bool copyUsen(const ExportContext& ctx);

/// SFEN形式で局面をクリップボードにコピー
/// @return 成功した場合true
bool copySfen(const ExportContext& ctx);

/// BOD形式で局面をクリップボードにコピー
/// @return 成功した場合true
bool copyBod(const ExportContext& ctx);

/// ShogiMoveリストからUSI形式の指し手リストを生成
QStringList gameMovesToUsiMoves(const QVector<ShogiMove>& moves);

/// SFENレコードからUSI形式の指し手リストを生成
QStringList sfenRecordToUsiMoves(const QStringList& sfenRecord);

/// クリップボードから棋譜テキストを取得
QString getClipboardText();

/// 棋譜形式を自動判定
enum class KifuFormat {
    Unknown,
    Kif,
    Ki2,
    Csa,
    Usi,
    Jkf,
    Usen,
    Sfen
};
KifuFormat detectFormat(const QString& text);

} // namespace KifuClipboardService

#endif // KIFUCLIPBOARDSERVICE_H
