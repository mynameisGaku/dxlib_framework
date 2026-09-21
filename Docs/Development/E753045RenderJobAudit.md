# e753045 継続実装：並列描画とJobの失敗時寿命

## 起点と変更範囲

起点は `e753045c76affa2cc69e890cb70f13a9ef430172`。`Private/Dxf` / `Private/Toolbox` の配置を維持する。ProjectRoot、`.dxfpaths`、非同期Asset、Physicsの数値処理は変更しない。

今回の対象は並列描画の公開入口と再入・所有スレッド契約、およびJobの部分失敗と捕捉破棄である。フレームワーク全機能の完成を示す変更ではない。

## 公開入口

`FRenderContext::SubmitGenerated(Jobs, Count, Generate, MinimumBatch)` を追加した。既存の `FRenderQueue2D::SubmitGenerated` と同じ処理経路を利用する。

- 呼出しはキューを構築した所有OSスレッドで、Job実行区間の外側から行う。
- 各添字に専用の描画命令スロットがあり、すべてのJobが退役してから入力順に統合する。
- 通常のLayer / Orderによる安定ソートを維持し、同順位は投入された入力順を保つ。
- Generateは同じCallableを複数スレッドから呼び得る。捕捉は読み取り専用か、呼出し側で安全に設計した独立状態にする。
- Texture / Fontなどの入力資源は所有側で保持する。生成終了までRegistryの無効化や終了、入力配列の変更をしてはいけない。
- 通常のOnDrawやNative描画そのものを自動並列化するものではない。

```cpp
// Renderは所有スレッド上のFRenderContext、Jobsは共有FJobSystem。
// Positionsはこの呼出し中に変更しない入力Snapshot。
auto Result = Render.SubmitGenerated(Jobs, Positions.Size(),
    [&](Toolbox::size_t Index, Dxf::FRenderCommand& Output) -> Dxf::TResult<void>
    {
        Dxf::FRectangleCommand Command;
        const Toolbox::int32 X = Positions[Index];
        Command.Rectangle = {X, 0, X + 1, 1};
        Output = Toolbox::Move(Command);
        return {};
    });
// Resultの失敗は呼出し側のエラー経路へ伝える。
```

生成失敗・命令検証失敗・確保失敗では既存のキュー内容を変更しない。件数のオーバーフローを確保前に拒否する。Reserve後の移動が例外を投げないことを静的に検査してから一括反映する。メモリ不足でエラー結果自体の構築が失敗した場合は例外が伝播し得るが、キューの不変性とJobの完了同期は維持する。

受付中の空範囲は成功する。受付停止・不正Thread・再入の状態では空範囲でもInvalidStateとする。

## 再入・スレッドの契約

`FJobSystem::IsExecutingJob()` はOS Worker上だけでなく、同期レーンとJobの捕捉破棄中もtrueを返す。`IsInWorkerThread()` は実OS Workerの所有Systemを返す意味を維持し、別Systemの同期Jobを入れ子実行しても所有Systemは変わらない。

描画キューとRenderSystemは不正なスレッドを、変更可能なフラグを読む前に拒否する。生成中はSubmit、再生成、実行、クリア、描画先変更を通さない。RenderSystemのNative、EndFrame、CancelFrameもJob中は実行しない。

`FRenderQueue2D` は所有スレッドと参照元を持つためコピー禁止とした。キュー・Renderer自体の生成と破棄は所有スレッドで行い、破棄と他のAPIの同時実行は許可しない。不正Threadでの入力値の構築・破棄まで安全にするAPIではなく、Native資源を最後の参照ごとWorkerへ渡してよいという意味ではない。

`noexcept`の内部設定・Cancel操作は不正な呼出しでは状態を変更せず戻る。結果型を返す操作はInvalidStateを返す。同期Jobも例外扱いにはしない。

## Jobの修正

