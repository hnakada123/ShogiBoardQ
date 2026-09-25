# 依頼：Hayanagi の詰み探索（TsumeSearch）と局面処理の高速化・コード改善

このリポジトリ `/home/nakada/GitHub/Hayanagi` が Hayanagi の正式リポジトリです。`CLAUDE.md` の方針（C++17、`-Wall -Wextra -Wpedantic`、ソース・コメント・コミットメッセージは日本語）に従ってください。作業前に `git status` で状態を確認してください。未追跡の `docs/`（ビットボード解説の HTML と画像）と `build-hayanagi/` は今回の対象外なので触らないでください。

## 背景：ShogiBoardQ との関係

Hayanagi は将棋GUI **ShogiBoardQ**（`/home/nakada/GitHub/ShogiBoardQ`）にサブモジュール `ShogiBoardQ/Hayanagi` として組み込まれています。サブモジュールは現在 `main` 先頭の `30cfce58e0a863f9f2d2144635f1d9e9d4e3e54f` を指しています。ShogiBoardQ 側は `git submodule` の参照を検証済みコミットへ更新する運用なので、**このセッションでは ShogiBoardQ のファイルを変更しないでください**（必要な追従作業は最後に報告してください）。

ShogiBoardQ は Hayanagi を2通りで使っています。

1. **静的ライブラリ `hayanagi_tsume`**（`src/position.cpp` + `src/tsume.cpp`、CMake ターゲット名と `target_include_directories(... SYSTEM PUBLIC src)` に依存）を GUI アプリとテストに直接リンクしています。
   - 詰将棋対局（`ShogiBoardQ/src/analysis/tsumegamesession.cpp`）：玉方の応手生成（`generate_legal_moves`）と内蔵の詰み判定（`TsumeSearch::solve` を残り手数の深さ・制限時間付きでワーカースレッドから呼ぶ）。
   - 問題一覧の解析（`ShogiBoardQ/src/analysis/tsumepositionanalyzer.cpp`）：`solve(position, attacker, 31, ms, stop)` で最大31手まで解析し、PVを再構成。結果はキャッシュされ、キー `"hayanagi-30cfce58-depth31-v1"` に Hayanagi のコミットが含まれています。探索結果や手数の意味が変わる改良をしたら、このキーを更新する必要があると報告してください。
   - 局面生成の余詰検査（`ShogiBoardQ/src/analysis/tsumeshogiverifier.cpp`）：`Position` の合法手・王手生成で全変化を列挙し、外部エンジン（KomoringHeights）に問い合わせます。
   - 局面生成の事前選別（`ShogiBoardQ/src/analysis/tsumeshogicandidatescreener.cpp`）：ランダム局面ごとに `solve` を深さ3〜5・制限100ms で呼び、1局面あたり約1〜1.5ms で「目標手数ちょうどで詰み、詰む初手が1つ」の候補だけを残します。この呼び出し回数が最も多く、高速化の恩恵が直接出ます。
   - テスト：`tests/mock_tsume_engine.cpp`、`tests/tst_tsumeshogi_verification.cpp`、`tests/tst_tsumeshogi_screener.cpp`、`tests/tsumeshogi_generation_harness.cpp`。
2. **USI エンジン実行ファイル `hayanagi`**：ShogiBoardQ の `tst_tsume_play` が `go tsume attack/defense depth N movetime M` と `stop` をプロセス経由で実行します。

複数のワーカースレッドがそれぞれ別の `Position` / `TsumeSearch` インスタンスを同時に使います。`position.cpp` と `tsume.cpp` にグローバルな可変状態を持ち込まないでください。

## 変えてはいけない契約

以下は ShogiBoardQ が依存している公開インターフェースと意味です。変更が必要な場合は理由と移行方法を報告し、互換性を保てないなら実装しないでください。

- `shogi::Position`（`src/position.h`）：`set_sfen(sfen, tsume)`（`tsume=true` で攻方の玉がない局面を受理し、不正な SFEN は `false`）、`apply_usi_move`、`do_move`、`generate_legal_moves()`（打ち歩詰めを除いた全合法手。玉方の応手列挙に使用）、`generate_checking_moves()`（合法な王手のみ。打ち駒を含み、成・不成は別の手）、`move_to_usi`、`to_sfen()`（現在の書式のまま。持駒は先手・後手の順に R,B,G,S,N,L,P、末尾は手数）、`side_to_move`、`is_in_check`、`find_king`、`piece_at`、`hand_count`、`Move` 構造体の各フィールド、`types.h` の `square_row`/`square_col`/`piece_type`/`piece_color`。
- `shogi::TsumeSearch::solve(position, attacker, max_plies, time_limit_ms, stop)`（`src/tsume.h`）と `TsumeResult{status, move, plies, nodes}`、`TsumeStatus{Mate, NoMate, Limit, Timeout, Cancelled}`。
  - `Limit` は「指定深さ内に詰みがない」であり、不詰の証明ではありません。**深さ制限を `NoMate` として返さないこと**（`tests/test_engine.py` の `test_depth_limit_is_not_nomate` が検査）。
  - `NoMate` は玉方に逃れがあることが確定した場合だけ。`Mate` の `plies` は深さ内で見つかる最短の詰み手数で、`solve` は深さ 1,3,5,… と延ばして最初に確定した結果を返します（攻方手番なら奇数、玉方手番なら 0,2,4,…）。
  - 攻方手番の `move` は詰む手、玉方手番の `move` は最長抵抗または逃れの手。`max_plies` は 63 で丸められます。`stop` は他スレッドから立てられ、速やかに `Cancelled` で戻ること。
