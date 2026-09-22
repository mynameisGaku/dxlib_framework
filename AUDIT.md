# TaskDispatcher回復更新・検証記録

## 起点と未適用状態

利用者から「前回のダウンロードリンクが無効で、TaskDispatcher更新を適用できていない」と通知を受け、
前回更新を前提にしないパッケージを作り直しました。前回ZIPを同一内容として再配布したものではありません。
GitHubのmainは4868dc9811d60e227aa0d7c4d2c18410388d3d63でした。

元のTaskDispatcherの2ファイルはGitHub取得内容を保存し、Git blob SHAで一致を確認しました。

- TaskDispatcher.h: `2802e6d7f9e3cdb9dc8012ac1eabc40564a1c9ff`
- TaskDispatcher.cpp: `bdbf369e5b4fd5efc2c6a9741b89b5b807403de6`

Toolbox依存は回収できたソースから必要なものだけを取り出し、同コミットのGitHub treeのblob SHAと照合しました。
Tests/Support/Test.hとCheckNoStl.pyも個別に照合しました。MANIFEST.jsonに照合値があります。
**完全なチェックアウトは取得できていないため、全体ビルドを行ったとは扱いません。**
依存の代用品や省略ヘッダーを実装Payloadへ入れていません。

## 修正判断

1. PrepareがCanceledを返した記録を終端として回収し、後続Commitを進めます。
2. 停止・取り消しの検査を生成と投入のMutex区間へ置きました。
3. TVector::EmplaceBackは再確保前に一時値を作るため、捕捉を持つ記録を直接渡すだけでは、
   確保失敗時に一時値の捕捉がMutex内で破棄されます。空記録を確保してから移動代入する形へ修正しました。
   既存記録の再配置はnoexcept移動であることをstatic_assertで検査します。
4. 提出途中を別カウンターで追跡し、Fence登録前の隙間も停止待ちに含めました。
5. 実行中の記録は取り消し後も準備終了まで保持し、保留件数の枠を早く解放しません。
6. Prepareの捕捉をMutex外で解放してからReadyを公開します。
7. 取り消し・反映・投入撤回・終了の捕捉破棄をMutex外へ移し、待機の再入を拒否しました。
8. 親の世代を保存し、親/子Scopeが再利用されても旧世代操作が新Scopeへ伝播しないようにしました。
9. RetireScopeを追加しました。ただし全DispatcherのPrepareを同期する保守的な実装です。

## RedとGreen

最終26件を元のTaskDispatcherでもビルド・実行しました。
元ソースは5件通過、15件失敗、6件タイムアウトで、CTest終了コードは8です。
API未実装による失敗も含むので、これを「21個の独立した不具合」とは数えません。
タイムアウトは各ケース10秒、テストを別プロセスへ分離しています。

修正後はGCC Debug、GCC Release、Clang ASan/UBSan、GCC ThreadSanitizerでそれぞれ26件通過しました。
Releaseの各ケースを50回ずつ反復したログには1,300件のPassedがあり、全体終了コード0を確認しました。
ゲートと条件変数で準備・捕捉の寿命を観測し、任意のsleepだけを根拠とする待機検証にはしていません。
複数Producer/停止のストレスだけで、すべてのスケジュールを網羅したとは主張しません。

開発中には、新テストの初期化警告と、最初の修正版に残った確保失敗時の再入停止も検出しました。
これらを修正してから最終プロファイルを実行しています。初期ログと最終ログは区別して保存しています。
成功数を過去の更新から転記していません。

## 適用器

対象6ファイルだけを検査して更新し、Toolboxや無関係なApplicationの修正はコピーしません。
15件のテストで、dry-run、適用、同一内容の再実行、変更前退避、stage済み変更の拒否、未コミット変更の拒否、
未追跡ファイルとの衝突、依存差異、依存欠落、Payload破損、CRLF、途中失敗の差し戻し、
検査後の変更、symlink、ルート外指定/パス遡り等を確認しました。
初期のCRLF試験でGitのstatキャッシュによる見かけ上の変更を検出したため、内容差分とindex差分を分けて確認しています。
実適用中に対象を別プロセスで編集しないことが前提です。電源断を含む一括原子性は保証しません。

## 残る作業

- Application/SceneNavigatorから、SceneのOnExit/OnDeinitialize/破棄より前に退役する接続。
- Scene準備失敗時に旧Scene/Scopeを維持する実Application回帰。
- Scope単位の待機範囲の分離。現状は無関係なPrepareも待つ。
- Windows/MSVC、実DxLib SDK、フレームワーク全体と既存テストの検証。

この更新ではApplication/Renderer統合を適用済みとは仮定していません。
FVector2、sln基準アセットRoot、.dxfpaths、Assets原本、Physics数値処理、描画APIは変更していません。
GitHubへのcommit/pushも行っていません。新しいモデル・ライト・Viewport・GPU計測機能は含みません。

## 配布の再検証

ZIPを新しいディレクトリへ展開し、同梱の使い捨て旧版fixtureへdry-run、適用、再適用、
差分検査、Release生成・ビルド・26件のCTest、15件の適用器テストを実行しました。すべて終了コード0です。
配布用差分の初回チェックでは新規ファイル属性と字下げを修正し、その後のgit apply --checkも通過しました。
最終配布物のInstaller、MANIFEST、全Payload、差分、テスト適用器が再検証した内容と一致することを確認しました。
これは完全なリポジトリではなく、専用ターゲットに必要な元ファイルを持つfixtureでの再検証です。