1. Fence構築はMutex / ConditionVariableの確保を含むため、誤ったnoexcept指定を外した。
2. ParallelForの複数Job投入中に例外が発生した場合、受理済みJobを完了同期してから再送出する。処理済みの添字に対する副作用のロールバックはしない。
3. Worker作成中の例外でも、開始済みWorkerへ停止通知し、JoinしてからContext / Worker配列を解放する。
4. 実行枠を捕捉のデストラクタが終わるまで保持し、捕捉破棄中の自分のFence待機を拒否する。
5. 同一スレッド上の祖先Fenceは、入れ子のJobSystemが異なっても待機を拒否する。
6. 実OS Workerの所属と、現在実行中のJob所属を別TLSで管理する。

独立した後続Jobの投入と待機は引き続き許可する。別スレッド間の任意の循環依存を自動検出する一般Task Graphではない。Job内から自身のJobSystemをShutdown / 破棄する操作は禁止のままである。

## テストと証拠の区別

実際のe753045の対象ソースと依存ヘッダーをGit blob SHAで照合した部分チェックアウトを使用した。描画バックエンドだけは呼出し順を記録するテストダブルであり、キュー・RenderSystem・Threading・JobSystemは本体実装を使う。

TDD：最初の描画8ケース中6件、Fence noexceptの1ケース、公開Context入口のコンパイル、入れ子SystemのWorker所有判定でRedを先に記録し、修正後にGreenを確認した。

追加セルフ監査：故障注入用の5系統は修正後に作成し、未修正の正確なe753045ソースでも比較実行した。これらをすべて「実装前に書いたテスト」とは扱わない。未修正側は構築と投入でSIGSEGV、Fence確保でSIGABRT、捕捉破棄と別System祖先待機でタイムアウトを再現した。

現行ThreadingTests.cppはSHA `4f4e137fec05624017bab1c05e102774546ac714` と一致する無変更ファイルを使用した。

検証結果：

| 構成 | 描画・追加回帰 | 既存Threading | 故障注入 | CTest |
|---|---:|---:|---:|---:|
| GCC Debug | 20/20 | 23/23 | 5/5系統 | 7/7 |
| GCC Release | 20/20 | 23/23 | 5/5系統 | 7/7 |
| GCC ASan + UBSan | 20/20 | 23/23 | 5/5系統 | 7/7 |
| GCC ThreadSanitizer | 20/20 | 23/23 | 5/5系統 | 7/7 |
| Clang Release | 20/20 | 23/23 | 5/5系統 | 7/7 |

故障注入の構築系統は確保位置0〜13を走査した。確保失敗の注入はテスト実行ファイル内の通常new/new[]に限定し、OS APIのあらゆるエラーやAligned newを網羅したものではない。

Releaseの描画・Threading実行ファイルをそれぞれ100回反復し、すべて通過。直接影響する4ヘッダーをGCC / Clangそれぞれで単独コンパイルし8/8通過。部分チェックアウトのSource/Testsは40ファイル、追加検証用cppは2ファイルをSTL監査し、違反0。

## 未確認事項

- 全リポジトリのApplication / Asset / Gameplay / Physicsを含むrootビルド・全テストは、この部分チェックアウトでは実行していない。
- Windows/MSVC、実DxLib SDK、F5、実画面・音声・入力の検証は実行していない。
- root CMakeに追加する登録は既存のdxf::supportへリンクする。独立検証でも同じ登録関数を使うが、独立検証の成功をroot全体ビルド成功と呼ばない。
- Physicsの長時間数値品質、CCDの対応拡大、TaskDispatcher/AsyncAsset全体の競合監査は今回の変更範囲外。
- GitHubのmainにはこのパッチを反映していない。

## 検証方法

リポジトリ全体へパッチを適用した後は、既存のWindows Debug/Release検証とSTL監査をまず実行する。部分検証だけの場合は次を使用する。

```powershell
cmake -S Tools/RenderValidation -B Build/RenderContinuation -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Build/RenderContinuation
ctest --test-dir Build/RenderContinuation --output-on-failure
```

WindowsでNinjaを使う場合は、既存のVisual Studio環境初期化経路を使用する。コンパイラ未検出をテスト失敗や成功と混同しない。Linuxでは別のビルドフォルダーで `DXF_RENDER_ASAN=ON` または `DXF_RENDER_TSAN=ON` を指定できる。