- USI 拡張：`setoption name TsumeMode value true`、`go tsume <attack|defense> depth N movetime M`、応答 `tsume <mate|nomate|depthlimit|timeout|cancelled|invalid> move <USI手|none> plies <n> nodes <n>`、探索中の `stop`、玉のない局面を `TsumeMode` なしで渡したときの `tsume invalid` と `bestmove resign`。
- 手の生成順：ShogiBoardQ と `tests/test_engine.py` の一部は生成順に依存する期待値を持ちます（例：逃れの手 `1h1i`、検査PVの応手 `2a1a`）。並び順を変える場合はその旨を報告し、Hayanagi 側のテストは更新してください。
- 通常対局用の `search.cpp` の挙動と、`perft`（開始局面で深さ 1:30, 2:900, 3:25470）は維持してください。

## 現状のベースライン（2026-09-25、このマシン、Release ビルド、シングルスレッド）

| 計測 | 結果 |
|---|---|
| `perft depth 4`（開始局面） | 719,731 nodes、約 7.3M nps |
| `bench nodes 200000` | 合計 761,806 nodes、約 324k nps |
| `go tsume attack depth 5`：`9/9/9/9/9/9/7+S1/5G2k/9 b RSr2b3g2s4n4l18p 1` | mate 5手、38,531 nodes、0.108 秒 |
| 同 `9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1` | mate 5手、34,569 nodes、0.077 秒 |
| `go tsume defense depth 6`：`5k3/9/9/3+P1B1N1/9/9/9/9/9 b RSrb4g3s3n4l17p 1 moves S*4b` | mate 6手、36,882 nodes、0.061 秒 |
| `go tsume attack depth 5`：`9/9/9/9/9/2k6/4r4/9/9 b RBSLb4g3s4n3l18p 1`（不詰、攻方の持駒が多い） | depthlimit、1,840,939 nodes、**4.76 秒** |
| 同 depth 3 | depthlimit、13,341 nodes、0.040 秒 |

詰み探索は約 35〜40 万 nodes/秒で、`perft` の 20 分の 1 程度です。ShogiBoardQ の事前選別では攻方が飛角銀香を持つ不詰局面で制限時間（100ms）に達して素通しになるケースがあり、ここが実用上のボトルネックです。

コードを読んだ範囲で高速化の余地がありそうな点（要確認）：

- `TsumeSearch::visit` は子局面ごとに `Position` を丸ごとコピーして `do_move` し、`do_move_unchecked` が毎手 `std::make_shared<HistoryNode>` で履歴を確保している。詰み探索では千日手履歴は不要。
- 各ノードで `generate_legal_moves()` / `generate_checking_moves()` を `std::vector` に生成し、玉方ノードでは別途 `is_in_check` も計算している。
- 置換表が `remaining` ごとの `std::unordered_map` で、`solve` ごとに作り直している。キーに持駒・手番が含まれているか、Mate/NoMate の結果を深さをまたいで再利用できるかを確認。
- 手の並べ替え（取る王手・玉に近い打ち・成りを先に、合駒の順序など）や、1手詰の即時判定がない。
- `generate_legal_moves` の駒種ごとの 13 回の `generate_piece_moves` 呼び出しなど、`position.cpp`（1,702 行）に整理の余地がある。

## 依頼内容

1. **詰み探索の高速化**：上のベースライン局面で同じ結果（status・plies・可能なら move）を返したまま、探索速度を上げてください。目標は 5 手詰の 3 局面で 3 倍以上、不詰局面（depth 5）で 5 倍以上ですが、正確さを優先してください。手段は問いませんが、`Limit`/`NoMate` の区別、最短手数の保証、`stop` の応答性、スレッドごとの独立性を必ず保ってください。df-pn など探索方式を変える場合も、公開 API と意味を変えない範囲で行ってください。
2. **局面処理の高速化**：`Position` のコピー・履歴確保・合法手生成のうち詰み探索で効いている部分を改善してください（例：make/unmake、履歴の省略、王手生成の直接列挙、割り当ての削減）。`perft` の結果は変えないこと。
3. **コード改善**：重複の整理、責務の分割、`-Wshadow -Wconversion` でも警告が出ない書き方への修正、詰み探索のベンチマーク用コマンド（例：`bench tsume` か、上記局面を並べた `tests/` のスクリプト）の追加、C++ の単体テストまたは Python テストの拡充（`Limit`/`NoMate` の区別、`stop`、打ち歩詰め、玉なし局面、持駒の多い局面）。
4. **検証**：
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   python3 tests/test_engine.py build/hayanagi
   ```
   改良前後で上のベースライン表と同じ局面を計測し、nodes と秒数を README か `docs/` 以外の適切な文書（例：`BENCHMARK.md`）に表として残してください。
5. **成果物**：日本語のコミット（機能ごとに分ける）、README の更新、API に変更がある場合はその一覧と移行方法。最後に ShogiBoardQ 側で必要な追従作業（サブモジュール参照の更新コミット、`tsumepositionanalyzer.cpp` のキャッシュキー更新、生成順が変わった場合のテスト修正など）を箇条書きで報告してください。

## ShogiBoardQ 側での確認手順（参考、このセッションでは実行しない）

```bash
cd /home/nakada/GitHub/ShogiBoardQ
git -C Hayanagi fetch origin && git -C Hayanagi checkout <新しいコミット>
cmake --build build
ctest --test-dir build -R 'tst_tsume|tst_tsumeshogi'
python3 Hayanagi/tests/test_engine.py build/Hayanagi/hayanagi
./build/tests/tsumeshogi_generation_harness --engine /home/nakada/shogi/KomoringHeights-kh-v1.1.0/source/KomoringHeights-by-gcc --count 4000 --screen
```
