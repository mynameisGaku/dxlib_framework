# 仕掛けJoint拡張の検証記録（2026-10-01）

開始main/originは379733632d55574ea54cc3b418a3046a0c50384e、作業開始時はclean。製品・試験・配布をそろえた検証コードcommitは`07e60fe0887af791a50addb97acdcccbaf448cd3`。この記録と現在仕様の文書commitが後続する。最終HEAD/origin/ls-remoteの照合は引き渡し時に行う。mainのみで作業し、巻き戻し・別branch/worktree・stash・amend・force pushは行っていない。

採用した全コードのsource manifestは`1f7b8c8e6117086ef1fb1a81b527f61e4ff51c33d41f106ff47e9bf2c3cfd403`。Source/Examples/Tests/Tools/CMakeを対象にし、Docs/Build/SDKは含めない。文書だけの変更でコード検証結果を書き換えない。生ログ、生成物、画像、変異backup、測定CSVはGitに追加しない。

保存先は`Build/MechanismJoints-20261001-6f1e47`。Logs/<試行>/Result.jsonにはcommand/cwd/開始・終了UTC/exit/秒/config/source manifestを保存。SourceManifest.txt、全群のRegistration.json/JUnit.xml/LastTest.log/ExeHashes.json、変異の生バイトbackupと復元hashも保存した。同じ保存先を上書きせず、各試行は別名。前回の成功を今回へ転記していない。

## 実装と利用

K0〜K9を実施。新3種類を両次元で共通slot・世代付きFJointIdへ統合し、DistanceとContactを残した。Physicsが唯一の実体所有者、Gameplayの6Componentが参照・要求・寿命を担当し、Sample/UIが操作と表示を担当する。新Manager、Body所有層、GUI実行ファイル、実行時のBox2D/Jolt依存は追加していない。

|段階|成果と検証|
|---|---|
|K0|main/差分/所有/登録/config/旧28・34群を確認・保存|
|K1|共通slot/Kind/Frame、異種slot再利用とnoexcept解放。Body/Collider返却容量も登録前予約|
|K2|Revolute 2DのAnchor2、3DのAnchor3+swing2、twist自由|
|K3|Fixed 2D/3D、現在の相対PoseのLocal Frame化、非Identity・半回転・異方性慣性|
|K4|Prismatic、横拘束と姿勢、動くFrameA軸の微分、自由な軸移動|
|K5|片側/等値Limit、有限速度Motor、設定変更と同値ID/基本cache保持|
|K6|6Component、型付きBody参照、目標速度helper、既存コースとUI操作|
|K7|解析/実World/Component/故障/新旧38変異/並列/CCDの累積回帰|
|K8|5新規Worldの費用測定、Physics/Framework/Nativeの再配置Consumer、4配布|
|K9|Portable/Native D/R、正規独立検証、IDE/ヘッダー/NoSTL/Python、現在仕様とRoadmap|

公開名はCreate/GetRevoluteJoint、Create/GetFixedJoint、Create/GetPrismaticJoint。両Worldで同名、DestroyJoint/IsJointAlive共通、種類違いはエラー。Make*JointDescriptionは現在の共通World Frameから両Local Frameを一度だけ作る読み取りhelper。6種類のWorld例とComponent/目標操作は[現在仕様](../Physics/Joints.md)、compile/runする実例はTools/PackageConsumer/PhysicsMechanism.cpp・MechanismConsumer.cppにある。

Sampleは手動扉、電動扉、スライド、回転Kinematic支点の昇降機、Fixed連結物、Fixed荷物+Distance吊り下げを2D/3Dに置く。F1の既存Button/Slider/Toggle/Labelで対象・目標・速度・努力上限・Limit・接続・停止/再開/逆転・一度のImpulseを指定する。Controller→Component要求→固定更新だけで操作し、描画はStepしない。3Dは斜め軸と非Identity姿勢を使う。空Starter、Sandbox、既存キャラクター/取得物/Trigger/Pause/1・2表示を保持した。

単位m/kg/s/rad。Joint Angleは(-π,π]で回転数ではなく、Limitは折返しを跨がない厳密な(-π,π)区間。q/-qは同じ回転。3D Revoluteの反平行近傍（軸内積<-0.999999）とtwist投影ノルム<1e-12は拒否する。Motor累積ImpulseはSubStep hごとに±MaxTorque*h/±MaxForce*h、Limit/Anchor反力とは別。速度0は有限ブレーキ。設定失敗は旧設定/ID保持、求解履歴は成功Stepのみ確定しBody rollbackは保証しない。TOI即時処理はContactのみ。Joint TOI、Anchor Sweep、任意障害物を貫通しない位置補正は提供しない。

## 数値条件と層の区別

基礎fixtureは重力0・質量1kg・慣性1kg m²・dt=1/60s、1〜3mの配置、Anchor/横ずれ1mm・姿勢1mrad・速度0.001を別単位で判定する。60/600Stepの有限性・自由度・収束を確認。異方性、質量比、COM外Anchor、q/-q、90/120度、半回転、Prismaticの動く軸を追加。配置・反復・許容値の具体値はTests/Physics/MechanismJointTests.cppのfixtureと個別caseに固定した。一律3%や既存画素の弱化はしない。

独立計算は単位ImpulseのK（2D22.5、3D36等）、Rodrigues回転、SO(3)/swing/twistの有限差分、動く軸の微分を用いる。回転支点で速度0 Motorを位置ロックと誤認した初期期待は訂正し、半陰的Eulerの独立Oracleへ照合した。600StepのTranslationは2D2.006680m/3D2.010434m。横誤差/姿勢の1mm/1mrad判定を緩めた結果ではない。

Mathの解析、実World、Component、実Scene/実Application、実DxLib、外部Consumerを分けた。実DxLibは固定Snapshot入力でGPU描画と一括読戻しを使う。3種類×2次元×全画面1/左右2/1001×501左右2＝30領域で現在の投影位置の画素を確認し、白い文字が代用にならない色条件と領域内>8画素を要求する。画素ごとのGPU読出しは追加していない。Debug/Releaseの画像をNativeArtifacts-<config>へ保存し、代表2D/3D画像も確認した。

Physics588/588（旧525+新63）、Framework410/410、InteractionSample46/46。これらは下記採用コードの全群stdoutから集計した内部case数で、CTest群数とは異なる。

## 全群と途中の試行（合算しない）
|試行|登録|実行|成功|失敗|skip|exit|秒|manifest先頭|
|---|---|---|---|---|---|---|---|---|
|final3-Portable-Debug-tests|28|28|27|JointComponentFault|0|8|164.313|e27d353f5f97|
|final4-Portable-Debug-tests|28|28|28|0|0|0|163.359|d5771104eb55|
|final4-Portable-Release-tests|28|28|28|0|0|0|29.063|d5771104eb55|
|final4-Native-Debug-tests|34|34|34|0|0|0|208.328|1f7b8c8e6117|
|final4-Native-Release-tests|34|34|34|0|0|0|67.015|1f7b8c8e6117|
|final5-Portable-Debug-tests|28|28|28|0|0|0|162.25|1f7b8c8e6117|
|final5-Portable-Release-tests|28|28|28|0|0|0|29.016|1f7b8c8e6117|
採用はfinal5 Portable D/R、final4 Native D/R、final4の独立/配布/測定/NoSTLと最終IDE指定Python。final4開始後の測定コード5箇所の書式整理を前後manifest照合が検出し、全体wrapperはexit1で停止した。全群が成功したこととは別で、Portable両構成と38変異を新しい指紋で再実行した。final4の混在した全体実行を成功と扱わない。採用集合はFinalAdoptedValidation.jsonで固定した。

