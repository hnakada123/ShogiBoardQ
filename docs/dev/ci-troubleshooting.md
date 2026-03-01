# CI トラブルシューティング

## Sanitizer ジョブの失敗

### AddressSanitizer (ASan)
- **heap-buffer-overflow**: 配列の境界外アクセス
- **use-after-free**: 解放済みメモリへのアクセス
- **stack-buffer-overflow**: スタック変数の境界外アクセス

### UndefinedBehaviorSanitizer (UBSan)
- **signed-integer-overflow**: 符号付き整数のオーバーフロー
- **null**: NULL ポインタのデリファレンス
- **alignment**: アラインメント違反

### 対処方法
1. ローカルで再現: `cmake -B build -S . -DENABLE_SANITIZERS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build && cd build && ctest --output-on-failure`
2. `ASAN_OPTIONS=detect_leaks=0` でリーク検出を無効化済み（Qt 内部のリークを回避）
3. Qt 内部の vptr 警告は `-fno-sanitize=vptr` で抑制済み

## Include カウント監視の失敗

`check-include-counts.sh` が失敗した場合、指定ファイルの `#include` 数がベースラインを超過している。

### 対処方法
1. 不要な include を削除する（前方宣言で代替できないか検討）
2. やむを得ない増加の場合は `resources/include-baseline.txt` のベースラインを更新する

## 翻訳同期チェックの警告

`check-translation-sync.sh` が警告を出す場合、`tr()` 文字列が変更されているが `.ts` ファイルが更新されていない。

### 対処方法
```bash
cmake --build build --target update_translations
```
実行後、`.ts` ファイルの未翻訳エントリに翻訳を追加する。
