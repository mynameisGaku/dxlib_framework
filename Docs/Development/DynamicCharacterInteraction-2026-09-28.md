# Dynamic Character Interaction — 検証記録（2026-09-28）

作業開始HEAD: `81235df`（`origin/main` は `385bd96`）
この記録のHEAD: `92b0ed6`（この記録に追記したcommitは`1e1bd29`と`92b0ed6`）

## 1. 段階

| 段階 | 内容 | 状態 |
|---|---|---|
| S0 | 現行挙動の測定（実WorldでDynamic相互作用を記録） | 完了 |
| S1 | PostPhysics配送予約の定常1確保/Step | 完了（慣らし後の定常0確保/Stepを実測） |
| S2 | キャラクター→Dynamic剛体の押し | 完了（`e4ba140`） |
| S3 | Dynamic剛体→キャラクターの押し戻し | 完了（`e4ba140`） |
| S4 | 挟まれ・複数Body・Sleep・削除 | 完了（`e4ba140`） |
| S5 | Componentと実ゲーム経路 | 完了（`e4ba140`／`24ff4c1`） |
| S6 | 測定・故障注入・変異・外部利用 | 完了（`460d98e`／`27f5158`） |
| S7 | 最終回帰・文書・commit | 完了（`9ff2705`／`81235df`） |
| S8 | IDE表示・IDE検査・最終回帰・文書・push | 本記録とcommit `1e1bd29`／`92b0ed6` |

## 2. commit

- `fa2c762` カプセル幾何（S1）／`e33577e` World統合（S2）／`2fa6043` カプセルの移動問い合わせの修正
- `b2f8e3c` キャラクターのカプセル（S3）／`e4ba140` Dynamicの双方向押し合い（S4）
- `31bf702` 3D箱どうしの接触閾値の変更を戻し、誤った原因の記録を訂正
- `fafaee9` Solverとイベントの組の候補を索引から集め、3D箱の投影半径を整理
- `460d98e` 確保故障注入と外部利用者検証（S7）／`27f5158` 測定系列（S7）
- `9ff2705` Sensor通過とDynamicを含まない組の回帰（S7）
- `9e2ff30` 分割Stepの混合配置の試験を短くしてDebugの制限時間内に収める
- `81235df` 押した箱を障害物として保つことと、面の揃った3D箱の床への留まり（S7）
- `1e1bd29` IDE表示（UISample／GameplaySample）と生成物のIDE検査
- `92b0ed6` 外部consumerへのカプセルconsumerソースの配置漏れを修正

## 3. 契約

### Character→Dynamic

- 設定は`FCharacterMoveTuning`（2D／3D共通）。`bPushDynamicBodies`（既定false）のみ有効。
- 大きさは`PushForceScale ×（希望の水平速度の、剛体へ向かう成分）× 固定秒数`を`MaxPushImpulse`で制限。
- 接触法線は障害物からキャラクターへ向かうため、剛体へ与えるImpulseはその逆向き（`−Away`）。接線方向は含めない。
- 同じBodyへの複数接触は1件に集約する。`FCharacterPushSet2D/3D`の`Count`は保持数、`TotalFound`は発見数。
- 対象はDynamicのSolidのみ。Static・Sensor・衝突フィルターで拒む対象・自分自身の登録Bodyは対象外。
- `StepCharacter`はWorldを変更しない。適用は`DCharacterMovement2D/3DComponent`が同じ固定更新の物理Step前に行う。

### Dynamic→Character

- `bReceiveDynamicPush`（既定false）で有効。固定更新の開始時に、接触余裕の2倍＋（`MaxReceivedPushSpeed × 固定秒数`）以内で
  近づく剛体の速度のUpに直交する成分から退く。
- 押し戻しにも既存の障害物規則（overlap recovery・sweep・slide・ContactLimit・QueryLimit・PrecisionLimit）を使い、
  別簡易判定は作らない。結果は`FCharacterStepResult2D/3D`の`Received`と`bPushedByBody`で返す。

### Crush／挟まれ

