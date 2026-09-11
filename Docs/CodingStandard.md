# コーディング規則

UE5の命名・可読性・レイアウトの考え方を基準にし、DxLibフレームワーク向けの接頭辞と、依頼されたメンバー／内部処理の規則を適用しています。Unreal Engine自体への依存はありません。

## 型の接頭辞

| 接頭辞 | 用途 | 例 |
|---|---|---|
| `D` | `DObject`を継承するRTTI対象オブジェクト | `DScene`, `DGameObject`, `DGameObjectComponent` |
| `F` | 通常のクラス、値型、設定・Contextなど | `FApplication`, `FAssetService`, `FFrameTime` |
| `I` | 抽象インターフェース | `ITextureBackend`, `IInputSource`, `IRenderControl` |
| `T` | テンプレート | `TResult<T>`, `TObjectHandle<T>`, `TManagedLifecycle<T>` |
| `E` | `enum class` | `EKey`, `ELifecycleState` |

`U`・`A`など、Unreal固有の型体系を連想させる接頭辞は使用しません。`D`はこのフレームワークのオブジェクト系を区別するための規約であり、UObjectのGC・反射・シリアライズを備える意味ではありません。

## メンバー変数

振る舞いや状態管理を持つクラスのメンバーには、値に `m_`、生ポインタ・スマートポインタ・関数ポインタに `m_p` を付けます。boolは `m_b` に続けて意味の分かる名前にします。

```cpp
FTexture m_Texture;
FVector2 m_Position;
bool m_bInitialized = false;
ITextureBackend* m_pBackend = nullptr;
std::unique_ptr<DGameInstance> m_pGame;
```

設定、要求、結果、Context、ID、メタデータなど、データ保持を主目的とする型のフィールドには付けません。宣言がclassかstructかだけでは判定しません。`TResult`の内部データもこの区分です。RAIIや資源の有効性を管理する型は振る舞い型として扱います。

```cpp
struct FPlaybackOptions
{
    bool bLoop = false;
    float Volume = 1.0f;
    std::uint64_t Scope = 0;
};
```

世代付きの `TObjectHandle<T>` はメモリの所有ポインタではなく、所有領域とIDを表す値型です。これを保持するメンバーは `m_Player` のように表記します。

## 関数

公開APIはPascalCase、boolの問い合わせは `IsDown`・`WasPressed`・`IsInitialized` のように意味を明確にします。内部の補助処理や、基盤だけが使うディスパッチ入口は末尾を `_Internal` にします。

```cpp
TResult<void> Initialize_Internal(const FInitContext& Context);
void FinishDispatch_Internal() noexcept;
TResult<void> RestoreTarget_Internal();
```

`_Internal` は公開／非公開の代わりではありません。複数の担当クラスから呼ぶためpublicに置く入口もありますが、ゲーム側から直接呼ぶ用途ではありません。公開された実用部品の `Load`・`Play`・`Update` などは通常のAPIなので、内部専用の接尾辞は付けません。

ユーザー拡張点は `OnInitialize`・`OnTick`・`OnDraw`・`OnDeinitialize`・`OnEnter`・`OnExit` です。これらは内部処理ではなく、意図的にoverrideするフックです。コンストラクタ、デストラクタ、演算子にも `_Internal` は付けません。

## レイアウトと実装

波括弧はAllman、インデントはタブ（表示幅4）、型・関数・ローカル変数はPascalCaseです。設定は `.editorconfig` と `.clang-format` に含めています。ヘッダーは `#pragma once`、cppでは対応する公開ヘッダーを先頭にincludeし、PublicとPrivateを分離します。テンプレートと短いAPIはヘッダー内実装です。

コピーで所有関係が壊れる型はコピー禁止にします。基底経由で破棄する型にはvirtualデストラクタを置きます。個々のオブジェクトは基本的に単独所有、画像など実際に共有する資源だけ共有所有にします。

Unreal専用のコンテナ・マクロ・ビルドシステムは持ち込みません。C++20、標準RTTI、標準ライブラリ、C++例外を使用します。`OnDeinitialize`・`OnEnter`・`OnExit` はnoexcept契約です。それ以外のユーザーフックの例外はApplication境界などでエラーへ変換します。一般のC++構築やメモリ確保まで「絶対に例外を投げない」保証をするAPIではありません。
