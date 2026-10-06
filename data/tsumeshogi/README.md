# 詰将棋問題集

3・5・7・9・11・13手詰を各1000題収録しています。UTF-8のテキストで、`#` 始まりはコメント、各問題は `SFEN moves USI指し手…` の1行です。

## 2026年10月1日版：駒余りの検査と差し替え

`tsume_{3,5,7,9,11,13}ply_1000_20261001.txt` が駒余り検査を追加した新版です。20260926版の6ファイルは収録していません。その監査記録（`validation_20260926.json`）はアプリに内蔵しているため、旧版のファイルも検証済みとして読み込めます。残した局面の問題番号を維持し、除外した問題の番号に別の局面を入れています。解答履歴はSFENをキーにしているため、残した局面の履歴は引き継がれます。

旧版の保存手順を全件再生すると、手順中に取った駒を含めて攻方の持駒が余る問題が1,835題ありました。9手詰14番も歩1枚が余ります。

| 手数 | 旧版の保存手順で駒余り | 新版で差し替えた問題 |
|---|---:|---:|
| 3 | 202 | 361 |
| 5 | 244 | 318 |
| 7 | 288 | 329 |
| 9 | 342 | 400 |
| 11 | 354 | 406 |
| 13 | 405 | 435 |
| 合計 | 1,835 | 2,249 |

新版では保存手順だけでなく、**最長抵抗の全応手と、最終手の全ての詰め方で、攻方の持駒が空になること**を要求します。別の主手順・最終手で駒が余る400題も保守的に除外しました。長い駒余り変化と短い駒余りのない変化が併存する問題は、解答手順を書き換える代わりに別問題へ差し替えています。これにより、採択作では最長抵抗と駒余り回避の選択が食い違いません。短い変化での駒余りまで一律に禁止するものではありません。

再検証が確定しなかった旧版の14題も含め、計2,249題を差し替えました。公開する6000題は別プロセスで最終監査を行い、詰め上がり21,723局面の持駒と、28,177通りの1枚除去を確認しました。判定不能のまま採択した問題はありません。

最短手数、連続王手、全指し手の合法性、主手順の余詰、全1枚除去による不要駒、同手数内の重複・類似作も従来どおり検査しています。新規候補は局面変更・逆算・ランダム配置・検証済み手順の途中局面から作成し、いずれも同じ監査を通します。時間切れや判定不一致を合格には数えません。

新版のファイルハッシュ、差し替え番号、検査件数は `validation_20261001.json` に記録しています。各問題の独立監査と全1枚除去の判定理由は `audit_20261001.jsonl.gz` に保存しています。

再検査・補充の例（エンジンのパスは環境に合わせて指定）:

```bash
cmake --build build --target tsumeshogi_collection_auditor tsumeshogi_diversity_sampler --parallel 4
python3 scripts/tsume_surplus.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --output-dir /tmp/tsume-hands
python3 scripts/audit_tsume_collections.py /tmp/tsume-hands/tsume_*ply_clean.txt \
  --engine /path/to/KomoringHeights-by-gcc --output-dir /tmp/tsume-hands-audit \
  --workers 8 --timeout-ms 60000 --require-no-surplus
python3 scripts/refill_diverse_tsume.py /tmp/tsume-hands-audit/tsume_*ply_minimal.txt \
  --engine /path/to/KomoringHeights-by-gcc --output-dir /tmp/tsume-hands-refill \
  --workers 14 --seconds 3600 --require-no-surplus \
  --exclude-sfens /tmp/tsume-hands/excluded-originals.txt
python3 scripts/audit_tsume_collections.py /tmp/tsume-hands-refill/tsume_*ply_working.txt \
  --engine /path/to/KomoringHeights-by-gcc --output-dir /tmp/tsume-hands-final \
  --workers 8 --timeout-ms 60000 --require-no-surplus
python3 scripts/publish_diverse_tsume.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --audit-logs /tmp/tsume-hands-final/audit.jsonl --output-dir /tmp/tsume-hands-publish \
  --date 20261001 --require-no-surplus --preserve-order
```

補充は全手数の `missing` が0になるまで再開します。最終監査で除外が出た場合も、そのSFENを除外一覧へ追加して補充・再監査します。公開処理は各手数1000題の確定監査、駒余り検査の証明、手順の再生、類似判定が揃わなければ出力しません。`--require-no-surplus` は問題集の監査・補充用の追加条件で、通常のGUI生成設定の既定値は変更していません。