- 解消不能時は`ECharacterRecoveryStatus`（`Blocked`／`TooDeep`／`Ambiguous`／各Limit）と`ECharacterMoveStop`で理由を出す。
- 最後の安全な位置を保持し、NaN的产生・Collider内残留・瞬間移動・無制限のImpulseは行わない。
- ダメージや死亡処理はPhysicsへ入れない。ゲーム側が結果を受けて処理する。

## 4. Capsule／Solver BroadPhase

- カプセルは2D／3Dで共通設定（`ECharacterShape::Capsule`、`HalfHeight`、`Radius`）を持つ。足元を保って高さだけ変えられる。
- Solverとイベントの組の候補はWorldが保つ問い合わせの索引（AABB木）から集める。別の木は作らない。
  候補は`(First, Second)`の昇順へ並べ直してから詳細判定し、総当たりと同じ順・同じ結果になる。
- 索引不可の座標や入れられないColliderでは総当たりへ戻る。並列経路と連続衝突の候補は切り替えない。

## 5. 変異12件

- 12件すべてが行為で検出できる状態にある（`81235df`で完成）。
- M10（押した箱を自分用Bodyとして除外）は最初は未検出で、`81235df`で次の観点の試験を追加して検出できるようにした。
  - 押したDynamic箱を自己Bodyとして除外しない
  - Characterのカプセルが押した箱との接触余裕を保つ
  - 軽い箱뿐 아니라質量200の重い箱でも押し続けた際の障害物扱いを確かめる
  - Character自身のBody登録によるSolver補正で変異が隠れない条件を使う
- 変異コードは製品ソースに残っていない。`git status`が製品ソースで空、`Settings.Radius < 0`／
  `Scale(Away, Size)`／`m_LastStep.Pushes.Items[0].Body` のいずれも残っていないことを確認した。

## 6. DebugのPhysicsContinuationとtimeout

- timeoutは延長していない（既存180秒のまま）。
- 直前の主因は新しく追加したSolver BroadPhaseのON/OFF一致試験の3D混合場面（Debugで約89秒）。
  通常Step版を240Step（休止まで確認）、SubSteps=3版を90Step（途中操作を維持、休止確認は通常版だけ）に短くした。
- 最終回帰でDebugのPhysicsContinuationは**101.33秒**で180秒以内に収まった。

## 7. 3Dの奥行き50mの箱が床へ沈む件

- Solverの不具合として直していない。接触自体は毎Step維持されていた。
- 原因は試験条件の慣性不適切（箱に既定の`DiagonalInertia = (1,1,1)`を使い、腕50mの形状に対して回転応答が過大）。
  物理的な寸法に合わせた慣性なら静止する。
- 対応はM10用Componentの箱を立方体にすることと、「面が揃った箱でも正しい慣性なら接触を失わない」回帰を残すこと。

## 8. IDE表示の修正

- `UISample`と`GameplaySample`のGUI入口を専用ディレクトリ（`CMake/UiSampleApp`、`CMake/GameplaySampleApp`）へ移した。
  ソースファイルの属性はディレクトリ単位のため、同じスコープから本体のcppを`HEADER_FILE_ONLY`で載せられる。
- 本体のcppは表示専用で、入口targetがコンパイルするのは`WindowsMain.cpp`だけ。実装は`dxf_ui_sample`／`dxf_gameplay_sample`が担う。
- `GameplaySample`は`dxf_runtime_paths`のPOST_BUILDがtargetを同じディレクトリに要求するため、呼び出しを専用ディレクトリ側へ移した。
  生成された`.dxfpaths`の`ProjectRootRelative=../../../../../`（5階層）を確認した。
- `Tools/Tests/test_ide_filters.py`は`git ls-files`の集合と`.vcxproj`／`.vcxproj.filters`を比較する。誤判定2件を試験側で直した。
  - 空`Include`の生成物エントリを集合に入れない
  - `DXF_INTERACTION_BENCHMARK_PROBES`がOFFの生成物へ計測専用ソースを要求しない（CMakeCacheから読む。ON時は必須のまま）
