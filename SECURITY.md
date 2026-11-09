# セキュリティの報告 / Security Policy

## 脆弱性を見つけたら

公開の Issue には書かず、GitHub の [Private vulnerability reporting](https://github.com/hnakada123/ShogiBoardQ/security/advisories/new) で報告してください。
内容を確かめ、直してから公開します。

報告には次を書いてください。

- 影響する版（「バージョン情報」の版、またはコミット）と OS
- 再現の手順と入力（細工した棋譜・定跡・CSA サーバーの応答など）
- 想定する影響

## 対象

最新のリリースと `main` だけを対象にします。

次の入力は信頼しない前提で作っています。

- 棋譜・定跡・局面集・詰将棋集のファイルと、貼り付けた文字列
- CSA サーバーからの受信（平文の通信は途中で書き換えられる前提）
- MCP サーバーの制限モードでの AI エージェントの要求

次は対象外です。

- 利用者が登録した USI エンジンの動作（エンジンは利用者の権限で動くプログラムです）
- MCP サーバーの非制限モード（`SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1`）でできること（任意のコードの実行に等しい権限を渡します）

---

## Reporting a vulnerability

Please do not open a public issue. Use GitHub's [Private vulnerability reporting](https://github.com/hnakada123/ShogiBoardQ/security/advisories/new) instead, and include the affected version and OS, steps and input to reproduce, and the expected impact.

Only the latest release and `main` are supported. Registered USI engines (programs that run with your privileges) and the MCP server's unrestricted mode (`SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1`, equivalent to arbitrary code execution) are out of scope.
