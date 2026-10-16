#ifndef ENGINELIFECYCLEMANAGER_H
#define ENGINELIFECYCLEMANAGER_H

/// @file enginelifecyclemanager.h
/// @brief 対局で指すエンジンの生成・起動・破棄と、手番との対応

#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <optional>

#include "matchtimesource.h"
#include "shogitypes.h"

class Usi;
class ShogiClock;
class EngineLifecycleManager;

/**
 * @brief 枠のエンジンを使う権利（move-only）
 *
 * EngineLifecycleManager::acquire が返す。持ち主はこれを持っている間だけ engine() を使う。
 * 手放す（reset・破棄）と、枠のエンジンを止めて枠を空ける（思考情報・通信ログの表示は残す）。
 * ほかの持ち主が同じ枠を取った・全部の枠を破棄したときは、前の持ち主に
 * EngineLifecycleManager::engineRevoked で知らせ、このリースは空になる（engine() が nullptr）。
 */
class EngineLease
{
public:
    EngineLease() = default;
    ~EngineLease();
    EngineLease(const EngineLease&) = delete;
    EngineLease& operator=(const EngineLease&) = delete;
    EngineLease(EngineLease&& other) noexcept;
    EngineLease& operator=(EngineLease&& other) noexcept;

    /// 使えるエンジン（取り上げられた・手放したら nullptr）
    Usi* engine() const;
    explicit operator bool() const { return engine() != nullptr; }
    /// 手放す（枠を空ける）
    void reset();

private:
    friend class EngineLifecycleManager;
    EngineLease(EngineLifecycleManager* manager, Usi* engine);

    QPointer<EngineLifecycleManager> m_manager;
    QPointer<Usi> m_engine;
};

/**
 * @brief 対局で指すエンジン（最大2つ）を持ち、手番との対応を管理する
 *
 * エンジンは表示の枠（E1・E2。思考情報と通信ログの表示先）に置き、どちらの手番を指すかを覚える。
 * 人間対エンジンではエンジンは常に E1 に置く（後手を指すときも）。表示のモデルには触れない
 * （表示する側が enginesChanged を受けて枠のエンジンとモデルを結ぶ）。
 * エンジンの投了・入玉宣言勝ちは、指している手番の知らせ（engineResigned・engineDeclaredWin）にする。
 */
class EngineLifecycleManager : public QObject
{
    Q_OBJECT

public:
    /// clock: 対局の時計（エンジンが探索の時間を読む）
    explicit EngineLifecycleManager(ShogiClock* clock, QObject* parent = nullptr);
    ~EngineLifecycleManager() override;

    /**
     * @brief 枠（1=E1、2=E2）にエンジンを作り、owner に使わせる
     *
     * side の手番を指させる（前にあったエンジンは破棄し、その持ち主に engineRevoked で知らせる）。
     * 手番を持つエンジンだけが対局の時計を読み、投了・宣言勝ちを知らせる。
     * 検討・詰み探索・棋譜解析・通信対局のエンジンは手番を持たない（side は nullopt）
     */
    [[nodiscard]] EngineLease acquire(QObject* owner, int slot, std::optional<Turn> side, const QString& engineName);
    /// 作ったエンジンのプロセスを起動して初期化する（初期化は非同期）
    void startEngine(Usi* engine, const QString& enginePath, const QString& engineName);

    /// side の手番を指すエンジン（人間の手番なら nullptr）
    Usi* engineFor(Turn side) const;
    /// 枠のエンジン（1=E1、2=E2）
    Usi* engineInSlot(int slot) const;
    /// 手番を指すエンジンの枠（エンジンが指さない手番は 0）
    int slotFor(Turn side) const;

    /// 全エンジンを破棄する（持ち主には engineRevoked で知らせる。clearDisplay なら思考情報・通信ログの表示も消すよう知らせる）
    void destroyEngines(bool clearDisplay = true);
    /// engine が枠に入っているか
    bool holds(const Usi* engine) const;

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
    /// 枠のエンジンが変わった（作った・破棄した）
    void enginesChanged();
    /// owner のエンジンを取り上げた（ほかの持ち主が同じ枠を取った・全部の枠を破棄した）。owner のリースは空になる
    void engineRevoked(QObject* owner);
    /// 前のエンジンの思考情報・通信ログの表示を消す（新しい対局・検討を始める）
    void displayClearRequested();
    /// side の手番を指すエンジンが投了した
    void engineResigned(Turn side);
    /// side の手番を指すエンジンが入玉宣言勝ちを宣言した
    void engineDeclaredWin(Turn side);
    /// 対局のエンジン（手番を持つエンジン）の起動の失敗・異常終了
    void engineError(const QString& message);

private:
    friend class EngineLease;

    struct Seat {
        Usi* engine = nullptr;
        std::optional<Turn> side;
        QPointer<QObject> owner;
    };
    Seat& seat(int slot) { return m_seats[static_cast<size_t>(slot == 2 ? 1 : 0)]; }
    const Seat& seat(int slot) const { return m_seats[static_cast<size_t>(slot == 2 ? 1 : 0)]; }
    /// 枠を空ける。notifyOwner なら持ち主に engineRevoked で知らせる
    void destroySeat(Seat& seat, bool clearThinking, bool notifyOwner);
    /// リースを手放したとき（持ち主には知らせない）
    void release(Usi* engine);

    ClockTimeSource m_clockTime;   ///< 対局のエンジンが持ち時間を読む対局時計
    std::array<Seat, 2> m_seats;
    bool m_shutdownInProgress = false;
};

#endif // ENGINELIFECYCLEMANAGER_H