## 20260926版の作成記録

ランダム配置・生成済み局面の変更などの試行錯誤と、CLIの `generate-tsume` をPythonから直接並列実行するコードは [詰将棋6000題の生成・検証・再編の記録](../../docs/dev/tsumeshogi-collection-generation.md) にまとめています。

詰将棋対局では、アプリに同梱した監査記録と内容が一致する6ファイルの最短手数・手順を再利用し、初回の手数確認の探索を省略します。改名・移動は可能ですが、編集すると通常の解析に戻ります。履歴の一括取得・SFEN解析の重複削減・解析キャッシュの再利用は他の問題集にも有効です。詳細は [詰将棋対局の読み込みと適用範囲](../../docs/dev/tsume-play.md#読み込みの軽量化と適用範囲) を参照してください。

## 不要駒の再検査

初回作成分の13手詰703番に、除去しても同じ13手詰が成立する4三桂・2九香が残っていました。生成器が除去後の時間切れを「駒を残す」扱いにしていたことと、保存後に各駒の必要性を再検査していなかったことが品質上の問題でした。

2026年9月27日に、全6000題に対して盤上の玉以外の駒と攻方持駒を1枚ずつ玉方持駒へ移し、目標手数と主手順の一意性を保てるかを再検査しました。除去できる駒があれば取り除いて全駒を再検査し、除去後のSFENで重複を排除しました。時間切れ・判定不能・検査間の判定不一致は合格に数えず、不足分を新しく生成して同じ検査に通しました。この不要駒整理の時点の6000題について確認した1枚除去は計25,468通りです。その後の類似作整理では入れ替えた問題を含めて再検査し、最新の集計を `validation_20260926.json` に記録しています。

| 手数 | 初回1000題を整理した後の異なる局面数 | 判定不能で除外 | 整理後の重複 | 新規補充 |
|---|---:|---:|---:|---:|
| 3 | 994 | 5 | 1 | 6 |
| 5 | 991 | 7 | 2 | 9 |
| 7 | 992 | 8 | 0 | 8 |
| 9 | 992 | 8 | 0 | 8 |
| 11 | 583 | 1 | 416 | 417 |
| 13 | 5 | 3 | 992 | 995 |

この不要駒整理の時点で問題番号を付け直し、旧703番は桂・香を除いた局面へ統合しました。その後の類似作整理でも番号が変わるため、旧番号は現在の出題番号とは一致しません。

## 採択条件

- Hayanagiの有限深さ探索で最短手数を確認する。局面ごとに新しい探索表を使う。
- 保存する全指し手が合法で、攻方が連続して王手し、最終局面が詰みであることを確認する。
- KomoringHeightsと`TsumeshogiVerifier`で主手順の余詰を検査する。最終手の複数解と、早く詰む変化の変化別詰は許容する。
- 盤上の玉以外の駒・攻方持駒について、1枚除去後に同じ条件の詰将棋が成立しないことを全件確認する。未使用駒と除去した駒は玉方持駒とする。
- 二歩・行き所のない駒・駒の総数・SFENの重複も確認する。

不要駒の検査が保証するのは、上記の条件を維持できる1枚除去がないことです。複数枚の同時除去・駒種変更・配置変更まで含めた最少駒数は保証しません。類似作は次の別の編集基準で選別します。

## 類似作の選別

不要駒除去後の13手詰722・723番は、駒種や配置に違いがあっても13手のUSI手順が全て一致していました。1000題全体でも手順は41種類に集中していたため、局面の重複検査に加えて、解く手順による類似判定を導入しました。

- 最終手を除いた移動元・移動先・駒打ちの種類・成の有無を比較する。盤上駒の種類を比較しないため、金・成桂などの交換で同じ手順を水増ししない。
- 玉を原点とする相対座標と左右反転の正規化を使い、平行移動・鏡像も同系統として扱う。玉の初期位置だけが違う同じ手順も除外する。
- 9手以上では、初めの2手と最終手を除いた中間手順も比較し、違いが1手以内のものは代表作にまとめる。
- 不要駒除去の前後で、全ワーカー共通の採択済み問題と照合する。
- 保存時は直近10題と手順座標がなるべく重ならない順序に並べる。

2026年9月27日の類似作整理で計3,756題を入れ替え、各1000題・合計6000題を維持しました。

| 手数 | 入れ替えた局面数 | 変更前のUSI手順の種類数 | 更新後のUSI手順の種類数 |
|---|---:|---:|---:|
| 3 | 717 | 891 | 1000 |
| 5 | 307 | 930 | 1000 |
| 7 | 219 | 892 | 1000 |
| 9 | 574 | 821 | 1000 |
| 11 | 955 | 166 | 1000 |
| 13 | 984 | 41 | 1000 |

全6000題を別プロセスで最終検証し、前記の採択条件で計27,832通りの1枚除去を確認しました。判定不能のまま残した問題はありません。最終検証で得た手順にも上記の類似判定を適用し、全手数で1000題ずつが通過しています。

この判定は主手順を使った保守的な選別です。変化・紛れを含む作品の同一性や、鑑賞上の多様性を完全に判定するものではありません。同じ手順でも異なる価値を持つ問題を除外する場合があります。

更新後は問題集を読み込み直してください。再編で問題番号は変わりますが、解答履歴は番号ではなく正規化した局面SFENをキーに保存されるため、同じ局面の履歴は引き継がれます。差し替えた局面は別の問題として扱われます。

### 補充から保存まで

```bash
cmake --build build --target tsumeshogi_diversity_sampler tsumeshogi_collection_auditor --parallel 4
python3 scripts/refill_diverse_tsume.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --engine /path/to/KomoringHeights-by-gcc \
  --output-dir /tmp/tsume-diverse --workers 10 --seconds 3600
```

`progress.json` の `missing` が全て0になるまで、同じコマンドで再開できます。時間切れは完成を意味しません。元の問題集は書き換えず、確定検査を通った結果だけを作業用ファイルに保存します。
候補はランダム配置・駒変更・直前の王手と玉移動・合駒の逆算から生成します。各系統の近傍も最大3題まで探索用に保持しますが、検証済み件数には含めません。新しい手順に余詰があった候補も各手数300系統まで修正探索用に保持し、種に混ぜるのは各手数50題までとします。標準CLIの `generate-tsume` とは別の収集用プログラムで、同じ局面生成クラスと検証器を使い、MCPは介しません。

完成した作業用ファイルを別プロセスで再監査します。
余詰のある新規候補については、駒を1枚除いた修正候補も検査します。修正候補も最短手数・余詰・不要駒の全検査に合格しなければ採択しません。

```bash
python3 scripts/audit_tsume_collections.py /tmp/tsume-diverse/tsume_*ply_working.txt \
  --engine /path/to/KomoringHeights-by-gcc \
  --output-dir /tmp/tsume-diverse-final --workers 8 --timeout-ms 30000
python3 scripts/publish_diverse_tsume.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --audit-logs /tmp/tsume-diverse-final/audit.jsonl --output-dir /tmp/tsume-diverse-publish
```

公開用出力は各1000題の確定判定、監査による変更なし、類似判定後の件数を要求します。判定不能・類似作で不足した場合は補充してから再監査します。出力と検査記録を確認し、公開用ディレクトリのファイルを `data/tsumeshogi/` にコピーします。

最終監査で除外する局面は、1行1SFENのファイルを作り、補充コマンドに `--exclude-sfens /tmp/excluded-sfens.txt` を追加して再開します。除外は作業ディレクトリの `exclusions.json` に蓄積され、次回この引数を省いても復活しません。採択数から除いて不足分を生成し直します。最終保存には、公開する1000題に対応する独立監査の記録を使用してください。

## 再検査の実行

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build --target tsumeshogi_collection_auditor --parallel 4
python3 scripts/audit_tsume_collections.py data/tsumeshogi/tsume_*ply_1000_20260926.txt \
  --engine /path/to/KomoringHeights-by-gcc \
  --output-dir /tmp/tsume-audit --workers 8 --timeout-ms 30000
```

`summary.json`の各手数が`minimal: 1000`、`unique_minimal: 1000`、`changed: 0`であることを確認します。全1枚除去の理由は`audit.jsonl`の`checks`に記録されます。`unknown`は判定不能であり、問題の不成立や駒の必要性を証明したものではありません。

保存ファイルのハッシュと最終検査の集計は`validation_20260926.json`に記録しています。
