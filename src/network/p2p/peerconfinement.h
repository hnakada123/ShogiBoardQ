#ifndef PEERCONFINEMENT_H
#define PEERCONFINEMENT_H

/// @file peerconfinement.h
/// @brief 通信プロセスの起動のしかた（権限の制限と、引き継ぐ環境・ハンドル）

#include <QString>
#include <QStringList>

#include <memory>

class QProcess;

/**
 * @brief 通信プロセス（shogiboardq-peer）を、権限を制限して起動するための設定（実装仕様 10.4節、protocol/ipc.md 6章）
 *
 * Linux と macOS では、通信プロセスが起動の直後に自分で制限を掛ける（Linux は Landlock・seccomp-bpf、macOS は
 * サンドボックス Seatbelt）ので、ここでは環境変数と記述子を引き継がないことだけを決める。Windows では、プロセスを作る時点でしか掛けられない制限（AppContainer・
 * ジョブ・子プロセスの禁止・緩和ポリシー）をここで掛け、通信プロセスは掛かっていることを確かめてから動く。
 * どの OS でも、通信プロセスが hello の応答で返す仕組みの名前を requiredMechanisms() と照らす。
 *
 * apply() が返すものは、プロセスが終わるまで持つ（Windows では、手放すとジョブが閉じて通信プロセスが終わる）。
 */
class PeerConfinement
{
public:
    /// 通信プロセスに許す宛先の範囲（Windows は AppContainer の能力、macOS は loopback への発信だけを
    /// サンドボックスで止める。Linux では通信プロセスのコードだけが守る）
    enum class Reach {
        Internet,      ///< インターネットだけ（loopback と、OS がプライベートと扱うネットワークへは出られない）
        LocalNetwork,  ///< 同じ LAN も（ローカルのネットワーク sbq-local-1）
    };

    virtual ~PeerConfinement();

    /// この OS で通信プロセスの権限を制限できる
    static bool isSupported();
    /// reach で起動した通信プロセスが、hello の応答の mechanisms に必ず入れているはずの名前（起動の条件で決まる
    /// 制限も含む。起動の処理が退行して制限が欠けたら、つながずに止める。監査 2026-11-21 I-13）
    static QStringList requiredMechanisms(Reach reach);
    /// 起動の前に process へ設定する（プログラムと引数を入れた後に呼ぶ。引数を足すことがある）。player は、この PC の
    /// 何人目のプレイヤーの通信プロセスか（Windows の AppContainer をプレイヤーごとに分ける）。できなければ nullptr を
    /// 返し、error に診断用の理由（ASCII）を入れる
    static std::unique_ptr<PeerConfinement> apply(QProcess* process, const QString& executable, Reach reach, int player,
                                                  QString* error);

protected:
    PeerConfinement() = default;

private:
    Q_DISABLE_COPY_MOVE(PeerConfinement)
};

#endif // PEERCONFINEMENT_H
