#ifndef ENGINELIFECYCLEMANAGER_H
#define ENGINELIFECYCLEMANAGER_H

/// @file enginelifecyclemanager.h
/// @brief 対局で指すエンジンの生成・起動・破棄と、手番との対応

#include <QObject>
#include <QString>
#include <array>
#include <optional>

#include "shogitypes.h"

class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiClock;

/**
 * @brief 対局で指すエンジン（最大2つ）を持ち、手番との対応を管理する
 *
 * エンジンは表示の枠（E1・E2。思考情報と通信ログの表示先）に置き、どちらの手番を指すかを覚える。
 * 人間対エンジンではエンジンは常に E1 に置く（後手を指すときも）。
 * エンジンの投了・入玉宣言勝ちは、指している手番の知らせ（engineResigned・engineDeclaredWin）にする。
 */
class EngineLifecycleManager : public QObject
{
    Q_OBJECT

public:
    /// 思考情報・通信ログの表示先（E1・E2）
    struct Models {
        UsiCommLogModel* comm1 = nullptr;
        ShogiEngineThinkingModel* think1 = nullptr;
        UsiCommLogModel* comm2 = nullptr;
        ShogiEngineThinkingModel* think2 = nullptr;
    };

    /// clock: 対局の時計（エンジンが探索の時間を読む）
    explicit EngineLifecycleManager(ShogiClock* clock, QObject* parent = nullptr);
    ~EngineLifecycleManager() override;

    void setModels(const Models& models);
    const Models& models() const { return m_models; }

    /// 枠（1=E1、2=E2）にエンジンを作り、side の手番を指させる（前にあったエンジンは破棄する）。
    /// 検討・詰み探索のエンジンは手番を持たない（side は nullopt）
    Usi* createEngine(int slot, std::optional<Turn> side, const QString& engineName);
    /// 作ったエンジンのプロセスを起動して初期化する（初期化は非同期）
    void startEngine(Usi* engine, const QString& enginePath, const QString& engineName);

    /// side の手番を指すエンジン（人間の手番なら nullptr）
    Usi* engineFor(Turn side) const;
    /// 枠のエンジン（1=E1、2=E2）
    Usi* engineInSlot(int slot) const;
    /// 手番を指すエンジンの枠（エンジンが指さない手番は 0）
    int slotFor(Turn side) const;

    /// 全エンジンを破棄する（clearModels なら思考情報・通信ログの表示も消す）
    void destroyEngines(bool clearModels = true);

    /**
     * @brief 終局をエンジンに知らせて終了させる
     * @param loser 負けた側（引き分けは nullopt）
     *
     * 探索を止めて gameover を送り、quit を送る（応答しなければ終了監視が強制終了する）。
     */
    void notifyGameOver(std::optional<Turn> loser);

    /// 投了・入玉宣言勝ちの知らせを止める（中断の後に届く bestmove resign を終局にしない）
    void disconnectArbiterSignals();

    bool isShutdownInProgress() const { return m_shutdownInProgress; }
    void setShutdownInProgress(bool inProgress) { m_shutdownInProgress = inProgress; }

signals:
    void enginesChanged();
    /// side の手番を指すエンジンが投了した
    void engineResigned(Turn side);
    /// side の手番を指すエンジンが入玉宣言勝ちを宣言した
    void engineDeclaredWin(Turn side);
    /// 対局のエンジン（手番を持つエンジン）の起動の失敗・異常終了
    void engineError(const QString& message);

private slots:
    void onEngineResign();
    void onEngineWin();

private:
    struct Seat {
        Usi* engine = nullptr;
        std::optional<Turn> side;
    };
    Seat& seat(int slot) { return m_seats[static_cast<size_t>(slot == 2 ? 1 : 0)]; }
    const Seat& seat(int slot) const { return m_seats[static_cast<size_t>(slot == 2 ? 1 : 0)]; }
    std::optional<Turn> sideOf(const QObject* engine) const;
    void destroySeat(Seat& seat, bool clearThinking);

    ShogiClock* m_clock = nullptr;
    Models m_models;
    std::array<Seat, 2> m_seats;
    bool m_shutdownInProgress = false;
};

#endif // ENGINELIFECYCLEMANAGER_H
