# 仕掛けJointの引き渡し点

作業開始main/origin：379733632d55574ea54cc3b418a3046a0c50384e（clean）。製品commit：890a6d63092122dd0bbb547bf07d568b64f0b3b6。配布・変異検証を含む検証コードHEAD：07e60fe0887af791a50addb97acdcccbaf448cd3。この文書を含む文書commitが後続する。mainのみ、利用者の変更の破棄・別branch/worktree・reset/clean/stash/amend/rebase/force pushなし。

K0〜K9の機能・検証・現在仕様の更新を完了。次のJointや別分野は自動着手しない。最終文書commit/push/fetch/HEAD-origin-lsremote照合の結果はチャットの最終報告を起点にする。中断からの実装再開は不要。

採用source manifest：`1f7b8c8e6117086ef1fb1a81b527f61e4ff51c33d41f106ff47e9bf2c3cfd403`。製品/試験/Tools/CMakeは採用回帰後に変えていない。文書のみ後続。採用集合はBuild/MechanismJoints-20261001-6f1e47/FinalAdoptedValidation.json、コマンド/開始終了/exit/構成/manifest/生ログ/登録/JUnit/LastTest/exe hashは同じrunのLogs配下。過去ログを上書きしていない。

|工程|結果|
|---|---|
|K0|現行main/所有/登録/config保護・保存|
|K1〜K5|新3種類両次元・Frame・Limit・有限Motor・設定変更・混在slot・成功Step cache確定|
|K6|6Component・型付き参照・目標速度・2D/3D仕掛けコース・既存UI。Framework410/410、Interaction46/46|
|K7|Physics588/588、全6種類fault、K-M01〜24とDistance M01〜14を検出/復元。同期/1/2/4/8lane、TOI0/1/3を確認|
|K8|通常コード5World費用測定、4構成配布。OFF各10工程、ON/device各13工程成功|
|K9|final5 Portable D/R各28/28、final4 Native D/R各34/34、正規独立各28/28、NoSTL773違反0、全IDE指定Python67/67、公開ヘッダー50compile、通常/Development IDE生成|

失敗を消さない。final3 Portable Debugは27/28 exit8（新試験の即時解放後の予約実行）。owner-contract Debugも失敗（失効Handle再参照）。既存遅延破棄契約へ合わせて修正し、後続の単独D/Rと最終全群を成功した別試行として採用。final4全体wrapperは測定コード5箇所の書式変更を前後manifest比較で検出しexit1。混在版を全体成功とせず、同じ最終コードで38変異とPortable D/Rを再実行した。Native/独立/4配布/測定/最終Pythonは同じ最終manifest。全試行は[今回の検証記録](MechanismJointsCompletion-2026-10-01.md)に保存した。

SDKは同PCの既存ThirdParty/DxLib-3.25a-source（extension3/FBXSDK OFF）。新SDK取得/再構築は行わない。SDKなしPC/開発ツールなしPC/TSan/人のF5・物理入力・聴感/Worker内部人工例外/D3D全資源リーク/既存NativeModel終了原因/キャラクターDynamic床追従/UI3Dパネル確保は未実施・従来残課題。NativeUiAppのOS captureはbuiltin/styledでnot_exercised。

画像保存先の複製objフォルダー削除は自動承認レビューがポリシーで拒否。Build内に保持し、別手段で削除しない。生ログ・SDK・Build・画像・CSV・変異backupはcommitしない。

再実行は[Testing](../Testing.md)の正規入口と[今回記録](MechanismJointsCompletion-2026-10-01.md)のcommand/構成/SDKに従い、新しいrun-idで保存する。既存のrun名や保存先を再利用しない。Archiveの適用器は使わない。現在仕様は[Joint](../Physics/Joints.md)、次の候補は[Framework Roadmap](FrameworkRoadmap.md)。
