# Scene／Prefabの初期構成

`dxf::scene_content`は任意リンクの静的ライブラリです。UTF-8 JSONを検証し、既存Gameplay Componentへ初期構成を渡します。実行状態の保存、世代IDの復元、任意C++型の生成、スクリプト、編集GUIは提供しません。Contentを使わないFramework／PhysicsOnly／SupportOnlyの利用方法は変わりません。

## 形式・単位

JSON文法は[RFC 8259](https://www.rfc-editor.org/rfc/rfc8259.html)を基準にします。重複キー拒否、有限数値、未知項目拒否と資源上限はContent形式の追加制約です。

最上位は整数`schema: 1`、`kind: "prefab"`または`"scene"`、整数`dimension: 2`または`3`です。メンバー順は自由、生成配列の順は保持します。整数項目は小数・指数表記を拒否します。未知項目・版・種類、復号後の重複キー、不正UTF-8、escape、単独surrogate、コメント、末尾comma／データ、数値overflowを拒否します。先頭BOM一つは許容し、同梱JSONはBOMなしです。`metadata`だけは補足値として受理し、生成処理には使用しません。

IDは`[A-Za-z_][A-Za-z0-9_-]*`、表示名・ラベル・パスはUTF-8です。Content文字列中のNULを拒否します。論理IDはPrefab個体の範囲で解決し、毎Tickの全世界名前検索は行いません。実行時Body／Joint／ObjectのIDを保存しません。

単位はm、kg、s、rad、2DはX右／Y上、3DのQuaternionは`[x,y,z,w]`です。有効なQuaternionは正規化します。配置は平行移動と回転だけで、scale／shear等は拒否します。モデル自身の`visual.scale`は描画だけの別設定です。

初期位置・姿勢・線速度・3D角速度は配置変換を一度だけ適用します。BodyローカルCollider・Joint Frameは再変換せず、Scene重力はWorld座標のままです。生成後の物理Poseを初期値で上書きしません。

## Prefab

完全な例は`Assets/Content/door2d.dxfprefab.json`と3D版です。外部利用の小さい例は`Tools/PackageConsumer/Data/prefab2d.dxfprefab.json`と3D版です。本体パーサーと実Worldへ渡す検証入力として使用します。

|項目|内容|
|---|---|
|parameters|bool、number、vector2、vector3、color、assetの宣言、default、数値min/max。許可位置の`{"parameter":"speed"}`だけを解釈|
|assets|型付きキー→Texture／Model／Sound／Font。Fontはfamily/size、その他はpathと種類別オプション|
|parts|id、任意name、bodyまたは非物理pose、colliders、visual。空の論理部品も許す|
|body|Static／Dynamic／Kinematic、position、angleまたはrotation、mass／inertia／減衰等。`adapter: "KinematicMover"`指定は既存専任Componentへ接続|
|colliders|Box／Sphere（2D Circleも可）／Capsule、ローカル位置・寸法、Solid／Sensor、category／mask／queryCategory等|
|joints|Distance／Revolute／Fixed／Prismatic、bodyA/B、connected、種類別Frame／Length／Drive／Limit|
|exports|公開名→`part/body`、`part/sensor`、Joint名、`asset/asset`、`child/export`。型とindexへ解決|
|children|id、ProjectRoot相対prefab、placement、parameters。内部名への直接アクセスは不可|

負寸法、非有限値、同一Body接続、非Dynamic両端、種類違いのDrive、無効Frame／Limit／材質／資源参照を拒否します。Local FrameはframeA／frameB、共通Frameは`frame: {"space":"Prefab", ...}`です。共通Frameは初期Body姿勢からLocalへ一度変換します。Distanceはlength／anchorA／anchorBを使用します。Motorと角度・Limitは[Joint仕様](../Physics/Joints.md)に従います。

Overrideは個体専用で、不明名・型違い・範囲外を拒否します。定義共有先を書き換えず、別個体・次回生成は既定値を保持します。式評価や任意JSON pathの書換えはありません。

子Prefabの依存循環は拒否し、同じ定義を複数から参照するDAGは許します。子の公開exportだけを親へ取り込みます。物理Jointグラフの閉路とは別の検査です。I/OなしのParsePrefab2D/3Dでは外部childrenを展開できず、FSceneContentSourceを使用します。

## Scene

`Assets/Content/course2d.dxfscene.json`と3D版は、同じPrefabを扉A／Bと回転配置の昇降装置へ展開します。

Sceneはgravity、任意fixedUpdate（stepSeconds／maxStepsPerFrame／maximumFrameSeconds）、共通assets、prefabs（キー→相対パス）、instances（id／prefab／placement／parameters）、views、connectionsを持ちます。2D viewはorigin／pixelsPerMeter／任意viewport、3D viewは既存の静止カメラ・照明・viewport設定へ接続します。Viewは最大二つです。

connectionsは`bodyA: "instance/export"`とbodyB、種類別設定を持ち、共通FrameのspaceはSceneです。全個体Body登録後に専任Joint Componentで接続します。別Scene／Worldや未公開内部Bodyを探しません。

## パス・診断・上限

全参照は固定ProjectRootからの相対パスです。定義ファイル位置やCWDへ暗黙に切り替えず、Assets/も二重付加しません。絶対、drive-relative、root-relative、Root外への..はContent入力として拒否します。既存同期Loadの許可範囲を変更せず、symlink／reparse pointによる実体隔離は保証しません。

FSceneContentDiagnosticは論理パス、資源キー、行・列、JSON位置、依存経路、理由を持ちます。資源／実行時検査でJSON位置を取得できない場合は行・列を0として区別し、推測した1行1列を表示しません。

|対象|既定上限|
|---|---:|
|一ファイル／要求全体|4 MiB／32 MiB|
|JSON深度／Prefab依存深度|64／16（最上位を1）|
|異なる定義ファイル|128|
|展開部品／Joint／資源|4096／8192／1024|
|一文字列／ID|64 KiB／128 bytes|

FSceneContentLimitsで有限上限を指定します。同じ要求の同じ字句的絶対ファイルは一回分の読取バイトとして数え、個体の展開数は合算します。確保前に合計を検査し、超過を切り捨て成功にしません。JSON再帰には深度256の追加上限があります。

所有とReadyは[Prefab利用](Prefabs.md)、準備・取消し・失敗保証は[読込](Loading.md)を参照してください。試行の成功範囲は開発記録で区別します。