- 実数的な不足だったのは`GameplaySample`の`WindowsMain.cpp`がfilter`Source Files`に入っていた点で、CMake側で直した。

## 9. 最終回帰（すべて最終製品コードで新規ログ先から）

ログ先: `Build/DynamicInteractionFinal-20260928/`

| 項目 | 結果 |
|---|---|
| root Release クリーンビルド | 終了コード0 |
| root Release CTest | 27/27 passed、終了コード0、28群 389/389件、0失敗（28.13秒） |
| root Debug クリーンビルド | 終了コード0 |
| root Debug CTest | 27/27 passed、終了コード0、28群 389/389件、0失敗（142.00秒） |
| Debug PhysicsContinuation | 101.33秒（180秒以内） |
| ValidateDebug | 8/8 PASS（Debug／Releaseのconfigure・build・built-registration・tests） |
| No-STL | 652ファイル、違反0 |
| Python（通常） | 60 tests OK、skipped=7 |
| Python（IDE実生成物あり） | 4 tests OK |
| IDE実生成物検査 | 通常版／Development版を生成し4件OK |
| 公開ヘッダー | 主要10ヘッダーの単独include OK、2D/3D併用 OK |
| 配布 Native OFF Debug | 11/11 PASS |
| 配布 Native OFF Release | 11/11 PASS |
| 配布 Native ON Debug ＋ device | 14/14 PASS |
| 配布 Native ON Release ＋ device | 14/14 PASS |
| ベンチマーク | interaction／capsule／scaling の3系列が終了コード0 |
| `git diff --check` | 終了コード0 |

### ベンチマークの主な値

- 3系列とも終了コード0（interaction 34.5秒、capsule 403.6秒、scaling 301.5秒）。
- `interaction`は7 case × colliders × 当事者数 × 2D/3Dで126行。
- S1の目標（配送予約の定常確保）: `cold_delivery_alloc`は全126行で0。サンプリングした行の
  `world_alloc_per_step`／`delivery_alloc_per_step`／`character_alloc_per_step`／`prepare_alloc_per_step`も0。
- 既存SolverのSolid系確保、UIの3Dパネル4系列に残る2割当／frame、DxLib内部は分けて記録しており、全体のゼロではない。

### 途中で見つかった不具合

- `Tools/ValidatePackage.py`の`CONSUMER_SOURCES`にカプセルconsumerの4ファイルが無く、再配置した
  外部consumerのconfigureが`Cannot find source file`で失敗していた（`460d98e`でCMakeListsは更新されていたが一覧が未追随）。
  `92b0ed6`で修正し、配布4構成が通るようになった。
- Release＋Nativeを公式VCパッケージ（`ThirdParty/DxLib-3.25a`）で指定するとconsumerのリンクが
  FBXシンボル（LNK2019）で失敗する。Release用の`DxLib_vs2015_x64_MT.lib`に必要なFBX静的ライブラリが
  そのパッケージにないためで、フレームワークの欠陥ではない。リポジトリ自身が使う
  `ThirdParty/DxLib-3.25a-source`を指定すると全件通る。

## 10. 実施していない確認

- Visual Studio GUIは開いておらず、IDE表示を目視では確認していない。生成物（`.vcxproj`／`.vcxproj.filters`）の集合検査だけを実施した。
- 人が物理入力をした操作はしていない。`--run-device`は自動の固定入力で起動したもので、手動F5の操作は含まない。
- `NativeModelDeviceSmoke`の過去の`ProcessMessage=-1`は今回0件だが、原因特定は済んでいない。
- UIの3Dパネル4系列に残る2割当／frameは今回解決していない。

## 11. 既存の未解決事項

- 3Dの奥行き50m箱の件は試験条件の不適切慣性であり、Solverの不具合ではない（§7）。
- Worker生成失敗の故障注入は注入点がなく未実施。
- 起床伝播の追加回帰とCCD方針の明文化は残存。
- 配送やセンサのScene終了時の扱いは未確認。
