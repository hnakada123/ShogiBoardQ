#ifndef JOSEKIWINDOWWIRING_H
#define JOSEKIWINDOWWIRING_H

/// @file josekiwindowwiring.h
/// @brief 定跡ウィンドウ配線クラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <QString>
#include <QStringList>
#include <QPoint>

#include "playmode.h"

class JosekiWindow;
class ShogiGameController;
class KifuRecordListModel;

/**
 * @brief 定跡ウィンドウとMainWindowの間のUI配線を担当するクラス
 *
 * 責務:
 * - JosekiWindowの生成・表示管理
 * - 定跡手選択シグナルの処理（USI→QPoint変換）
 * - 棋譜データの収集と提供
 * - humanCanPlayの判定
 *
 * 本クラスは定跡ウィンドウ関連の処理を一元管理し、
 * MainWindowの肥大化を防ぐ。
 */
class JosekiWindowWiring : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        QWidget* parentWidget = nullptr;
        ShogiGameController* gameController = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        QStringList* sfenRecord = nullptr;
        QStringList* usiMoves = nullptr;
        QString* currentSfenStr = nullptr;
        int* currentMoveIndex = nullptr;
        int* currentSelectedPly = nullptr;
        PlayMode* playMode = nullptr;
    };

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit JosekiWindowWiring(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~JosekiWindowWiring() override = default;

    /**
     * @brief 定跡ウィンドウを表示する
     */
    void displayJosekiWindow();

    /**
     * @brief 定跡ウィンドウを更新する
     *
     * 現在の局面と手番情報を反映する。
     */
    void updateJosekiWindow();

    /**
     * @brief 定跡ウィンドウを取得
     * @return JosekiWindowへのポインタ（未作成時はnullptr）
     */
    JosekiWindow* josekiWindow() const { return m_josekiWindow; }

    /**
     * @brief JosekiWindowを確保する（未作成時のみ作成）
     */
    void ensureJosekiWindow();

signals:
    /**
     * @brief 指し手実行を要求
     * @param from 移動元座標
     * @param to 移動先座標
     */
    void moveRequested(const QPoint& from, const QPoint& to);

    /**
     * @brief 強制成りモードを設定
     * @param forced 強制するかどうか
     * @param promote 成るかどうか
     */
    void forcedPromotionRequested(bool forced, bool promote);

private slots:
    /**
     * @brief 定跡手選択時の処理
     * @param usiMove USI形式の指し手
     */
    void onJosekiMoveSelected(const QString& usiMove);

    /**
     * @brief 棋譜データマージ要求時の処理
     */
    void onRequestKifuDataForMerge();

private:
    /**
     * @brief USI形式の指し手をQPoint座標に変換
     * @param usiMove USI形式の指し手
     * @param from 移動元座標（出力）
     * @param to 移動先座標（出力）
     * @param promote 成るかどうか（出力）
     * @return 変換成功時true
     */
    bool parseUsiMove(const QString& usiMove, QPoint& from, QPoint& to, bool& promote) const;

    /**
     * @brief 駒打ちのUSI形式を解析
     * @param usiMove USI形式の指し手
     * @param from 移動元座標（出力）
     * @param to 移動先座標（出力）
     * @return 解析成功時true
     */
    bool parseDropMove(const QString& usiMove, QPoint& from, QPoint& to) const;

    /**
     * @brief 通常移動のUSI形式を解析
     * @param usiMove USI形式の指し手
     * @param from 移動元座標（出力）
     * @param to 移動先座標（出力）
     * @param promote 成るかどうか（出力）
     * @return 解析成功時true
     */
    bool parseNormalMove(const QString& usiMove, QPoint& from, QPoint& to, bool& promote) const;

    /**
     * @brief 人間が着手可能かどうかを判定
     * @return 着手可能時true
     */
    bool determineHumanCanPlay() const;

    /**
     * @brief SFEN差分からUSI形式指し手リストを生成
     * @return USI形式指し手リスト
     */
    QStringList generateUsiMovesFromSfen() const;

    // 依存オブジェクト
    QWidget* m_parentWidget = nullptr;
    ShogiGameController* m_gameController = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;
    QStringList* m_sfenRecord = nullptr;
    QStringList* m_usiMoves = nullptr;
    QString* m_currentSfenStr = nullptr;
    int* m_currentMoveIndex = nullptr;
    int* m_currentSelectedPly = nullptr;
    PlayMode* m_playMode = nullptr;

    // 内部オブジェクト
    JosekiWindow* m_josekiWindow = nullptr;
};

#endif // JOSEKIWINDOWWIRING_H