|正規独立検証|登録/実行/成功|失敗/skip|exit|
|---|---|---|---|
|Debug|28/同数/同数|0/0|0|
|Release|28/同数/同数|0/0|0|
最初のfinal3 JointComponentFaultは新試験の即時解放後の非所有予約実行によるUAF。予約キューの契約を確認し、Owner.Destroy→抑止確認→予約消費→境界Shutdownへ訂正。owner-contract Debugも失敗（3221225477）：破棄要求で失効したHandleを試験が再参照。要求前の有効実体を固定更新の間だけ保持して検査し、owner-contract2 Debug/Releaseはbuild0/test0。即時打切りは予約Clear→Shutdownで確認。製品のOS終了原因と混ぜない。

過去のNativeModel予期しない終了原因は未特定のまま。今回のNative全群D/R各1試行では発生しなかった。無限再試行・自動再初期化・timeout延長は行わない。

## 変異と故障

|変異|build|Red|復元build|Green|
|---|---|---|---|---|
|K-M01|0|1|0|0|
|K-M02|0|1|0|0|
|K-M03|0|1|0|0|
|K-M04|0|1|0|0|
|K-M05|0|1|0|0|
|K-M06|0|1|0|0|
|K-M07|0|1|0|0|
|K-M08|0|1|0|0|
|K-M09|0|1|0|0|
|K-M10|0|1|0|0|
|K-M11|0|1|0|0|
|K-M12|0|1|0|0|
|K-M13|0|1|0|0|
|K-M14|0|1|0|0|
|K-M15|0|1|0|0|
|K-M16|0|1|0|0|
|K-M17|0|1|0|0|
|K-M18|0|1|0|0|
|K-M19|0|1|0|0|
|K-M20|0|1|0|0|
|K-M21|0|1|0|0|
|K-M22|0|1|0|0|
|K-M23|0|1|0|0|
|K-M24|0|1|0|0|
|M01|0|1|0|0|
|M02|0|1|0|0|
|M03|0|1|0|0|
|M04|0|1|0|0|
|M05|0|1|0|0|
|M06|0|1|0|0|
|M07|0|1|0|0|
|M08|0|1|0|0|
|M09|0|1|0|0|
|M10|0|1|0|0|
|M11|0|1|0|0|
|M12|0|1|0|0|
|M13|0|1|0|0|
|M14|0|1|0|0|
生バイト・BOM・hashの完全復元と更新時刻更新、実再コンパイルを確認。最初のK-M01 canonical1はf64修飾不足のcompile失敗で検出に数えず、実R diag(V) Rᵀへの非等価変異をcanonical2で検出した。変異検証器もbuild失敗時にRedを実行せず、復元/Greenしても失敗を維持する2試験を持つ。

全新種類・両次元の登録/Component登録・初回Pre/Post/再接続/Step/解放を実Allocation Faultで確認。Component登録countdown0〜6注入、7が最初の未注入正常試行、初回接続0〜4注入、5未注入。Stepは各種類・次元で42注入（うち求解後25）、42が未注入正常試行。再接続は容量32を満たして新slot確保失敗も含め、旧接続を保持し回復後index32、ghostなし。各具体countdown/到達/recoveryはstdoutへ残した。Body/Collider33件解放で旧コードの各10確保をRed検出し、登録前返却容量予約で0確保。Componentの100読み取り/同値維持/予約終了も0追加確保。公開Body姿勢/速度を対照へ戻して次の正常Stepを比較し、Body rollbackとcache確定を区別した。

## 配布4構成

|Native|構成|状態|工程|wrapper秒|全工程名（各exit0）|
|---|---|---|---|---|---|
|off|Debug|passed|10|19.11|build-configure,build,install,consumer-configure,consumer-build,consumer-run,support-only-run,physics-only-run,ui-only-run,ui-runtime-run|
|off|Release|passed|10|21.438|build-configure,build,install,consumer-configure,consumer-build,consumer-run,support-only-run,physics-only-run,ui-only-run,ui-runtime-run|
|on|Debug|passed|13|36.5|build-configure,build,install,consumer-configure,consumer-build,consumer-run,support-only-run,physics-only-run,ui-only-run,ui-runtime-run,native-app-run,native-ui-app-builtin-run,native-ui-app-styled-run|
|on|Release|passed|13|39.422|build-configure,build,install,consumer-configure,consumer-build,consumer-run,support-only-run,physics-only-run,ui-only-run,ui-runtime-run,native-app-run,native-ui-app-builtin-run,native-ui-app-styled-run|
前回10/13工程を削除せず、新3種類は既存Consumerへ追加。export-paths.logでも元Source/Build絶対パスの漏れを検査。PhysicsOnlyは上位層なしで6種類、Step/Get/Drive/Limit/Body破棄。Frameworkは型付き参照/6Component/待機/同値/解除/再接続/Scene終了。NativeAppは両次元3種類の実画素、NativeUiAppはbuiltin/styled双方の旧場面と新Motor設定-.5、12固定Step後の実角速度<-.1を確認。UIのOSマウスcaptureは各Native構成の2checksでnot_exercised、物理操作成功へ読み替えない。CONSUMER_SOURCES/CMake両方へ登録し、元Source cppはConsumerへ追加コンパイルしていない。

## 費用測定（通常製品コード）

