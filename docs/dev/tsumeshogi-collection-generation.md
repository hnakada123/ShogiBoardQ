# 詰将棋6000題の生成・検証・再編の記録

3・5・7・9・11・13手詰を各1000題、合計6000題作成した際の試行錯誤と、PythonからCLIを直接並列実行する方法をまとめる。初回生成は2026年9月26日、不要駒と類似作の整理は9月27日に行った。

**初回生成はPythonから `shogiboardq-cli generate-tsume` を直接起動した。ShogiBoardQのMCPサーバーは使用していない。** 後の再編では、リポジトリに追加した専用の候補生成・監査プログラムをPythonから起動した。この経路もMCPを介していない。

成果物は [data/tsumeshogi/](../../data/tsumeshogi/README.md) の `tsume_{3,5,7,9,11,13}ply_1000_20260926.txt`。UTF-8で、`#` 始まりのコメントを除き、1行に1題を `SFEN moves USI指し手…` の形式で保存している。ファイル名の日付は初回作成日であり、内容には翌日の再編を反映している。

## 1. 実行経路と再現できる範囲

| 段階 | 使用した処理 | この資料での扱い |
|---|---|---|
| 初回の候補収集 | Python → 標準CLIの `generate-tsume` → 共通の生成器・USIエンジン | 下記に実行可能な並列収集例を掲載 |
| 初回の長手数生成の調整 | 候補生成・事前選別を変更した `/tmp` の作業用CLI | 当時の工夫を記録。一時実行版そのものは配布物に含まれない |
| 不要駒・類似作の整理と補充 | Python → `tsumeshogi_diversity_sampler` / `tsumeshogi_collection_auditor` | 現在のリポジトリ内のツールで実行できる |
| 最終確認と保存 | 独立した再監査 → 類似判定・並べ替え → 問題集と検証記録 | 候補収集とは別の完了条件を設ける |

