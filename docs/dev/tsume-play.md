# 詰将棋対局

「対局 → 詰将棋対局…」で専用ウィンドウを開き、「局面集を開く…」から
テキストファイルを選択する。問題の選択欄で局面を選び、盤上の駒または持ち駒、
移動先の順にクリックして解く。成りを選べる場合は確認画面が開く。
将棋盤は通常画面と同じ `ShogiView` を使用している。

- 1行1局面のSFENを読み込む。`sfen` / `position sfen` 接頭辞、UTF-8 BOM、空行、
  `#` コメント、CRLF、タブ区切りに対応する。
- `moves ...` は参考手順として分離し、開始局面へ適用しない。
  正解判定は参考手順との一致ではなく、Hayanagiの探索で行う。
- 攻め方の玉は省略できる。攻め方が後手の局面にも対応し、盤面を反転表示する。
  余った駒を玉方へ補完する処理はなく、SFENに記載された持ち駒をそのまま使う。
- 選択時に最短の詰み手数を求め、その手数以内に詰ませる問題として出題する。
  別解や早詰めも認め、玉方は最長に抵抗する合法手を指す。
- 間違った王手には逃れる応手を盤上で指し、盤面を確認できるよう1秒後に通知する。
  待機中に一手戻す・問題切替・画面を閉じる操作をした場合は通知を取り消す。
  完全な不詰と、残り手数以内の詰みがない場合は表示を分ける。
  例えば5手詰の問題を7手で詰ませる手は「残り手数以内では詰まない」と判定する。
- 王手でない手、二歩、打ち歩詰め、自玉の王手放置は入力を拒否する。
- 時間切れと深さ上限は不正解扱いにしない。「再判定」で時間を増やして再探索できる。
  初期探索の上限は31手、既定の判定時間は5秒。時間は1〜600秒で変更できる。
- 「一手戻す」は攻め方の直前の着手前へ戻す。「最初から」は出題局面へ戻す。
  探索の中止、問題の切替、ウィンドウの終了時には探索をキャンセルする。
- ウィンドウサイズ、盤のマスサイズ、最後のファイル・問題番号、判定時間を保存する。
  盤は Ctrl+ホイールで拡大・縮小できる。

## 画面例

![詰将棋対局](../images/tsume-play/tsume-play.png)

![逃れの応手](../images/tsume-play/tsume-refutation.png)

## 構成

`Hayanagi/src/tsume.{h,cpp}` は王手のOR節点と全応手のAND節点からなる
反復深化探索。詰み、不詰、深さ上限、時間切れ、キャンセルを区別する。
深さ別の置換表を用い、打ち歩詰めを含む合法手生成はHayanagiの `Position` を共有する。
通常将棋の `set_sfen` は引き続き双方の玉を要求し、詰将棋では明示的に省略を許す。

`TsumeGameSession` がワーカースレッドへ局面のコピーを渡し、GUIスレッドで結果を適用する。
`TsumeCollection` は局面集の解析、`TsumePlayDialog` は表示と操作を担当する。
設定は `TsumeshogiSettings`（SettingsServiceのドメイン設定）に保存する。

CMakeは `Hayanagi/` を同時にビルドする。詰将棋コアをアプリへ静的にリンクするため、
エンジン登録や別プロセスの起動は不要。独立したUSI実行ファイルは
`build/Hayanagi/hayanagi` に生成される。Hayanagi側の独自コマンドは
`Hayanagi/README.md` を参照。

## Hayanagiの管理と更新

`Hayanagi/` は [独立したHayanagiリポジトリ](https://github.com/hnakada123/Hayanagi) を
参照するGitサブモジュール。導入時の参照先は、詰将棋対応を含む
`30cfce58e0a863f9f2d2144635f1d9e9d4e3e54f`。
ShogiBoardQにはHayanagiのソースをコピーして登録せず、検証済みコミットのIDを記録する。
ビルドではそのソースから詰将棋コアとUSI実行ファイルを生成する。

新しく取得するときは `git clone --recurse-submodules` を使用する。
既存のcloneやShogiBoardQの更新後は、リポジトリ直下で次を実行する。

```bash
git submodule update --init --recursive
git submodule status
```

Hayanagiの開発は独立した作業ディレクトリ（例: `/home/nakada/GitHub/Hayanagi`）で行い、
テストしてHayanagi側へコミット・pushする。その後、ShogiBoardQ側で採用する版を指定する。
以下の `30cfce5` は例なので、採用するコミットIDに置き換える。

```bash
git -C Hayanagi fetch origin
git -C Hayanagi checkout --detach 30cfce5
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
python3 Hayanagi/tests/test_engine.py build/Hayanagi/hayanagi
git add Hayanagi
git commit -m "Hayanagiの参照コミットを更新"
```

サブモジュールは通常、ブランチから離れた状態（detached HEAD）で固定する。
独立リポジトリへのpushだけではShogiBoardQの使用版は変わらず、上記の参照更新が必要。
通常の取得には `git submodule update --init --recursive` を使用し、
`--remote` による最新版への自動追従は行わない。

## 検証

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
python3 tests/gui/prepare.py
xvfb-run -a build/gui-audit/test-build/tst_tsume_play_gui
```

`tst_tsume_play` は添付例相当の5問、参考手順あり・なし、別解、完全な不詰、
手数超過、攻め方の玉の省略、後手攻め、打ち歩詰め、形式不正、
USI拡張コマンド、探索中止と問題切替、解答と待ったを検証する。
`tst_tsume_play_gui` は実際の盤クリック・成り選択・駒打ち・応手・結果表示を確認する。