|次元|Kind(1回転/2固定/3直動)|条件|本数|lane(1=nullptr)|CPU区間|median ms/Step|min|max|確保/Step|Body|Joint|Contact manifold|島|Worker島|Sleep|活動|設定Joint行数（Contact除外）|
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|2D|1|independent-static|256|1|world-step|2.870052|2.867122|2.891603|523.000|512|256|0|256|0|0|256|512|
|3D|1|independent-static|256|1|world-step|4.111400|4.088753|4.427070|523.000|512|256|0|256|0|0|256|1280|
|2D|2|independent-static|256|1|world-step|2.143607|2.135126|2.221743|523.000|512|256|0|256|0|0|256|768|
|3D|2|independent-static|256|1|world-step|3.791763|3.773108|4.019192|523.000|512|256|0|256|0|0|256|1536|
|2D|3|independent-static|256|1|world-step|1.994637|1.991487|2.334179|523.000|512|256|0|256|0|0|256|512|
|3D|3|independent-static|256|1|world-step|3.836361|3.831061|3.850858|523.000|512|256|0|256|0|0|256|1280|
|2D|1|independent-static|256|4|world-step|2.692557|2.678010|2.953856|625.000|512|256|0|256|256|0|256|512|
|3D|1|independent-static|256|4|world-step|3.755221|3.743547|4.028626|625.000|512|256|0|256|256|0|256|1280|
|2D|2|independent-static|256|4|world-step|1.972396|1.965482|2.241931|625.000|512|256|0|256|256|0|256|768|
|3D|2|independent-static|256|4|world-step|3.449272|3.447272|3.481413|625.000|512|256|0|256|256|0|256|1536|
|2D|3|independent-static|256|4|world-step|1.856374|1.852373|1.861789|625.000|512|256|0|256|256|0|256|512|
|3D|3|independent-static|256|4|world-step|3.532585|3.491853|3.777973|625.000|512|256|0|256|256|0|256|1280|
|2D|3|contact-joint-mix|256|1|world-step|1.873407|1.866881|2.212552|1298.000|513|256|256|256|0|0|512|512|
|3D|3|contact-joint-mix|256|1|world-step|3.489082|3.391493|3.572435|1298.000|513|256|256|256|0|0|512|1088|
|2D|3|contact-joint-mix|256|4|world-step|1.739475|1.732832|1.780839|1985.000|513|256|256|256|256|0|512|512|
|3D|3|contact-joint-mix|256|4|world-step|3.035915|3.003753|3.177963|1985.000|513|256|256|256|256|0|512|1088|
|2D|1|motor-active|256|1|world-step|3.145027|3.120027|3.549313|523.000|512|256|0|256|0|0|256|768|
|3D|1|motor-active|256|1|world-step|4.271346|4.254051|4.581158|523.000|512|256|0|256|0|0|256|1536|
|2D|3|motor-active|256|1|world-step|2.085975|2.056544|2.087706|523.000|512|256|0|256|0|0|256|768|
|3D|3|motor-active|256|1|world-step|3.869702|3.861360|4.116872|523.000|512|256|0|256|0|0|256|1536|
|2D|1|motor-active|256|4|world-step|2.919801|2.907141|3.187577|625.000|512|256|0|256|256|0|256|768|
|3D|1|motor-active|256|4|world-step|3.918537|3.876552|3.992283|625.000|512|256|0|256|256|0|256|1536|
|2D|3|motor-active|256|4|world-step|1.899943|1.895096|1.900932|625.000|512|256|0|256|256|0|256|768|
|3D|3|motor-active|256|4|world-step|3.523212|3.513246|3.771985|625.000|512|256|0|256|256|0|256|1536|
|2D|1|sleeping|16|1|world-step|0.147430|0.141050|0.150984|39.000|32|16|0|16|0|16|0|32|
|3D|1|sleeping|16|1|world-step|0.206383|0.179053|0.222195|39.000|32|16|0|16|0|16|0|80|
|2D|2|sleeping|16|1|world-step|0.096508|0.095847|0.100530|39.000|32|16|0|16|0|16|0|48|
|3D|2|sleeping|16|1|world-step|0.183653|0.181652|0.185854|39.000|32|16|0|16|0|16|0|96|
|2D|3|sleeping|16|1|world-step|0.110427|0.109522|0.111434|39.000|32|16|0|16|0|16|0|32|
|3D|3|sleeping|16|1|world-step|0.208337|0.208008|0.212826|39.000|32|16|0|16|0|16|0|80|
|2D|1|sleeping|16|4|world-step|0.138269|0.137696|0.139833|81.000|32|16|0|16|16|16|0|32|
|3D|1|sleeping|16|4|world-step|0.199278|0.198036|0.203820|81.000|32|16|0|16|16|16|0|80|
|2D|2|sleeping|16|4|world-step|0.125367|0.122689|0.126460|81.000|32|16|0|16|16|16|0|48|
|3D|2|sleeping|16|4|world-step|0.203196|0.201854|0.203407|81.000|32|16|0|16|16|16|0|96|
|2D|3|sleeping|16|4|world-step|0.139215|0.136649|0.142783|81.000|32|16|0|16|16|16|0|32|
|3D|3|sleeping|16|4|world-step|0.228686|0.225451|0.229627|81.000|32|16|0|16|16|16|0|80|
|2D|3|component-steady|256|1|component-resolution-enqueue|0.022542|0.021830|0.023242|0.000|512|256|0|256|0|0|256|512|
|2D|3|component-steady|256|1|component-pre|0.011413|0.011155|0.011834|0.000|512|256|0|256|0|0|256|512|
|2D|3|component-steady|256|1|world-step|1.998585|1.980775|2.008003|523.000|512|256|0|256|0|0|256|512|
|2D|3|component-steady|256|1|component-post|0.035547|0.035404|0.037222|0.000|512|256|0|256|0|0|256|512|
|3D|3|component-steady|256|1|component-resolution-enqueue|0.025171|0.024850|0.028532|0.000|512|256|0|256|0|0|256|1280|
|3D|3|component-steady|256|1|component-pre|0.017275|0.017077|0.019352|0.000|512|256|0|256|0|0|256|1280|
|3D|3|component-steady|256|1|world-step|3.831391|3.821481|4.080303|523.000|512|256|0|256|0|0|256|1280|
|3D|3|component-steady|256|1|component-post|0.072180|0.071707|0.077223|0.000|512|256|0|256|0|0|256|1280|
|2D|3|component-steady|256|4|component-resolution-enqueue|0.023370|0.022841|0.028028|0.000|512|256|0|256|256|0|256|512|
|2D|3|component-steady|256|4|component-pre|0.011533|0.011296|0.013964|0.000|512|256|0|256|256|0|256|512|
|2D|3|component-steady|256|4|world-step|1.862073|1.857700|2.097062|625.000|512|256|0|256|256|0|256|512|
|2D|3|component-steady|256|4|component-post|0.037163|0.036737|0.042597|0.000|512|256|0|256|256|0|256|512|
|3D|3|component-steady|256|4|component-resolution-enqueue|0.025253|0.024633|0.028674|0.000|512|256|0|256|256|0|256|1280|
|3D|3|component-steady|256|4|component-pre|0.017496|0.017033|0.020172|0.000|512|256|0|256|256|0|256|1280|
|3D|3|component-steady|256|4|world-step|3.502561|3.498478|3.739608|625.000|512|256|0|256|256|0|256|1280|
|3D|3|component-steady|256|4|component-post|0.071954|0.071664|0.077697|0.000|512|256|0|256|256|0|256|1280|
|2D|3|component-drive-change|256|1|component-resolution-enqueue|0.017704|0.017563|0.021746|0.000|512|256|0|256|0|0|256|768|
|2D|3|component-drive-change|256|1|component-pre|0.012189|0.012056|0.014840|0.000|512|256|0|256|0|0|256|768|
|2D|3|component-drive-change|256|1|world-step|2.252556|2.250253|2.536780|523.000|512|256|0|256|0|0|256|768|
|2D|3|component-drive-change|256|1|component-post|0.038844|0.038025|0.044030|0.000|512|256|0|256|0|0|256|768|
|3D|3|component-drive-change|256|1|component-resolution-enqueue|0.020298|0.019923|0.022563|0.000|512|256|0|256|0|0|256|1536|
|3D|3|component-drive-change|256|1|component-pre|0.018277|0.017659|0.020034|0.000|512|256|0|256|0|0|256|1536|
|3D|3|component-drive-change|256|1|world-step|4.092376|4.059923|4.308205|523.000|512|256|0|256|0|0|256|1536|
|3D|3|component-drive-change|256|1|component-post|0.074887|0.074607|0.079418|0.000|512|256|0|256|0|0|256|1536|
|2D|3|component-drive-change|256|4|component-resolution-enqueue|0.017960|0.017862|0.022728|0.000|512|256|0|256|256|0|256|768|
|2D|3|component-drive-change|256|4|component-pre|0.012335|0.012314|0.015765|0.000|512|256|0|256|256|0|256|768|
|2D|3|component-drive-change|256|4|world-step|2.081349|2.074646|2.370027|625.000|512|256|0|256|256|0|256|768|
|2D|3|component-drive-change|256|4|component-post|0.039277|0.038459|0.045962|0.000|512|256|0|256|256|0|256|768|
|3D|3|component-drive-change|256|4|component-resolution-enqueue|0.019996|0.019887|0.020408|0.000|512|256|0|256|256|0|256|1536|
|3D|3|component-drive-change|256|4|component-pre|0.017972|0.017693|0.018158|0.000|512|256|0|256|256|0|256|1536|
|3D|3|component-drive-change|256|4|world-step|3.700039|3.693112|3.706683|625.000|512|256|0|256|256|0|256|1536|
|3D|3|component-drive-change|256|4|component-post|0.073865|0.073423|0.074777|0.000|512|256|0|256|256|0|256|1536|
|2D|3|component-toggle|256|1|component-resolution-enqueue|0.016310|0.015892|0.016607|0.000|512|256|0|256|0|0|256|512|
|2D|3|component-toggle|256|1|component-pre|0.020110|0.019832|0.020362|0.000|512|256|0|256|0|0|256|512|
|2D|3|component-toggle|256|1|world-step|0.995518|0.994517|1.001185|262.000|512|256|0|256|0|0|256|512|
|2D|3|component-toggle|256|1|component-post|0.018707|0.018199|0.018964|0.000|512|256|0|256|0|0|256|512|
|3D|3|component-toggle|256|1|component-resolution-enqueue|0.018348|0.017088|0.021525|0.000|512|256|0|256|0|0|256|1280|
|3D|3|component-toggle|256|1|component-pre|0.032788|0.031711|0.036389|0.000|512|256|0|256|0|0|256|1280|
|3D|3|component-toggle|256|1|world-step|1.994427|1.910570|2.115351|262.000|512|256|0|256|0|0|256|1280|
|3D|3|component-toggle|256|1|component-post|0.037885|0.036108|0.040461|0.000|512|256|0|256|0|0|256|1280|
|2D|3|component-toggle|256|4|component-resolution-enqueue|0.017283|0.016796|0.017656|0.000|512|256|0|256|256|0|256|512|
|2D|3|component-toggle|256|4|component-pre|0.020995|0.020633|0.021262|0.000|512|256|0|256|256|0|256|512|
|2D|3|component-toggle|256|4|world-step|0.941349|0.935238|0.951519|347.000|512|256|0|256|256|0|256|512|
|2D|3|component-toggle|256|4|component-post|0.019602|0.019452|0.019833|0.000|512|256|0|256|256|0|256|512|
|3D|3|component-toggle|256|4|component-resolution-enqueue|0.018171|0.017742|0.021366|0.000|512|256|0|256|256|0|256|1280|
|3D|3|component-toggle|256|4|component-pre|0.032174|0.031848|0.036544|0.000|512|256|0|256|256|0|256|1280|
|3D|3|component-toggle|256|4|world-step|1.770208|1.762465|1.893312|347.000|512|256|0|256|256|0|256|1280|
|3D|3|component-toggle|256|4|component-post|0.037348|0.036821|0.040385|0.000|512|256|0|256|256|0|256|1280|
|2D|3|long-chain|32|8|world-step|0.282511|0.279563|0.284376|60.000|33|32|0|1|1|0|32|64|
|3D|3|long-chain|32|8|world-step|0.523891|0.520788|0.537104|60.000|33|32|0|1|1|0|32|160|
各条件で新World5回。初回Step、慣らし（初回除外）、定常120StepのWorldごとの平均を別行に出し、その5値のmedian/min/maxを集計。通常慣らし30、Sleep90、pilotは定常12。16/64/256接続、8/32/128鎖、全3種類/駆動/Limit停止/Contact4種類混在/共有Static・Kinematic/Component維持・Drive変更・接続切替、同期・4laneと代表8laneを測った。要求値の処理はcomponent-resolution-enqueueに含む。活動維持のImpulseはStep区間外。World構築・Job生成は計測区間外。Component値はComponent自身の境界で、所有階層の配送は含まない。設定行数はJointの構成から算出し、活動行の実測やContact行を含む総数ではない。mixed系列は4種類を同数ずつ配置し、Kind列3は系列の代表指定である。Contact値は診断のManifoldCount。GPU時間・実機全資源確保ではない。最大島は配置からの算出で一般Worldの実測とは呼ばない。鎖1本は1島で鎖内部を並列化したとはしない。Worldの確保を隠さず、Component維持の定常追加確保0を別に示す。