標準GUIとCLIは同じ `TsumeshogiGenerator` を使う。候補生成、内蔵探索による事前選別、外部USIエンジンでの詰み探索、主手順の余詰検査、不要駒トリミングが共通である。実装の詳細は [生成機能の開発資料](tsumeshogi-generator.md)、標準CLIの操作方法は [利用ガイド](../guide/tsumeshogi-generator.html#cli-generation) を参照。

以下の比率・時間・並列数は作業中に比較・調整した値であり、6000題すべてを一つの固定設定で生成したという意味ではない。

## 2. 初回生成で試したこと

### 手数ごとの方針

| 手数 | 初回作成時の工夫 |
|---|---|
| 3 | ランダム配置で300題を作成し、その300題を保持したまま700題を追加した。 |
| 5 | ランダム配置を並列実行し、外部エンジンの探索時間を0.5～20秒の範囲で比較した。 |
| 7 | 攻め駒数・守り駒数・配置範囲を比較した。一時実行版で事前選別を5msに短縮し、詰みを確認できた候補に絞った。 |
| 9 | 事前選別を1・3・5・10・25msで比較した。玉の盤端からの距離を制限する方法も試し、並列数を8から16に増やした。 |
| 11 | ランダム配置に加え、生成済みの7・9手詰と収集中の11手詰を種として、駒配置や持駒を変更した。 |
| 13 | 7・9・11手詰と収集中の13手詰を種に使った。駒移動・駒種変更を増やし、採択後に独立した最短手数の確認を追加した。 |

### 短時間の事前選別

長手数では、ランダム候補をすべて外部エンジンで詳しく調べると採択までの時間が長くなる。一時実行版では、内蔵の有限深さ探索で目標手数ちょうどの詰みを確認でき、複数の詰む初手が検出されなかった候補を外部エンジンへ渡した。この選別だけで主手順全体の一意性が証明されるわけではなく、その後の検査も行った。

事前選別の時間は7手詰で5ms、9手詰で1～25ms、11・13手詰では主に1msと3msを比較した。当時の一時実行版では時間切れなどの `Unknown` を候補から外した。探索が難しい良作も取りこぼす代わりに、短時間で確認できる候補の収集を優先した。

9手詰では玉の盤端からの最短距離を、0マス（盤端のみ）、1マス以内、4マス以内（盤全体）で比較した。候補数だけでなく、重複を除いた採択数、判定不能数、CPU・メモリ使用量を見て条件を調整した。特定の短い制限時間や盤端への限定が常に最適という結果ではない。

この調整は [tsumeshogicandidatescreener.cpp](../../src/analysis/tsumeshogicandidatescreener.cpp) の作業用コピーの `generateBatch()` に加えた。`build/compile_commands.json` とビルドのリンクコマンドを参照し、変更したオブジェクトを既存の静的ライブラリより先にリンクしたCLIを `/tmp` に作成した。種局面の読込・変更処理も作業用ディレクトリに置き、標準CLIやリポジトリのアプリケーションソースを置き換えずに試した。

当時の `SHOGIBOARDQ_BATCH_SCREEN_MS` と `SHOGIBOARDQ_BATCH_KING_EDGE` は、この一時実行版に追加した環境変数である。**標準CLIの設定項目ではなく、標準CLIに設定しても同じ調整は行われない。** 当時の変更をそのまま再現するには実装が必要だが、現在は後述する専用サンプラーで駒変更や逆算を利用できる。

### 生成済み局面からの近傍探索

11手詰以降では、約10%の試行をランダム配置、約90%を種局面の変更に割り当てた。近傍探索では、収集中の目標手数の種があれば約60%でそこから選び、残りは既存の短い問題集から選んだ。1候補に加える変更は約75%で1回、約25%で2回とした。

変更操作は、盤上の駒を縦横それぞれ±2マスの範囲へ動かす、駒種・成不成を変える、玉周辺へ駒を追加する、駒を削除する、攻方持駒を増減する、の5種類である。行き所のない歩・香・桂を成駒に補正し、二歩、駒種ごとの総数、玉の数、初期王手を確認した。未使用駒を玉方持駒に戻してSFENを組み立て直した。

13手詰の途中では、追加した駒がトリミングで消えて元の種と同じ局面に戻る例が多かった。そこで追加の割合を下げ、移動と駒種変更を増やした。

| 変更操作 | 当初 | 13手詰の調整後 |
|---|---:|---:|
| 盤上の駒の移動 | 45% | 60% |
| 駒種・成不成の変更 | 20% | 25% |
| 盤上への駒の追加 | 20% | 5% |
| 攻方持駒の増減 | 10% | 5% |
| 盤上の駒の削除 | 5% | 5% |

これらは変更を試す確率であり、採択された作品の割合ではない。変更前の詰め手順を答えとして流用せず、変更後のSFENを改めて探索した。

### 最短手数の独立確認

PV（詰め手順）の長さが13手でも、より短い詰みがないことの証明にはならない。作業用検証プログラムで `TsumeCollection::parse()` と `validMateLine()` により読み込み、全手の合法性、攻方の連続王手、終端の詰みを確認した。さらにHayanagiの `TsumeSearch::solve()` による有限深さの反復深化探索で最短手数を調べた。

初回の13手詰では、生成済み489題を1局面1秒の独立探索で再確認し、479題を残して不足分を補充した。その後はSFENごとに新しい探索表を用意し、最短13手詰と手順を確認できた局面だけを件数に加えた。確認できなかった局面もキーを記録し、同じ局面の再採択を避けた。

当初の検証は最短手数・余詰・手順を対象としており、全駒の必要性まで独立して確認したものではなかった。その不足を補ったのが、後述する全1枚除去の監査である。

## 3. 標準CLIをPythonから直接並列実行する

### ビルドとエンジン設定

以下のコマンドはリポジトリのルートで実行する。

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target shogiboardq-cli --parallel 4
./build/shogiboardq-cli list-engines
```

`generate-tsume --engine` には `list-engines` に表示される**登録名**を指定する。実行ファイルのパスではない。今回の登録名は `KomoringHeights  1.1.0 64ZEN2` で、`KomoringHeights` の後の空白は2つだった。使用環境に合わせて変更する。

エンジンには `Threads=1`、`USI_Hash=256`、`PostSearchLevel=MinLength`、`GenerateAllLegalMoves=true` を設定した。設定はGUIのエンジン管理などで用意してから起動する。Linuxでは作業用の `XDG_CONFIG_HOME` の下に `ShogiBoardQ/ShogiBoardQ.ini` を用意し、エンジン登録・オプションをコピーして全子プロセスで同じ設定を使った。既存の登録をそのまま使う場合は `XDG_CONFIG_HOME` の指定は不要である。

初回生成の並列数は主に8、一部の長手数では16だった。16プロセスで各256MiBのハッシュを確保するとハッシュだけで約4GiBになり、ほかにCLIや探索のメモリも必要になる。エンジン内のスレッド数と外側のワーカー数を合わせてCPU・メモリに収まるよう調整する。

### 1バッチの起動と出力形式

```bash
./build/shogiboardq-cli generate-tsume \
  --engine 'KomoringHeights  1.1.0 64ZEN2' \
  --target-moves 3 --max-positions 100 \
  --timeout-ms 5000 --max-attack 4 --max-defend 1 \
  --attack-range 3 --stdin-control > tsume-batch.jsonl
```

| オプション | 意味 |
|---|---|
| `--target-moves` | 目標手数。1～19の奇数 |
| `--max-positions` | 1回の起動で生成する最大件数。1～100 |
| `--timeout-ms` | 候補探索・トリミング・余詰検査に使う予算。500～60000ms。プロセス全体の実行時間ではない |
| `--max-attack` | 攻方の盤上・持駒の合計上限。1～10 |
| `--max-defend` | 玉以外の玉方盤上駒の上限。0～10 |
| `--attack-range` | 玉を中心とする攻め駒の配置範囲。1～8マス |
| `--stdin-control` | 標準入力の `stop\n` またはEOFで停止する |

未使用駒を玉方持駒へ回す設定と最終手の複数解を許容する設定は既定値を使った。事前選別の数msと `--timeout-ms` の数秒は別の予算である。

標準出力はJSON Linesで、`started`、`progress`、`position`、`finished`、`error` などが1行1JSONで届く。`position` の `sfen` と `pv`（USI指し手の配列）を取り出し、`sfen + " moves " + " ".join(pv)` で保存する。`progress` には生成数・発見数・除外数・判定不能数などが含まれ、設定比較に使える。

`--stdin-control` を使う場合は標準入力を開いたまま保つ。標準入力が初めから閉じている実行環境では早期に停止するため、Pythonでは `stdin=PIPE` とし、必要数が揃った時点で停止要求を送る。

### 並列収集のPython例

以下を `generate_tsume_parallel.py` として保存し、リポジトリのルートで `python3 generate_tsume_parallel.py` を実行する。Python標準ライブラリだけを使用する。

この例は**標準CLIからの収集とSFENの重複除外**を示す。一時実行版の近傍探索、問題集全体の類似判定、収集後の独立監査は含まない。出力は最終検証前の候補ファイルであり、1000件収集できたことと、公開可能な1000題が完成したことは区別する。

```python
import asyncio
import json
import os
from pathlib import Path

CLI = str(Path("build/shogiboardq-cli").resolve())
ENGINE = "KomoringHeights  1.1.0 64ZEN2"  # list-engines の登録名
PLIES = 3
TARGET = 1000
WORKERS = 8
OUTPUT = Path(f"tsume-work/tsume_{PLIES}ply_candidates.txt")
ENV = dict(os.environ)
# 別途準備した設定を使う場合（Linux）:
# ENV["XDG_CONFIG_HOME"] = "/path/to/tsume-config"

positions = {}
active = set()


def remember(sfen, pv):
    fields = sfen.split()
    if len(fields) != 4 or len(pv) != PLIES:
        raise ValueError("SFEN または手順長が不正です")
    key = " ".join(fields[:3])  # 盤面・手番・持駒。開始手数を除く
    if key not in positions and len(positions) < TARGET:
        positions[key] = sfen + " moves " + " ".join(pv)


def checkpoint():
    temporary = OUTPUT.with_suffix(".tmp")
    text = "# CLI収集結果（最終検証前）\n"
    text += "\n".join(positions.values()) + "\n"
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(OUTPUT)


async def stop(process):
    if process.returncode is None:
        try:
            process.stdin.write(b"stop\n")
            await process.stdin.drain()
        except (BrokenPipeError, ConnectionResetError):
            pass


async def close_process(process):
    await stop(process)
    try:
        await asyncio.wait_for(process.wait(), timeout=15)
    except asyncio.TimeoutError:
        process.kill()
        await process.wait()
    active.discard(process)


async def worker(number):
    while len(positions) < TARGET:
        args = [
            CLI, "generate-tsume", "--engine", ENGINE,
            "--target-moves", str(PLIES),
            "--max-positions", str(min(100, TARGET - len(positions))),
            "--timeout-ms", "5000",
            "--max-attack", "4", "--max-defend", "1",
            "--attack-range", "3", "--stdin-control",
        ]
        log_path = OUTPUT.parent / f"worker-{PLIES}ply-{number}.jsonl"
        err_path = OUTPUT.parent / f"worker-{PLIES}ply-{number}.stderr.log"
        with log_path.open("a", encoding="utf-8") as log, err_path.open("ab") as err:
            process = await asyncio.create_subprocess_exec(
                *args, env=ENV,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE, stderr=err,
            )
            active.add(process)
            try:
                # 他のワーカーが起動待ちの間に目標数へ達した場合にも停止する。
                if len(positions) >= TARGET:
                    await stop(process)
                async for raw in process.stdout:
                    line = raw.decode("utf-8")
                    log.write(line)
                    log.flush()
                    event = json.loads(line)
                    if event["event"] == "error":
                        raise RuntimeError(event["message"])
                    if event["event"] != "position":
                        continue
                    remember(event["sfen"], event["pv"])
                    checkpoint()
                    print(f"{len(positions)}/{TARGET}", flush=True)
                    if len(positions) == TARGET:
                        await asyncio.gather(*(stop(p) for p in list(active)))
                if await process.wait() != 0:
                    raise RuntimeError(f"CLI が異常終了しました: {err_path}")
            finally:
                await close_process(process)


async def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if OUTPUT.exists():
        for line in OUTPUT.read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                sfen, moves = line.split(" moves ", 1)
                remember(sfen, moves.split())
    tasks = [asyncio.create_task(worker(i)) for i in range(WORKERS)]
    try:
        await asyncio.gather(*tasks)
    finally:
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        await asyncio.gather(*(close_process(p) for p in list(active)))
        checkpoint()
    print(f"収集完了: {OUTPUT}（最終検証は別途実施）")


if __name__ == "__main__":
    asyncio.run(main())
```

各ワーカーがCLIを起動し、最大100題のバッチが終わったら不足分のバッチを起動する。CPUを使う探索は子プロセスで動き、Pythonは `asyncio` でJSON Linesを読み取る。共通辞書への追加と途中保存の間には `await` がないため、この単一イベントループ内では更新が割り込まれない。

盤面・手番・持駒を共通キーにし、開始手数だけが違うSFENも同じ局面として扱う。発見のたびに一時ファイルから置換して保存し、再実行時に読み戻す。全CLIへ停止を依頼した後に届いた余分な候補も1000件を超えて追加しない。JSON Linesと標準エラーは手数・ワーカー別に残す。

`PLIES` を3・5・7・9・11・13に変えて実行すれば手数別の候補を保存できる。ここでの駒数・探索時間は説明用で、長手数を効率よく1000題生成できる固定設定ではない。手数ごとに調整し、複数の収集スクリプトを同時起動する場合は総ワーカー数を数える。同じ出力ファイルを複数の収集スクリプトから同時に更新しない。

実作業では、バッチごとの設定、採択数、重複数、判定不能数も記録した。条件変更時はCLIを停止し、収集済み局面を保持して再開した。13手詰の後半では `position` を受信して重複を除いた後、常駐する独立検証プロセスへ `SFEN moves …` を送り、最短手数と合法手順が確認できた場合だけ辞書へ追加した。

## 4. 作成後に分かった問題と改善

### 不要駒を時間切れのまま残さない

初回13手詰の旧703番には、除去しても13手詰が成立する4三桂・2九香が残っていた。生成器が駒除去後の時間切れを「その駒を残す」扱いにしていたことと、保存後の全駒の必要性を再確認していなかったことが原因だった。

生成器を修正し、除去判定が未完了なら採択しないようにした。保存済み問題についても、盤上の玉以外の駒と攻方持駒を1枚ずつ玉方持駒へ移し、最短手数と主手順の一意性を保てるかを調べた。除去できた場合は全駒を改めて調べ、最終SFENで重複を除いた。時間切れ・判定不能・検査間の不一致は合格に数えず、補充した。

この段階の6000題で調べた1枚除去は25,468通りだった。不要駒を整理すると、初回13手詰1000題は重複排除・判定不能の除外後に5局面しか残らず、995題を補充する必要があった。元の局面に駒を足すだけで件数を増やす方法の限界が、ここでも明らかになった。

この検査が保証するのは、指定手数・主手順の一意性などの採択条件を保てる**1枚除去が存在しないこと**である。複数枚の同時除去、駒種変更、配置変更まで含めた最少駒数は保証しない。

### 局面の違いに加え、解く手順の違いを判定する

不要駒整理後の13手詰旧722・723番は、配置や駒種に差があってもUSI手順が全て一致していた。13手詰1000題全体でも手順文字列は41種類しかなかった。SFENの重複排除に加えて、[tsume_diversity.py](../../scripts/tsume_diversity.py) で次の編集基準を導入した。

- 最終手を除き、移動元・移動先・駒打ちの種類・成の有無を比較する。移動する盤上駒の種類は比較せず、金と成桂などの交換でも同じ手順を水増ししない。
- 玉を原点とする相対座標と左右反転で正規化し、平行移動や鏡像を同系統として扱う。絶対座標でも比較し、玉の初期位置だけが違う同じ手順を除外する。
- 9手以上では最初の2手と最終手を除く中間手順も比較し、同一または1手だけ違うものを除外する。
- 全ワーカー共通の索引を持ち、不要駒除去の前後で照合する。検査待ちの間に別ワーカーが採択した類似作も再確認する。
- 最終保存時には直近10題と手順座標がなるべく重ならない順序に並べる。

これは主手順に基づく保守的な編集基準である。変化・紛れを含む作品の同一性や鑑賞上の価値を完全に判定するものではなく、異なる価値を持つ作品をまとめて除外する場合もある。

## 5. 類似作の補充で加えた生成の工夫

再編時は [refill_diverse_tsume.py](../../scripts/refill_diverse_tsume.py) から、[tsumeshogi_diversity_sampler](../../tests/tsumeshogi_diversity_sampler.cpp) と [tsumeshogi_collection_auditor](../../tests/tsumeshogi_collection_auditor.cpp) を直接起動した。標準CLIとは別の収集用プログラムだが、既存の局面生成クラス・検証器を使う。

### ランダム配置・駒変更・逆算を組み合わせる

ランダム配置と種の駒変更に加え、検証済みの2手短い問題から、直前の王手と玉移動、または合駒の一組を逆算して長い候補を作った。玉移動には駒を取る場合も含めた。逆算で想定した手順をそのまま採用せず、最短手数が伸びたこと、主手順の余詰、全1枚除去を再検査した。

| 目標手数 | 逆算 | 駒変更 | ランダム配置 |
|---|---:|---:|---:|
| 13手以上 | 15% | 75% | 10% |
| それより短い手数 | 40% | 35% | 25% |

これは調整後の専用サンプラーの試行比率であり、初回CLIの10%・90%とは別である。ランダム配置では攻方上限5～8枚、玉以外の玉方上限1～3枚、配置範囲2～4マスをワーカーごとに変えた。種の駒変更・逆算には攻方9枚・玉以外の玉方4枚の共通上限を使い、他ワーカーが見つけた種を小さな上限だけで捨てないようにした。

事前選別も、初手だけでなく、主手順を復元する際の最終手を除く各攻方手番で複数の詰む手を検出するようにした。この段階で判定できない候補は実エンジンの監査へ渡す。初回の一時CLIの `Unknown` 除外と扱いが異なるが、最終的な確定検査を省略するものではない。

### 種の固定化を避ける

種を採択順の先頭200題に固定すると、新しく見つけた系統を探索に生かせなかった。そこで手数ごとに最大200題を渡し、半分を最近の採択作、残りを全履歴から巡回抽出する方式にした。巡回位置もワーカーごとにずらした。

駒変更には目標と同手数または2手短い種を使い、同手数を優先した。長手数が不足している間は、9・11手詰の異なる検証済み種を収録数の最大3倍まで保持した。追加の種は公開件数に含めず、作業用問題集には各1000題だけを書き出す。

未採択の近傍局面も各系統最大3題まで探索用に保持した。新しい手順に余詰がある候補は各手数300系統まで修正探索用に保存し、種に混ぜるのは各手数50題までにした。これらも合格件数には数えない。

余詰候補には、生成器と共有する `onePieceRemovedPositions()` で1枚除去の修正候補を作り、最大2通りを試した。修正後も最短手数・余詰・不要駒の全検査を要求した。

### 不足数と再開を管理する

不足している手数へワーカーを振り向け、残りが1種類で75%以上揃ったら、短い種を増やす担当を減らしてその手数の補充を優先した。今回の補充では14ワーカーも使用した。初回CLIの8・16並列とは異なる段階の設定である。

採択済み・除外済みSFENをサンプラーへ共有し、同じ局面の再探索を減らした。`progress.json` に採択数・不足数・処理件数を記録し、`audit.jsonl`、`checked.jsonl`、種のファイル、ワーカー別ログを残した。途中結果は一時ファイルから置換して保存した。

`run.json` には入力ファイル・監査プログラム・類似判定コードのハッシュを保存し、異なる条件で以前の作業を再開しないようにした。最終監査で外すSFENは `--exclude-sfens` から渡し、`exclusions.json` に蓄積する。再開時に元ファイルや古い合格ログから復活させず、不足分として補充する。

## 6. 現在のツールによる補充・最終監査・保存

以下は現在のツールで再編する手順である。公開済み6000題はすでに類似判定を通っているため、そのまま入力すると補充が不要で直ちに収集を終える場合がある。

入力には検証済みの問題集を使う。標準CLIの例で新たに収集した候補を使う場合は、まず `audit_tsume_collections.py` で検査し、得られた `tsume_*ply_minimal.txt` を補充の入力にする。補充ツールは入力中の既存問題すべてをその場で再監査するわけではなく、公開前の独立監査も必要である。

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build --target tsumeshogi_diversity_sampler tsumeshogi_collection_auditor --parallel 4

# 以下の --engine は、CLIの登録名ではなく実行ファイルのパス。
python3 scripts/refill_diverse_tsume.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --engine /path/to/KomoringHeights-by-gcc \
  --output-dir /tmp/tsume-diverse --workers 14 --seconds 3600
```

`progress.json` の全手数の `missing` が0になるまで同じコマンドで再開する。`--seconds` による終了は完成を意味しない。入力ファイルは再開時の照合に使うため、最終保存まで書き換えない。

完成した作業用問題集を、収集中の監査とは別プロセスで再監査する。

```bash
python3 scripts/audit_tsume_collections.py /tmp/tsume-diverse/tsume_*ply_working.txt \
  --engine /path/to/KomoringHeights-by-gcc \
  --output-dir /tmp/tsume-diverse-final --workers 8 --timeout-ms 30000
```

今回の最終監査では30,000msを使い、判定不能分を60,000msで再試行した。再試行は同じ入力・出力ディレクトリを使い、`--timeout-ms` を増やす。監査プログラムが変わった場合は新しい出力ディレクトリが必要になる。

判定不能や不合格のSFENを除外する場合は、1行1SFENのファイルを用意し、補充コマンドに `--exclude-sfens /tmp/excluded-sfens.txt` を追加する。不足を補充してから再監査する。監査で駒が除去された場合も、変更後の局面を対象に再監査し、公開する局面そのものの確定記録を得る。

保存前の完了条件は次のとおり。

1. 各手数で1000題あり、SFENが重複しない。
2. 各SFENを新しい探索表で調べ、最短手数が目標と一致する。
3. 保存する全手が合法で、攻方が連続王手し、終端が詰みである。
4. 主手順の各攻手が一意である。最終手の複数解と、早く詰む変化での変化別詰は許容する。
5. 全1枚除去を確認し、未確定がなく、監査で局面が変更されない。記録が `minimal`、`removed == 0`、`original == sfen` となる。
6. 最終監査が返した手順でも類似判定後に各1000題残る。二歩・行き所のない駒・駒種ごとの総数も確認する。

```bash
python3 scripts/publish_diverse_tsume.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --audit-logs /tmp/tsume-diverse-final/audit.jsonl \
  --output-dir /tmp/tsume-diverse-publish
```

保存ツールは各入力局面・手数について最新の監査記録を使う。後の判定不能を、以前の合格記録で上書きして採択しない。公開する各1000題に対応する独立監査のログを渡す。再試行や補充で余分な候補の記録も残った場合は、最終収録分に対応する記録を選び、古い不採択の記録だけを都合よく取り除かない。

出力件数、局面、手順、並び順、検証記録とファイルのSHA-256を確認し、公開用ディレクトリの問題集と `validation_20260926.json` を `data/tsumeshogi/` へコピーする。再編で問題番号は変わる。解答履歴は正規化したSFENをキーにしているため、同じ局面の履歴は引き継がれる。

## 7. 再編後の結果

次の数値は [validation_20260926.json](../../data/tsumeshogi/validation_20260926.json) に記録した2026年9月27日の最終結果である。「変更前」は不要駒整理後・類似作整理前を指す。

| 手数 | 類似作整理で入れ替えた局面 | 変更前のUSI手順の種類 | 更新後のUSI手順の種類 | 最終の1枚除去検査数 |
|---|---:|---:|---:|---:|
| 3 | 717 | 891 | 1000 | 3,447 |
| 5 | 307 | 930 | 1000 | 3,558 |
| 7 | 219 | 892 | 1000 | 4,147 |
| 9 | 574 | 821 | 1000 | 5,221 |
| 11 | 955 | 166 | 1000 | 5,515 |
| 13 | 984 | 41 | 1000 | 5,944 |
| 合計 | 3,756 | — | 6,000 | 27,832 |

全6000題が最短手数・合法な詰め手順・全1枚除去の監査を通り、判定不能と除去可能な駒が残る問題は0題となった。最終手順にも類似判定を適用し、全手数で各1000題を維持した。不要駒整理時の25,468通りと、類似作の入れ替え後の27,832通りは別の問題集に対する集計である。

再利用する際は、生成数だけでなく「検証済みで、不要駒と類似作を除いて残った件数」を進捗にする。ランダム配置で新しい系統を探し、種の変更と逆算で候補を広げ、採択後の独立監査で品質を確定する流れを一組として運用する。
