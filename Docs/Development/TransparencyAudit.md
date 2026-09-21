# 透明3D継続実装の監査

## 起点と検証対象

GitHub main: `86a37b9cdb7e3641e41f7838a8c5694de801679c`。
前回のdebug_tools追加分はリモート未反映の候補として引き継いだ。
変更対象の元Renderer3DソースとヘッダーはGit blob SHAと照合した。
本実行環境には完全な現行checkoutを復元できていない。実Physics World/Windows/実DxLib SDKの実行はしていない。

## 実装

- Scene不透明→Scene透明→Overlayのパス計画を独立したPrivate担当へ分離。
- 透明三角形重心と線中点の符号付きビュー奥行きをf64で計算し安定ソート。
- 同種の連続範囲だけまとめ、面と線を混ぜた奥行き順を保持。
- 半透明はTestOnly。明示Overlay/Alwaysは最後、どちらも深度を書かない。
- A=0はNativeに送らない。SolidWithEdgesでも見えない元図形の辺だけを描かない。
- SetView呼出し区間とFlushを越えてソートしない。同じIdの別カメラを混同しない。
- 全計画・確保をNative操作より先に終える。描画中の失敗は既存の最初のエラー保持・復元経路。
- Debugの重ね表示を明示Overlayへ。RenderDebugへF7のRGB透明パネルを追加。

## Red/Greenと監査の区別

最初の新規7テストは実装前に作成し、5件の期待値不一致を再現（red-tests.log）。
実装後の監査でA=0のSolidWithEdgesが辺を残す失敗を追加再現（audit-red.log）、修正後Green。
故障注入の初稿は通常newだけを対象とし、Toolbox配列の整列確保に注入できていなかった。
「注入が実際に発生した」ことを必須にする検査で失敗を記録（fault-injection-red.log）し、
Toolbox内にテスト専用の通常/整列確保置換を作って改善した。
現行の注入は52箇所で発生し、2つの注入なし対照試験も成功。各失敗でNative開始/描画/提示なし、未解放確保件数が元へ戻ることを検査。
全新規ケースを最初からTDDで書いたとは扱わない。境界やNative変換、デモの一部は追加回帰検査。

## 今回の最終検証

| 構成 | CTest |
|---|---:|
| GCC Debug | 12/12 |
| GCC Release | 12/12 |
| GCC ASan+UBSan、leak detection | 12/12 |
| GCC ThreadSanitizer | 12/12 |
| Clang Release | 12/12 |

新規RenderTransparencyは17件、NativeTransparencyは3件、透明計画の故障注入は52箇所＋2対照。
既存DebugTools、RenderContinuation、RenderViews、NativeViews、Job故障注入も同じ部分構成で実行。
CTest登録数とケース数は重複合算しない。
透明Renderer/Native変換/既存Viewの3実行ファイルを各100回、計300回反復して全通過。
直接関係する公開ヘッダー4個をGCC/Clangで単独コンパイルし8/8。
Source/Examples/Testsは86ファイルでSTL違反0。追加のTools C++4ファイルも違反0。

## 移行器監査

前回のowner修復＋debug追加と今回の変更を、書込前に一つの計画へ合成する。
前回分適用済みの既知内容も認識し、二重追加しない。不明な編集を消して成功させない。
今回13件の合成/冪等性/破損/上書き拒否テストを追加し、前回13件の退避/復元等のテストも再実行する。
これらは明示的な合成fixtureであり、完全な実リポジトリへの適用/ビルドの証明ではない。

追加で修正した移行器の問題：
- GetContext().Get3D()で得た変数をroot Contextと誤認し、正常なSubmitGeneratedを拒否していた。
- 旧APIがないことを確認するrequires式まで描画呼出しとして変換し得た。非評価のrequires本体は変換しない。
- 前回DebugTools CMakeのrender-only fake優先指定が、完全なNativeContract用fakeを隠し得た。
  その指定を撤去し、実86の完全fakeのSHAを確認してgeometry補助ヘッダーだけを追記する。

## 検証していないこと

- 完全なmainへ適用した全体ビルド/CTest、Application/AsyncAsset/Gameplay/実Physicsの回帰。
- Windows/MSVC、実DxLib SDK、RenderDebug全体のビルド、画面/入力/音声。
- GPU計測、実ゲーム負荷での性能改善、全てのメモリ不足やOS失敗。
- 交差透明面の画素単位の正しい順序。重心ソートは近似でありOITではない。
- MV1/スキニング/テクスチャモデル、実ライト/影、Viewport、多窓、正式World Snapshot。

Nativeテストは実際のDxLibGeometryBackend.cpp等をコンパイルするが、DxLib.hは手書きの翻訳テストダブル。
3DAPIの追加ではなく既存深度APIの呼出し順/値を検査したもので、実SDK/画面検査の代わりにはしない。
今回は当初候補の正式物理Snapshotより先に、実装と再現検証が揃う透明パスを完成させた。
正式Snapshotは未実装のまま対応表へ残す。前回の登録型3D観察を全World自動採取と呼ばない。
GitHubへのcommit/pushは行っていない。
