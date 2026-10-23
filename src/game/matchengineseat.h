#ifndef MATCHENGINESEAT_H
#define MATCHENGINESEAT_H

/// @file matchengineseat.h
/// @brief 対局で指すエンジン1台（枠のリース・手番・持ち時間の出どころ・知らせ・終局の手順）

#include <QObject>
#include <QPointer>
#include <QString>

#include "enginelifecyclemanager.h"
#include "shogitypes.h"
#include "usimatchhandler.h"

class MatchTimeSource;
class Usi;

/**
 * @brief 対局で指すエンジン1台
 *
 * 人間対エンジン・エンジン同士の Strategy と通信対局（CsaGameCoordinator）が持つ。エンジンの枠
 * （EngineLifecycleManager）からエンジンを借り、持ち時間の出どころを渡して起動する。
 * エンジンの初期化・指し手・投了・入玉宣言勝ち・エラーを自分のシグナルで知らせる（ほかの持ち主が枠を
 * 取ったときもエラーとして知らせる）。終局ではローカル対局・通信対局と同じ手順でエンジンに知らせる。
 */
class MatchEngineSeat : public QObject
{
    Q_OBJECT

public:
    struct Params {
        int slot = 1;                           ///< 枠（1=E1、2=E2）
        Turn side = Turn::Black;                ///< このエンジンが指す手番
        QString enginePath;
        QString engineName;
        QString role;                           ///< 通信ログの見出しの役割（空なら手番 "black"・"white"）
        MatchTimeSource* timeSource = nullptr;  ///< 持ち時間の出どころ（非所有。ローカル対局は対局時計）
    };

    explicit MatchEngineSeat(EngineLifecycleManager* engines, QObject* parent = nullptr);
    ~MatchEngineSeat() override;

    /// エンジンを枠に置いて起動する（前のエンジンは手放す）。初期化の完了は initialized、
    /// 起動の失敗は errorOccurred で、どちらもこの呼び出しから戻った後に届く
    void start(const Params& params);

    /// 使っているエンジン（起動していない・手放した・取り上げられたら nullptr）
    Usi* engine() const;
    Turn side() const { return m_side; }
    /// 起動して初期化（usi〜readyok）している
    bool isInitializing() const;
    /// readyok を受け、指し手を頼める（終局の後は false。次の対局は起動し直す）
    bool isReady() const;

    /**
     * @brief 終局をエンジンに知らせる（探索・先読みを止めて gameover と quit を送る）
     *
     * 終局後の bestmove で着手・先読みを再開させない。quit に応答しなくても終了監視が強制終了する。
     * エンジンは枠に残す（思考情報・通信ログの表示を残す）。
     */
    void finish(GameOverResult result);
    /// エンジンを手放して枠を空ける（quit を送る。思考情報・通信ログの表示は残す）
    void release();

signals:
    void initialized();
    void moveReady(const UsiMatchMove& move);
    /// 手番の探索に投了で答えた
    void resigned(Turn side);
    /// 手番の探索に入玉宣言勝ちで答えた
    void declaredWin(Turn side);
    /// 起動の失敗・異常終了・ほかの持ち主が枠を取った
    void errorOccurred(const QString& message);

private slots:
    void onEngineRevoked(QObject* owner);

private:
    EngineLifecycleManager* m_engines = nullptr;  ///< 非所有（エンジンは枠が持つ）
    EngineLease m_lease;
    Turn m_side = Turn::Black;
};

#endif // MATCHENGINESEAT_H