## 補助検査と未実施

No-STL773ファイル違反0。Python通常67中成功60/skip7、VS solution指定版67中成功64/skip3、全IDE生成物指定版は67/67・skip0を確認。通常/Development正規生成、filter集合、cpp二重コンパイル、F5設定を検査。新/変更Public47ヘッダー単独＋正順/逆順/2D・3D併用3＝50compile成功（MSVC /std:c++20 /utf-8 /W4）。観察型の直接Vector include不足は単独compileでRedを残して6ヘッダーへ追加し、6compile-only TUを正規dxf_testsへ登録した。コード124テキストのUTF8/CRLF/既存BOMを照合し、変更箇所の通常括弧内改行は0、git diff --check成功。既存C4324/C4244等を全体で抑制していない。

OS Windows11 Pro 26200、CPU Ryzen7 9800X3D 8core/16logical、GPU RTX4070 SUPERほかを機器一覧で採取（全機器が今回描画に選択されたという意味ではない）。VS18 2026 x64、既存Source SDK ThirdParty/DxLib-3.25a-source（extension3/FBX SDK OFF）。新SDK取得/再構築/新ライセンス承諾なし。SDKなしPC、開発ツールなしPC、TSan、人のF5/物理キー/マウス/聴感、Worker内部人工例外、D3D全資源リーク、NativeModel既知終了原因の特定、Dynamic床キャラクター追従、UI3Dパネル確保は未実施/従来残課題。

