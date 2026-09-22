# Scene / Task寿命統合の実装・検証記録

## 基準とソースの範囲

GitHubから確認した基準は`15f5df42571dddee2a0dcaede16e0fa826a9061b`。
完全なcloneは取得できず、使用する依存部分を取得・復元してGit blob SHAを照合しました。
`Validation/verified-source.json`は今回扱った元ファイルの照合値です。省略版の本体ヘッダーで検証していません。
Payloadは変更・追加する12ファイルに限定し、検証用に復元した依存をユーザーの本体へコピーしません。

## 設計上の判断

1. SceneポインタをStep後半で比較する旧方式を廃止し、Navigatorが切替と同じ場所でScopeを保持します。
2. 次Sceneの同期初期化と未公開Scopeの確保が成功してから旧Scopeを退役します。初期化失敗では旧Scopeを保持します。
3. 退役完了を確認するまではScene停止通知を呼びません。失効した旧Scopeは終了通知と旧デストラクタの間も保持します。
4. 候補Sceneと未公開Scopeに専用の後始末ガードを置き、例外・終了要求で有効化されなかった候補も終了します。
5. Taskの反映・捕捉解放中は、直接CommitやStepを拒否します。Shutdownは要求だけを保持し、安全な境界で完了します。
6. NavigatorをApplicationのStep外から直接使う経路も考慮しました。Navigatorの通知中はApplicationの終了を遅延します。
7. 旧SceneのデストラクタからQuitが要求される経路を、SetCurrent後にも検査します。新SceneのOnEnterを呼ばず終了します。
8. TaskDispatcher.cppの退役・待機実装は変更していません。ヘッダーに所有スレッドと同期可否の観測入口を追加しました。
9. Activation/TickのContext末尾に既定値付きTaskフィールドを追加します。同期初期化のContextは変更しません。

## 実行と比較

今回の新規18件を、変更前の実装へも同じソースからビルドして実行しました。
変更前は2件成功・16件失敗（CTest終了コード8）で、コンパイルエラーや省略したAPI実装だけを失敗の代用にしていません。
新しいContextフィールドがない版でもテンプレートの検出分岐で同じテストを実行できます。
16件を独立した不具合16個とは数えません。追加契約を確認するケースも含みます。

修正後はGCC Debug / Release、Clang ASan+UBSan、GCC ThreadSanitizerで18件が通過しました。
ソースの説明・行折り返しを整えた後にも再ビルド・再実行しました。最終Releaseの反復出力にはPassedが900回あります。
準備中Taskは協調取り消しを待つゲートを用いて観測し、任意sleepだけを待機の根拠にしていません。
旧版の誤った破棄順を再現するときも、Scene外の観測データへ記録し、解放済みSceneへのアクセスをテストの仕様にしていません。

前回配布したRenderer統合器の出力に相当するApplicationの2ファイルについても、
FRenderSystem / RenderPass3Dの本体と共に18件を実行しました。空描画のフレーム境界を使うScene寿命テストであり、
前回の3D描画回帰やGPU/実SDKの検証を代替するものではありません。

今回のテストは実Applicationを実行しますが、リンクするのは必要依存に絞った構成です。
Physics / Native / Gameplay全体と既存テストの全リンク・実行を確認したものではありません。
専用CMakeの既定値には本体`dxf::framework`へリンクする経路を用意していますが、完全なRootでの実行は未確認です。

## ビルド警告

初回の依存全体-Werror試験は、変更対象外のPlatform.cppのOutという引数が同名グローバルを隠す警告で停止しました。
依存は改変せず、新規変更したRuntime実装と新規テストを警告エラー扱いに限定しました。
GCC Releaseでは既存のTVector<char>に関する最適化時のstringop-overflow警告も表示されています。
これを無条件に誤検出とも実害とも断定していません。今回のASan/UBSan経路では問題は検出されませんでしたが、
Toolbox全体の数値・メモリ境界品質を保証する根拠にはしていません。原ログに警告を残しています。

## 配布・適用器

基準の6ファイル更新と6ファイル追加。前回のRenderer統合済み版はApplicationのh/cppを一対の別ハッシュとして認識します。
未知の内容や未コミット・stage済み変更に自動マージせず停止します。UTF-8 BOM / CRLFの有無を保ちます。
退避先はリポジトリの外に作り、書き込み途中の例外は適用済み分を戻します。同時編集があればその編集を保護して退避先を通知します。
Symlink / junction / パス遡り / 異なる内容の新規ファイルとの衝突を拒否します。
適用器の18件では、dry-run・バックアップ・再適用・CRLF・対象変更・依存差異・破損Payload・途中失敗・同時編集・Renderer二形態を確認しています。
GitのHEADやindexの内容を変更せず、リポジトリのreset / stash / commit / pushは行いません。
検証用の一時Gitリポジトリにはダミー名でbaseline commitを作ります。ユーザーのリモートには書き込んでいません。

## 残す制限

- 退役待ちはScope単位ではなく全DispatcherのPrepare待機です。終わらないPrepareを強制終了しません。
- Rootの仕事はScene寿命に結び付かず、個別GameObject/Componentの先行破棄もこのScopeでは保護しません。
- OnInitializeの非同期化は行っていません。候補Sceneの独自非同期処理を外部ポインタから勝手に開始した場合は対象外です。
- 所有スレッドでの利用・破棄が必要です。TaskDispatcherより長生きする借用Navigatorなど、所有契約違反を救済するものではありません。
- Windows/MSVC、実DxLib SDK・画面・入力・音声デバイス、全体の既存CTestは未実行です。
- FVector2、アセットRoot、.dxfpaths、Assets、Physics、描画APIの数値・挙動は今回の更新対象外です。
- モデル・実ライト・Viewport・GPU計測、正式なWorld Snapshot APIはこの更新に含めません。

## ZIP展開後の再検証

新しいZIPを別ディレクトリへ展開し、照合済み元ソースを持つ使い捨てGitリポジトリに、同梱適用器でdry-run・適用・再適用を実行しました。
差分検査、適用後のReleaseビルド、18件のCTest、ZIP内適用器の18件、部分ソース103ファイルのNo-STL検査がすべて終了コード0でした。
最終ZIPのPayload・Variants・適用器・テスト用fixtureが、この再検証に使用したものとバイト一致することも照合しています。
README・AUDIT・STATUS・検証ログは結果を追記した説明用ファイルで、適用器による本体の上書き対象ではありません。
