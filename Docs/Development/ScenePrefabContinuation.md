# Scene／Prefab基盤の継続点

B0〜B10の実装・累積回帰・配布・測定・現在仕様の記録を終えた。今回の範囲で区切り、新機能へ自動では進まない。

開始HEAD/originは`17ec54fadcb5861eddc67aa45c6d9771841e2ed3`、開始時clean、mainのみ。検証コードcommitは`124a97873f87f6fb4e2f18b8ca27e51eae0583af`。最終文書commitは本書の収録commitで識別する。通常push後のHEAD/origin/ls-remoteと作業ツリーの照合はチャット最終報告とBuild内のFinalGit.jsonへ残す。

保存先は`Build/ScenePrefabContent-20261001-b812ec`。最終manifestは`83156fdeb701927a8f6dc2b1e50bb6feb856bd9e77aca2771879b4accda19e89`。最終全群はfinal-mover-*（Portable D/R各30/30、Native D/R各36/36）、正規独立入口はStandalone-Final-Mover（D/R各30/30）。全てexit0、skip0。Content43、確保故障5、Framework413、Physics588、InteractionSample48。Python IDE71/71、通常64成功＋IDE依存7skip、No-STL862対象・違反0、公開ヘッダー45単位。

配布4構成はPackage-*-Accepted-Logs（OFF D/R各10工程、ON D/R各13工程）、同一exe試験はData-ABC-Accepted、独立Release測定はLogs/accepted-benchmark。後続は試験だけのCRLF修正とMover直接回帰追加で、製品・CMake・Sample・Consumer・正規Assetsの生バイトは同じ。照合はFinalProductInputComparison.json、各試行のCommand/exit/UTC/登録/JUnit/SourceManifest/BinaryHashes/LastTestを保存した。

一時変異・未commit製品コードなし。B-M01〜18は復元hash一致と復元build/Green成功。B-M12/17は診断修正後に追加再確認した。全成果と途中失敗は[試行別記録](ScenePrefabContentCompletion-2026-10-02.md)へ集約。SDK・生成物・生ログ・backup・キャプチャはstageしていない。原本Assets、空Starter、Sandbox、旧C++コースを保持。Archive未適用、拒否済み画像用中間フォルダー未操作。

次に依頼がある場合はこの記録と現在mainを照合し、新しいテーマを選ぶ。B0の調査や完了した全試行を毎回最初から反復する必要はない。

SDKなしPC、開発ツールなしPC、TSan、人の物理キー・聴感・Visual Studio目視、Worker内部の人工例外、全D3D/COM資源リーク証明は未実施。過去NativeModel早期終了原因、UI 3Dパネル確保、Dynamic床追従、Joint連続拘束を今回だけで解消扱いにしない。3Dモデル差替えのA/Bと全種類の独立cross-instance実World対照は実行しておらず、実施した代表範囲を試行別記録へ明記した。