参考は一次資料の[Box2D Joint用語](https://box2d.org/documentation/md_simulation.html)、[累積Impulse](https://box2d.org/posts/2024/02/solver2d/)、Joltの[Hinge](https://jrouwe.github.io/JoltPhysics/class_hinge_constraint_settings.html)・[Fixed](https://jrouwe.github.io/JoltPhysics/class_fixed_constraint_settings.html)・[Slider](https://jrouwe.github.io/JoltPhysics/class_slider_constraint_settings.html)。自由度とFrame/累積予算の概念照合だけに用い、API/原点/既定値/コード/ライセンスを移植しない。次の主題はFrameworkRoadmap.mdに候補として残し、自動着手しない。Spring/Gear/影/GPUモーフ等へ広げていない。

画像保存先内に複製されたobj中間フォルダーの削除は自動承認レビューがポリシーで拒否したため実施せず、別手段でも試していない。Build内に残し、Gitには含めない。

## 初期試行の失敗分類と採用外の結果

|分類|最初の問題・判断|修正または採用範囲|
|---|---|---|
|既存製品の確保|Body/Colliderのnoexcept解放で返却配列が拡張された|33件解放の両次元Red各10確保→登録前容量予約→Green0。機能を削除して回避しない|
|数値試験の期待|回転Prismatic支点で、共通Anchor helperの初期座標0を距離2と誤認。速度0を位置ロックと誤認|配置を明示し、動く軸の微分と半陰的Eulerの独立Oracleへ照合。横誤差/姿勢の許容は保持|
|Sampleの処理順|初回40/42：制御ObjectがJointの観察無効化後に読み要求を失った|制御Objectを装置より先に生成。公開API追加・ゲーム側の手動寿命管理で隠さない|
|Sample試験の入力/期待|42/44：再入場に必要なフレーム不足。44/46：選択数とSlider幅・方向操作|必要フレームと入力列を訂正、既存UI方向移動を使える小さい幅へ修正。46/46は後の別試行|
|Native試験の入力|odd UI初回：連続Downの間に解放Stepがなかった|固定Snapshotに押下/解放を明示。描画閾値を緩めず30領域を確認|
|配布ハーネス|OFF Debug2：final OnTick継承と値型Command/TResult混同。同時にDistance M01変異へ誤って重なった|その試行は正常版配布の証拠へ採用しない。Consumerを正規OnVariableTickへ直してから単独実行|
|配布生成|OFF Debug3：Consumer結果ヘッダー生成ミス|ヘッダーを修正、OFF Debug4から別試行。最終4構成は同じ最終manifest|
|有限駆動の試験期待|ON Debug physical2：UI逆転後5Stepでは3Dが未到達|Torque10Nm/慣性1kg m²で有限減速する条件を明記し12固定Stepへ設定。physical3と最終D/Rで設定-.5と実角速度<-.1を両次元確認。成功するまで反復しない|
|変異の生成|K-M01 canonical1はf64修飾不足でcompile失敗。変異検証器初回は生成時にmutate定義を落としてNameError|compile失敗を検出に数えない。canonical2の実テンソル誤用を挙動で検出し、検証器の失敗保持/復元2試験も確認|
|最終試験の寿命|final3 Debugとowner-contract Debugの異常終了|即時解放後の予約と失効Handle再参照を試験側で訂正。後続成功を初回結果へ合算しない|
|集計補助|一時集計スクリプトでWindows既定文字コードのJSON読取りエラー|UTF-8を明示して再読。製品や試験の成功へ読み替えない|
|コード指紋の不一致|final4 wrapperは開始後の測定書式変更を検出してexit1|混在実行は失敗として残し、38変異とPortable両構成を再実行。採用するNative/独立/配布/測定と指紋を揃えた|

今回の最終Native D/R全群には自然発生の初期化・ProcessMessage失敗はなかった。未実施の環境を、原因が環境だったという根拠にはしていない。変異や故障注入の期待するRedと、通常版の失敗は別に集計する。

## 保存した全コマンド試行

次表は実行順。途中非0を消さず、command/cwd/開始終了/詳細manifestは各Result.jsonへ対応する。最終版以外の成功を採用集合へ足さない。

|試行|構成|exit|秒|manifest先頭|
|---|---|---|---|---|
|K0-remote|-|0|0.031|e9fadf92212d|
|K0-branch|-|0|0.032|e9fadf92212d|
|K0-head|-|0|0.031|e9fadf92212d|
|K0-status|-|0|0.031|e9fadf92212d|
|K0-diff|-|0|0.031|e9fadf92212d|
|K0-stage|-|0|0.031|e9fadf92212d|
|K0-fetch|-|0|0.531|e9fadf92212d|
|K0-origin|-|0|0.016|e9fadf92212d|
|K0-history|-|0|0.032|e9fadf92212d|
|K0-configure-portable|-|0|5.672|e9fadf92212d|
|K0-registration-portable|-|0|0.047|e9fadf92212d|
|K1-first-build|Debug|0|10.656|3fbb32e7bef7|
|K1-distance-legacy|Debug|0|0.625|3fbb32e7bef7|
|K2-tests-build-1|Debug|0|7.031|6ed30f09b7a8|
|K2-initial-mechanisms|Debug|0|0.765|6ed30f09b7a8|
|K7-red-build-1|Debug|0|2.453|df4362af4968|
|K7-red-frame-1|Debug|0|0.766|df4362af4968|
|K7-red-release-1|Debug|1|0.547|df4362af4968|
|K7-green-build-1|Debug|0|2.422|5765691e4ec3|
|K7-green-release-1|Debug|0|0.562|5765691e4ec3|
|K7-green-frame-1|Debug|0|0.766|5765691e4ec3|
|K6-components-build-1|Debug|1|0.906|6cd3d62ee163|
|K6-components-build-2|Debug|1|7.203|6cd3d62ee163|
|K6-components-build-3|Debug|1|12.438|d3beb37826d5|
|K6-components-build-4|Debug|0|3.579|278ff7b53ff1|
|K6-components-framework-1|Debug|0|12.766|278ff7b53ff1|
|K6-expanded-build-1|Debug|0|4.047|0f1058b34474|
|K7-math-world-1|Debug|0|3.906|0f1058b34474|
|K6-target-framework-1|Debug|0|13.109|0f1058b34474|
|K7-new-fault-build-1|Debug|0|2.438|cef3c0798469|
|K7-new-fault-1|Debug|0|1.0|cef3c0798469|
|K7-formatted-build-1|Debug|0|15.984|062bba8d56ff|
|K7-formatted-world-1|Debug|0|3.937|062bba8d56ff|
|K7-formatted-fault-1|Debug|0|1.015|062bba8d56ff|
|K7-ccd-build-1|Debug|1|2.0|ba869c9f7ec3|
|K7-ccd-build-2|Debug|0|2.906|b76c09ef2d70|
|K7-ccd-world-1|Debug|0|4.063|b76c09ef2d70|
|K7-frame-world-build-1|Debug|0|0.937|92a034b62cd6|
|K7-frame-world-1|Debug|1|4.109|92a034b62cd6|
|K7-frame-world-build-2|Debug|0|1.094|8eccecb1225e|
|K7-frame-world-2|Debug|1|4.328|8eccecb1225e|
|K7-rail-diagnostic-build-1|Debug|0|0.969|750bc20fa505|
|K7-rail-diagnostic-1|Debug|1|4.344|750bc20fa505|
|K7-rail-diagnostic-build-2|Debug|0|0.937|8f0012efef5b|
|K7-rail-diagnostic-2|Debug|1|4.313|8f0012efef5b|
|K7-rail-oracle-build-1|Debug|0|0.922|72cd755235a3|
|K7-rail-oracle-1|Debug|0|4.312|72cd755235a3|
|K7-interim-build-Debug|Debug|0|14.75|72cd755235a3|
|K7-interim-registration-Debug|Debug|0|0.203|72cd755235a3|
|K7-interim-root-Debug|Debug|0|150.953|72cd755235a3|
|K6-sample-build-1|Debug|1|5.047|bbccf5b6a4d3|
|K6-sample-build-2|Debug|1|4.031|cb534e6f7c40|
|K6-sample-build-3|Debug|1|1.828|acbdf8b8c462|
|K6-sample-build-4|Debug|1|2.969|b8f2a548b037|
|K6-sample-build-5|Debug|0|1.375|f49ed5b91b5d|
|K6-sample-existing-1|Debug|0|16.36|f49ed5b91b5d|
|K6-sample-build-6|Debug|0|6.375|5b2bc77ec624|
|K6-sample-tests-build-1|Debug|0|1.531|0fcbb220c78d|
|K6-sample-tests-1|Debug|1|18.656|0fcbb220c78d|
|K6-sample-tests-build-2|Debug|0|1.609|380560623efd|
|K6-sample-tests-2|Debug|0|20.859|380560623efd|
|K6-sample-load-build-1|Debug|0|2.094|586e5212bd1e|
|K6-sample-load-1|Debug|1|22.516|586e5212bd1e|
|K6-sample-ui-build-1|Debug|0|1.484|a2952d0d1d2d|
|K6-sample-ui-1|Debug|1|22.312|a2952d0d1d2d|
|K6-sample-ui-build-2|Debug|0|1.657|e99908942819|
|K6-sample-ui-2|Debug|1|22.563|e99908942819|
|K6-sample-ui-build-3|Debug|0|1.219|ec0d4f0393d5|
|K6-sample-ui-3|Debug|0|22.64|ec0d4f0393d5|
|K7-helper-red-build-1|Debug|1|1.562|049b92fdb9b2|
|K7-helper-red-build-2|Debug|0|0.922|0859064e72cd|
|K7-helper-red-1|Debug|1|4.312|0859064e72cd|
|K7-helper-green-build-1|Debug|0|7.719|c51d80bbf66c|
|K7-helper-green-1|Debug|0|4.281|c51d80bbf66c|
|K6-native-configure-1|Debug|0|5.172|c51d80bbf66c|
|K6-native-build-1|Debug|0|29.687|c51d80bbf66c|
|K7-interim-build-Release|Release|0|41.938|c51d80bbf66c|
|K7-interim-registration-Release|Release|0|0.64|c51d80bbf66c|
|K7-interim-root-Release|Release|0|29.516|c51d80bbf66c|
|K6-native-device-1|Debug|1|1.047|c51d80bbf66c|
|K6-native-build-2|Debug|0|1.641|2414b67bc472|
|K6-native-device-2|Debug|0|1.734|2414b67bc472|
|K7-no-stl-1|-|0|0.453|2414b67bc472|
|K7-component-fault-build-1|Release|1|2.406|832edff81168|
|K7-component-fault-build-2|Release|0|1.891|76deeb5bfa8c|
|K7-component-fault-2|Release|0|0.047|76deeb5bfa8c|
|K8-benchmark-build-1|Release|1|1.907|1b7b779abb5d|
|K8-benchmark-build-2|Release|1|0.906|1b7b779abb5d|
|K8-benchmark-build-3|Release|0|5.375|c09376c078dd|
|K6-final-ui-status-tests-1|Release|0|2.469|c09376c078dd|
|K8-benchmark-pilot-1|Release|0|82.484|c09376c078dd|
|K8-benchmark-main-1|Release|0|262.672|fe200eb22e1f|
|K8-package-off-Debug-2|Debug|1|14.984|637b4902b75d|
|K7-reference-build-1|Release|1|0.953|e9b18b682b1d|
|K8-package-off-Debug-3|Debug|1|15.141|e9b18b682b1d|
|K7-reference-build-2|Release|0|2.844|e9e3b0ecc64b|
|K7-reference-tests-2|Release|0|2.375|e9e3b0ecc64b|
|K6-frame-overlay-tests-2|Release|0|2.469|e9e3b0ecc64b|
|K8-package-off-Debug-4|Debug|0|15.984|f21c51bdcc21|
|K6-native-ui-odd-build-1|Debug|0|8.891|f21c51bdcc21|
|K6-native-ui-odd-1|Debug|1|1.078|f21c51bdcc21|
|K6-native-ui-odd-build-2|Debug|0|1.672|e1d4cdffe337|
|K6-native-ui-odd-2|Debug|0|2.766|e1d4cdffe337|
|K8-package-on-Debug-native1|Debug|0|34.922|e1d4cdffe337|
|K7-additional-build1|Release|0|2.562|0f400cab01a2|
|K7-additional-tests1|Release|0|0.438|0f400cab01a2|
|K8-native-ui-physical-package2|Debug|1|29.187|0438d57776e2|
|K8-package-on-Debug-physical2|Debug|1|27.39|0438d57776e2|
|K8-package-on-Debug-physical3|Debug|0|34.5|467f66d271aa|
|K9-final-mechanism-mutations|Release|0|159.484|26935f1b187d|
|K9-final-distance-mutations|Release|0|80.891|26935f1b187d|
|K9-reserved-lifetime-build|Release|0|20.844|65a3a3ba5b1a|
|K9-reserved-lifetime-tests|Release|0|0.063|65a3a3ba5b1a|
|K9-final2-mechanism-mutations|Release|0|149.984|65a3a3ba5b1a|
|K9-final2-distance-mutations|Release|0|78.781|65a3a3ba5b1a|
|K9-multiturn-build|Release|0|1.125|76c090e1ba3b|
|K9-multiturn-tests|Release|0|0.438|76c090e1ba3b|
|K9-explicit-code-stage|-|0|0.265|76c090e1ba3b|
|K9-staged-check|-|0|0.062|76c090e1ba3b|
|K9-IDE-normal|-|0|4.906|76c090e1ba3b|
|K9-IDE-development|-|0|6.266|76c090e1ba3b|
|K9-IDE-filters|-|0|0.781|76c090e1ba3b|
|K9-header-Order2D3D|-|0|0.031|76c090e1ba3b|
|K9-header-ReverseOrder|-|0|0.031|76c090e1ba3b|
|K9-header-TypedBodiesMovers|-|0|0.219|76c090e1ba3b|
|K9-header2-AngularJointDrive|-|0|0.079|76c090e1ba3b|
|K9-header2-AngularJointLimits|-|0|0.078|76c090e1ba3b|
|K9-header2-AngularJointTargetCommand|-|0|0.078|76c090e1ba3b|
|K9-header2-AngularJointTargetSettings|-|0|0.063|76c090e1ba3b|
|K9-header2-FixedJointComponent2D|-|0|0.203|76c090e1ba3b|
|K9-header2-FixedJointComponent3D|-|0|0.203|76c090e1ba3b|
|K9-header2-FixedJointComponentDescription2D|-|0|0.203|76c090e1ba3b|
|K9-header2-FixedJointComponentDescription3D|-|0|0.203|76c090e1ba3b|
|K9-header2-FixedJointDescription2D|-|0|0.079|76c090e1ba3b|
|K9-header2-FixedJointDescription3D|-|0|0.078|76c090e1ba3b|
|K9-header2-FixedJointObservation2D|-|2|0.062|76c090e1ba3b|
|K9-header3-AngularJointDrive|-|0|0.078|ddc2d77d972d|
|K9-header3-AngularJointLimits|-|0|0.078|ddc2d77d972d|
|K9-header3-AngularJointTargetCommand|-|0|0.078|ddc2d77d972d|
|K9-header3-AngularJointTargetSettings|-|0|0.078|ddc2d77d972d|
|K9-header3-FixedJointComponent2D|-|0|0.203|ddc2d77d972d|
|K9-header3-FixedJointComponent3D|-|0|0.203|ddc2d77d972d|
|K9-header3-FixedJointComponentDescription2D|-|0|0.203|ddc2d77d972d|
|K9-header3-FixedJointComponentDescription3D|-|0|0.203|ddc2d77d972d|
|K9-header3-FixedJointDescription2D|-|0|0.063|ddc2d77d972d|
|K9-header3-FixedJointDescription3D|-|0|0.078|ddc2d77d972d|
|K9-header3-FixedJointObservation2D|-|0|0.078|ddc2d77d972d|
|K9-header3-FixedJointObservation3D|-|0|0.079|ddc2d77d972d|
|K9-header3-FixedJointState2D|-|0|0.078|ddc2d77d972d|
|K9-header3-FixedJointState3D|-|0|0.063|ddc2d77d972d|
|K9-header3-JointConnection|-|0|0.032|ddc2d77d972d|
|K9-header3-JointFrame2D|-|0|0.063|ddc2d77d972d|
|K9-header3-JointFrame3D|-|0|0.078|ddc2d77d972d|
|K9-header3-JointKind|-|0|0.078|ddc2d77d972d|
|K9-header3-JointLimitState|-|0|0.063|ddc2d77d972d|
|K9-header3-JointTargetMotion|-|0|0.078|ddc2d77d972d|
|K9-header3-JointTargetState|-|0|0.031|ddc2d77d972d|
|K9-header3-LinearJointDrive|-|0|0.062|ddc2d77d972d|
|K9-header3-LinearJointLimits|-|0|0.078|ddc2d77d972d|
|K9-header3-LinearJointTargetCommand|-|0|0.079|ddc2d77d972d|
|K9-header3-LinearJointTargetSettings|-|0|0.078|ddc2d77d972d|
|K9-header3-PrismaticJointComponent2D|-|0|0.203|ddc2d77d972d|
|K9-header3-PrismaticJointComponent3D|-|0|0.203|ddc2d77d972d|
|K9-header3-PrismaticJointComponentDescription2D|-|0|0.203|ddc2d77d972d|
|K9-header3-PrismaticJointComponentDescription3D|-|0|0.203|ddc2d77d972d|
|K9-header3-PrismaticJointDescription2D|-|0|0.063|ddc2d77d972d|
|K9-header3-PrismaticJointDescription3D|-|0|0.078|ddc2d77d972d|
|K9-header3-PrismaticJointObservation2D|-|0|0.078|ddc2d77d972d|
|K9-header3-PrismaticJointObservation3D|-|0|0.078|ddc2d77d972d|
|K9-header3-PrismaticJointState2D|-|0|0.062|ddc2d77d972d|
|K9-header3-PrismaticJointState3D|-|0|0.062|ddc2d77d972d|
|K9-header3-RevoluteJointComponent2D|-|0|0.203|ddc2d77d972d|
|K9-header3-RevoluteJointComponent3D|-|0|0.204|ddc2d77d972d|
|K9-header3-RevoluteJointComponentDescription2D|-|0|0.203|ddc2d77d972d|
|K9-header3-RevoluteJointComponentDescription3D|-|0|0.187|ddc2d77d972d|
|K9-header3-RevoluteJointDescription2D|-|0|0.063|ddc2d77d972d|
|K9-header3-RevoluteJointDescription3D|-|0|0.078|ddc2d77d972d|
|K9-header3-RevoluteJointObservation2D|-|0|0.078|ddc2d77d972d|
|K9-header3-RevoluteJointObservation3D|-|0|0.093|ddc2d77d972d|
|K9-header3-RevoluteJointState2D|-|0|0.078|ddc2d77d972d|
|K9-header3-RevoluteJointState3D|-|0|0.078|ddc2d77d972d|
|K9-header3-RigidBody2D|-|0|0.125|ddc2d77d972d|
|K9-header3-RigidBody3D|-|0|0.125|ddc2d77d972d|
|K9-header3-Order2D3D|-|0|0.219|ddc2d77d972d|
|K9-header3-ReverseOrder|-|0|0.219|ddc2d77d972d|
|K9-header3-TypedBodiesMovers|-|0|0.235|ddc2d77d972d|
|K9-code-seal-stage|-|0|0.032|e27d353f5f97|
|K9-regenerate-Portable|-|0|0.907|e27d353f5f97|
|K9-regenerate-Native|-|0|1.688|e27d353f5f97|
|K9-sealed-mechanism-mutations|Release|0|141.782|e27d353f5f97|
|K9-sealed-distance-mutations|Release|0|79.719|e27d353f5f97|
|K9-IDE2-normal|-|0|4.328|e27d353f5f97|
|K9-IDE2-development|-|0|5.984|e27d353f5f97|
|final3-Portable-Debug-build|Debug|0|11.422|e27d353f5f97|
|final3-Portable-Debug-registration|Debug|0|0.141|e27d353f5f97|
|final3-Portable-Debug-tests|Debug|8|164.313|e27d353f5f97|
|K9-owner-contract-Debug-build|Debug|0|1.265|a4e1354a6b56|
|K9-owner-contract-Debug-test|Debug|3221225477|2.109|a4e1354a6b56|
|K9-owner-contract2-Debug-build|Debug|0|1.25|d5771104eb55|
|K9-owner-contract2-Debug-test|Debug|0|0.218|d5771104eb55|
|K9-owner-contract2-Release-build|Release|0|1.891|d5771104eb55|
|K9-owner-contract2-Release-test|Release|0|0.047|d5771104eb55|
|K9-owner-fixed-stage|-|0|0.047|d5771104eb55|
|K9-owner-fixed-mechanism-mutations|-|0|135.75|d5771104eb55|
|K9-owner-fixed-distance-mutations|-|0|79.031|d5771104eb55|
|K9-final4-validation|-|1|1302.328|d5771104eb55|
|final4-Portable-Debug-build|Debug|0|5.078|d5771104eb55|
|final4-Portable-Debug-registration|Debug|0|0.125|d5771104eb55|
|final4-Portable-Debug-tests|Debug|0|163.359|d5771104eb55|
|final4-Portable-Release-build|Release|0|5.812|d5771104eb55|
|final4-Portable-Release-registration|Release|0|0.235|d5771104eb55|
|final4-Portable-Release-tests|Release|0|29.063|d5771104eb55|
|K9-benchmark-format-stage|-|0|0.047|1f7b8c8e6117|
|final4-Native-Debug-build|Debug|0|24.0|1f7b8c8e6117|
|final4-Native-Debug-registration|Debug|0|3.188|1f7b8c8e6117|
|final4-Native-Debug-tests|Debug|0|208.328|1f7b8c8e6117|
|final4-Native-Release-build|Release|0|49.735|1f7b8c8e6117|
|final4-Native-Release-registration|Release|0|1.0|1f7b8c8e6117|
|final4-Native-Release-tests|Release|0|67.015|1f7b8c8e6117|
|final4-standalone|-|0|268.656|1f7b8c8e6117|
|K9-environment-machine|-|0|1.625|1f7b8c8e6117|
|K9-environment-cmake|-|0|0.031|1f7b8c8e6117|
|K9-environment-python|-|0|0.032|1f7b8c8e6117|
|final4-package-off-Debug|Debug|0|19.11|1f7b8c8e6117|
|K9-post-format-benchmark-build|Release|0|1.531|1f7b8c8e6117|
|K8-package-off-Debug-final4|Debug|0|15.656|1f7b8c8e6117|
|final4-package-off-Release|Release|0|21.438|1f7b8c8e6117|
|K8-package-off-Release-final4|Release|0|19.734|1f7b8c8e6117|
|final4-package-on-Debug|Debug|0|36.5|1f7b8c8e6117|
|K8-package-on-Debug-final4|Debug|0|34.719|1f7b8c8e6117|
|K9-environment-json|-|0|1.422|1f7b8c8e6117|
|final4-package-on-Release|Release|0|39.422|1f7b8c8e6117|
|K8-package-on-Release-final4|Release|0|37.656|1f7b8c8e6117|
|final4-benchmark-pilot|Release|0|82.453|1f7b8c8e6117|
|final4-benchmark-main|Release|0|261.782|1f7b8c8e6117|
|final4-NoStl|-|0|0.468|1f7b8c8e6117|
|final4-Python|-|0|5.281|1f7b8c8e6117|
|final4-Python-IDE|-|0|5.625|1f7b8c8e6117|
|final4-diff-check|-|0|0.032|1f7b8c8e6117|
|K9-final-same-code-consolidation|-|0|412.687|1f7b8c8e6117|
|K9-final-code-mechanism-mutations|-|0|134.562|1f7b8c8e6117|
|K9-final-code-distance-mutations|-|0|79.015|1f7b8c8e6117|
|final5-Portable-Debug-build|Debug|0|4.266|1f7b8c8e6117|
|final5-Portable-Debug-registration|Debug|0|0.125|1f7b8c8e6117|
|final5-Portable-Debug-tests|Debug|0|162.25|1f7b8c8e6117|
|final5-Portable-Release-build|Release|0|2.156|1f7b8c8e6117|
|final5-Portable-Release-registration|Release|0|0.234|1f7b8c8e6117|
|final5-Portable-Release-tests|Release|0|29.016|1f7b8c8e6117|
|K9-final-full-IDE-validation|-|0|8.829|1f7b8c8e6117|
|K9-final-IDE-file-api-regenerate|-|0|2.469|1f7b8c8e6117|
|K9-final-native-registration-after-api|-|0|0.047|1f7b8c8e6117|
|K9-final-Python-all-IDE|-|0|5.812|1f7b8c8e6117|
|K9-before-commit-fetch|-|0|0.5|1f7b8c8e6117|
|K9-before-commit-head|-|0|0.031|1f7b8c8e6117|
|K9-before-commit-origin|-|0|0.032|1f7b8c8e6117|
|K9-split-quality-stage|-|0|0.031|1f7b8c8e6117|
|K9-product-staged-check|-|0|0.047|1f7b8c8e6117|
|K9-product-staged-stat|-|0|0.047|1f7b8c8e6117|
|K9-product-commit|-|0|0.156|1f7b8c8e6117|
|K9-quality-stage|-|0|0.031|1f7b8c8e6117|
|K9-quality-staged-check|-|0|0.015|1f7b8c8e6117|
|K9-quality-staged-stat|-|0|0.031|1f7b8c8e6117|
|K9-quality-commit|-|0|0.078|1f7b8c8e6117|

## 採用実行ファイル

|構成種別|構成|exe|SHA256|
|---|---|---|---|
|Portable|Debug|Build\MechanismJoints-20261001-6f1e47\Portable\Debug\dxf_physics_tests.exe|a99275109cd8083857d583c060446fe90e91ee40c39b5d622783174e40db8fa2|
|Portable|Debug|Build\MechanismJoints-20261001-6f1e47\Portable\Debug\dxf_tests.exe|0eb355c488367fed42a0b215d06e4d8bc7f5c90edaa01839407894a122567050|
|Portable|Release|Build\MechanismJoints-20261001-6f1e47\Portable\Release\dxf_physics_tests.exe|d3265e6808599e0e2188519da9fe6b2a80267ac8502d42facaf42206ed663599|
|Portable|Release|Build\MechanismJoints-20261001-6f1e47\Portable\Release\dxf_tests.exe|5d0ed0bf650431c1a130b0ecf569cf45535570564f311843d6562d345edecb10|
|Native|Debug|Build\MechanismJoints-20261001-6f1e47\Native\Debug\dxf_physics_tests.exe|aa3dd997e0e15d46152a72a5e1906c26871cdd6276915a6b35e74a4ed013d19e|
|Native|Debug|Build\MechanismJoints-20261001-6f1e47\Native\Debug\dxf_tests.exe|042e4cfcfcc264c22b26760e06f9c5e00da6a6cc390cc6813b16bdaee444c7f8|
|Native|Debug|Build\MechanismJoints-20261001-6f1e47\Native\Debug\NativeGameplaySmoke.exe|d652bd82e88cf53113179e62e75ae4e21c25003d961a832613e788630d33c6f6|
|Native|Debug|Build\MechanismJoints-20261001-6f1e47\Native\Debug\NativeModelSmoke.exe|f775eff191851c3645eba84208b796eb4fb065eae37efacc0f59b5082880cf1a|
|Native|Release|Build\MechanismJoints-20261001-6f1e47\Native\Release\dxf_physics_tests.exe|c42a90851231a80132320c4cea90c3b32b9b9cd2df1454d9df27bd8290116003|
|Native|Release|Build\MechanismJoints-20261001-6f1e47\Native\Release\dxf_tests.exe|18864a113277a549babcc6af9b192366f1a0478301a735aa3b3044ef4c926e0b|
|Native|Release|Build\MechanismJoints-20261001-6f1e47\Native\Release\NativeGameplaySmoke.exe|51b9689f8629e163d8e37a97698cc08c687f730a6f6803b624748937cdc61740|
|Native|Release|Build\MechanismJoints-20261001-6f1e47\Native\Release\NativeModelSmoke.exe|1f9561e0059c5b8a9527eb78ec5101328d070301bccbfd94dad159f7ad210c4e|
