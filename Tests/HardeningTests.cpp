#include "Toolbox/UniquePtr.h"
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/GameObjectCollection.h"
#include "Dxf/GameObject.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/RenderSystem2D.h"
#include "Toolbox/Function.h"
#include "Toolbox/Utility.h"

using namespace Dxf;
using namespace Dxf::Testing;

namespace
{
// 追加の動作を持たない登録用オブジェクト。
class DPlainObject final : public DObject
{
};

// 破棄時のコールバックで再入処理を再現する。
class DDestructionCallback final : public DObject
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DDestructionCallback(Toolbox::TFunction<void()> Callback) : m_Callback(Toolbox::Move(Callback))
	{
	}
	// 所有データを解放し、必要な破棄の観測を行う。
	~DDestructionCallback() override
	{
		m_Callback();
	}

private:
	// 再入を再現するための任意処理。
	Toolbox::TFunction<void()> m_Callback;
};

// 各ライフサイクルフックの呼び出し回数。
struct FHookCounts
{
	// 初期化フックを呼んだ回数。
	Toolbox::int32 Initialize = 0;
	// 更新フックを呼んだ回数。
	Toolbox::int32 Tick = 0;
	// 描画フックを呼んだ回数。
	Toolbox::int32 Draw = 0;
	// 終了処理フックを呼んだ回数。
	Toolbox::int32 Stop = 0;
	// シーン開始フックを呼んだ回数。
	Toolbox::int32 Enter = 0;
};

// 各ライフサイクルに任意の検証処理を挿入する。
class DHookObject final : public DGameObject
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	DHookObject(FHookCounts& Counts, Toolbox::TFunction<void()> Init = {}, Toolbox::TFunction<void()> Tick = {},
	            Toolbox::TFunction<void()> Draw = {})
	    : m_pCounts(&Counts), m_Init(Toolbox::Move(Init)), m_Tick(Toolbox::Move(Tick)), m_Draw(Toolbox::Move(Draw))
	{
	}

protected:
	// 初期化の呼び出しを観測し、指定した検証条件を適用する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		if (m_Init)
		{
			m_Init();
		}
		return {};
	}
	// 更新の呼び出しを観測し、指定された処理を実行する。
	void OnTick(const FTickContext&) override
	{
		++m_pCounts->Tick;
		if (m_Tick)
		{
			m_Tick();
		}
	}
	// 描画の呼び出しを観測し、指定された処理を実行する。
	void OnDraw(FRenderContext&) const override
	{
		++m_pCounts->Draw;
		if (m_Draw)
		{
			m_Draw();
		}
	}
	// 終了処理の呼び出しを観測する。
	void OnDeinitialize() noexcept override
	{
		++m_pCounts->Stop;
	}

private:
	// 外部のライフサイクル集計への参照。
	FHookCounts* m_pCounts;
	// 初期化中の再入操作を再現するコールバック。
	Toolbox::TFunction<void()> m_Init;
	// 更新中の再入操作を再現するコールバック。
	Toolbox::TFunction<void()> m_Tick;
	// 描画中の再入操作を再現するコールバック。
	Toolbox::TFunction<void()> m_Draw;
};

// 遷移準備中の操作と失敗を再現する。
class DPreparationScene final : public DScene
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	DPreparationScene(FHookCounts& Counts, Toolbox::TFunction<void()> Init = {})
	    : m_pCounts(&Counts), m_Init(Toolbox::Move(Init))
	{
	}

protected:
	// 初期化の呼び出しを観測し、指定した検証条件を適用する。
	TResult<void> OnInitialize(const FInitContext&) override
	{
		++m_pCounts->Initialize;
		if (m_Init)
		{
			m_Init();
		}
		return {};
	}
	// 遷移先へ入ったタイミングを観測する。
	void OnEnter(const FSceneActivationContext&) noexcept override
	{
		++m_pCounts->Enter;
	}
	// 終了処理の呼び出しを観測する。
	void OnDeinitialize() noexcept override
	{
		++m_pCounts->Stop;
	}

private:
	// 外部のライフサイクル集計への参照。
	FHookCounts* m_pCounts;
	// 初期化中の再入操作を再現するコールバック。
	Toolbox::TFunction<void()> m_Init;
};
} // namespace

TEST("Slot removal invalidates generation before running a reentrant destructor")
{
	// 検証対象を所有する世代付き格納先。
	TSlotMap<DObject> Storage;
	// 削除後に差し替えた対象。
	TObjectHandle<DObject> Replacement;
	// 変更前に保持した元の値。
	auto Original = Storage.Insert(Toolbox::MakeUnique<DDestructionCallback>(
	    [&]()
	    {
		    Replacement = Storage.Insert(Toolbox::MakeUnique<DPlainObject>());
	    }));
	REQUIRE(Storage.Remove(Original));
	REQUIRE(!Original);
	REQUIRE(Replacement);
	REQUIRE(Storage.Size() == 1);
	REQUIRE(Storage.Remove(Replacement));
	REQUIRE(Storage.Size() == 0);
}

TEST("Collection shutdown in Tick prevents later objects from ticking")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 最初に生成または登録した対象。
	FHookCounts First;
	// 二番目に生成または登録した対象。
	FHookCounts Second;
	FGameObjectCollection Objects;
	REQUIRE(Objects.Spawn<DHookObject>(First, Toolbox::TFunction<void()>{},
	                                   [&]()
	                                   {
		                                   Objects.Shutdown_Internal();
	                                   }));
	REQUIRE(Objects.Spawn<DHookObject>(Second));
	REQUIRE(Objects.CommitBoundary_Internal({Assets}));
	// 検証で配信する入力状態。
	FInputSnapshot Input;
	REQUIRE(Objects.Tick_Internal({Input, {}}));
	REQUIRE(First.Tick == 1);
	REQUIRE(Second.Tick == 0);
	REQUIRE(First.Stop == 1);
	REQUIRE(Second.Stop == 1);
	REQUIRE(Objects.Size() == 0);
}

TEST("Collection shutdown during preparation skips remaining initializations")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 最初に生成または登録した対象。
	FHookCounts First;
	// 二番目に生成または登録した対象。
	FHookCounts Second;
	FGameObjectCollection Objects;
	REQUIRE(Objects.Spawn<DHookObject>(First,
	                                   [&]()
	                                   {
		                                   Objects.Shutdown_Internal();
	                                   }));
	REQUIRE(Objects.Spawn<DHookObject>(Second));
	REQUIRE(Objects.CommitBoundary_Internal({Assets}));
	REQUIRE(First.Initialize == 1);
	REQUIRE(Second.Initialize == 0);
	REQUIRE(First.Stop == 1);
	REQUIRE(Second.Stop == 0);
	REQUIRE(Objects.Size() == 0);
}

TEST("Standalone object Tick translates user exceptions to TResult")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// ライフサイクルの観測回数。
	FHookCounts Counts;
	DHookObject Object(Counts, {},
	                   []()
	                   {
		                   throw Toolbox::FException("tick failure");
	                   });
	REQUIRE(Object.Initialize_Internal({Assets}));
	// 検証で配信する入力状態。
	FInputSnapshot Input;
	// 検証対象の操作が返した成否と値。
	auto Result = Object.Tick_Internal({Input, {}});
	REQUIRE(!Result);
	REQUIRE(Result.Error().Code == EErrorCode::UserException);
	Object.Shutdown_Internal();
	REQUIRE(Counts.Stop == 1);
}

TEST("Standalone object Draw translates unknown user exceptions to TResult")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem2D Renderer(Backend);
	// ライフサイクルの観測回数。
	FHookCounts Counts;
	DHookObject Object(Counts, {}, {},
	                   []()
	                   {
		                   throw 42;
	                   });
	REQUIRE(Object.Initialize_Internal({Assets}));
	// 検証対象の操作が返した成否と値。
	auto Result = Object.Draw_Internal(Renderer.GetContext());
	REQUIRE(!Result);
	REQUIRE(Result.Error().Code == EErrorCode::UserException);
	Object.Shutdown_Internal();
	REQUIRE(Counts.Stop == 1);
}

TEST("Navigator shutdown during preparation never activates the replacement")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	FHookCounts Previous;
	FHookCounts Next;
	// 遷移と生存期間を管理するシーン一覧。
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Previous));
	REQUIRE(Scenes.Commit());
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Next,
	                                                [&]()
	                                                {
		                                                Scenes.Shutdown();
	                                                }));
	auto Commit = Scenes.Commit();
	REQUIRE(Commit);
	REQUIRE(!Commit.Value());
	REQUIRE(Next.Enter == 0);
	REQUIRE(Next.Stop == 1);
	REQUIRE(Previous.Stop == 1);
	REQUIRE(Scenes.GetCurrent() == nullptr);
}

TEST("Navigator quit during preparation retains current scene until shutdown")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	FHookCounts Previous;
	FHookCounts Next;
	// 遷移と生存期間を管理するシーン一覧。
	FSceneNavigator Scenes(Assets, Audio);
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Previous));
	REQUIRE(Scenes.Commit());
	// 現在選択されている対象。
	auto* Current = Scenes.GetCurrent();
	REQUIRE(Scenes.RequestChange<DPreparationScene>(Next,
	                                                [&]()
	                                                {
		                                                Scenes.RequestQuit();
	                                                }));
	auto Commit = Scenes.Commit();
	REQUIRE(Commit && !Commit.Value());
	REQUIRE(Scenes.GetCurrent() == Current);
	REQUIRE(Next.Enter == 0);
	REQUIRE(Next.Stop == 1);
	Scenes.Shutdown();
}

TEST("Asset paths reject embedded NUL before reaching a backend")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	const Toolbox::FString Invalid("visible.bmp\0different.bmp", 25);
	REQUIRE(!Assets.LoadTexture(Invalid));
	REQUIRE(!Assets.LoadSound(Invalid));
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
	REQUIRE(Backend.GetTrace().SoundLoads == 0);
}

TEST("Asset paths reject overlong UTF8 surrogate and truncated sequences")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	for (const auto& Invalid : {Toolbox::FString("\xc0\xaf"), Toolbox::FString("\xed\xa0\x80"),
	                            Toolbox::FString("\xe3\x81"), Toolbox::FString("\xf4\x90\x80\x80")})
	{
		REQUIRE(!Assets.LoadTexture(Invalid));
		REQUIRE(!Assets.LoadSound(Invalid));
	}
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
	REQUIRE(Backend.GetTrace().SoundLoads == 0);
}

TEST("Asset paths accept valid Japanese and four byte UTF8")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(Assets.LoadTexture("Assets/\xe7\x94\xbb\xe5\x83\x8f/\xf0\x9f\x90\xa6.bmp"));
}

TEST("Direct loaders reject empty paths and invalid sound storage enums")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	FResourceRegistry Registry;
	// 生存中の画像資源番号。
	FTextureLoader Textures(Backend, Registry);
	// 音声資源ごとの再生状態。
	FSoundLoader Sounds(Backend, Registry);
	REQUIRE(!Textures.Load("", {}));
	REQUIRE(!Sounds.Load("", {}));
	// 検証条件を指定する読み込みオプション。
	FSoundLoadOptions Options;
	Options.Storage = static_cast<ESoundStorage>(777);
	REQUIRE(!Sounds.Load("a.wav", Options));
}

TEST("Font families reject embedded NUL and malformed UTF8")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証条件を指定する読み込みオプション。
	FFontOptions Options;
	Options.Family = Toolbox::FString("Meiryo\0X", 8);
	REQUIRE(!Assets.LoadFont(Options));
	Options.Family = "\xff";
	REQUIRE(!Assets.LoadFont(Options));
	REQUIRE(Backend.GetTrace().Fonts.IsEmpty());
}

TEST("Sound resources from another backend are rejected before duplication")
{
	// 最初に生成または登録した対象。
	FFakeBackend First;
	// 二番目に生成または登録した対象。
	FFakeBackend Second;
	// 検証に使用する資源管理。
	FAssetService Assets(First, First, First);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Second);
	// 検証で使用する音声資源。
	auto Sound = Assets.LoadSound("a.wav").Value();
	REQUIRE(!Audio.Play(Sound));
	REQUIRE(Second.GetTrace().Clones == 0);
}

TEST("Render execution failure prevents presenting a partial frame")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 描画を実行する検証用レンダラー。
	FRenderSystem2D Renderer(Backend);
	// 検証で使用する画像資源。
	auto Texture = Assets.LoadTexture("a.bmp").Value();
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Renderer.GetContext().Draw(Texture, {}));
	Backend.GetTrace().bFailDraw = true;
	REQUIRE(!Renderer.Flush());
	Backend.GetTrace().bFailDraw = false;
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 0);
	REQUIRE(Renderer.BeginFrame(100, 100));
	REQUIRE(Renderer.GetContext().Draw(Texture, {}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.GetTrace().Presentations == 1);
}

TEST("Resource registry rejects null records")
{
	FResourceRegistry Registry;
	REQUIRE(!Registry.Register(nullptr));
}

TEST("A scene destroyed during preparation is never activated")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 再生状態を管理する音声サービス。
	FAudioPlayer Audio(Backend);
	// ライフサイクルの観測回数。
	FHookCounts Counts;
	// 遷移と生存期間を管理するシーン一覧。
	FSceneNavigator Scenes(Assets, Audio);
	// 検証対象のシーン。
	DPreparationScene* Scene = nullptr;
	// まだ適用されていない操作。
	auto Pending = Toolbox::MakeUnique<DPreparationScene>(Counts,
	                                                      [&]()
	                                                      {
		                                                      Scene->RequestDestroy_Internal();
	                                                      });
	Scene = Pending.Get();
	REQUIRE(Scenes.RequestChange(Toolbox::Move(Pending)));
	// 検証対象の操作が返した成否と値。
	auto Result = Scenes.Commit();
	REQUIRE(!Result);
	REQUIRE(Counts.Enter == 0);
	REQUIRE(Counts.Stop == 1);
	REQUIRE(Scenes.GetCurrent() == nullptr);
}

TEST("Text rendering rejects malformed UTF8 and embedded NUL before dispatch")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証で使用するフォント資源。
	auto Font = Assets.LoadFont().Value();
	// 描画を実行する検証用レンダラー。
	FRenderSystem2D Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(320, 240));
	REQUIRE(!Renderer.GetContext().DrawText(Font, Toolbox::FString("a\0b", 3), {}));
	REQUIRE(!Renderer.GetContext().DrawText(Font, Toolbox::FString("\xc0\xaf", 2), {}));
	REQUIRE(Renderer.GetContext().DrawText(Font, "", {}));
	REQUIRE(Renderer.GetContext().DrawText(Font, "日本語", {}));
	REQUIRE(Renderer.EndFrame());
}

namespace
{
// 構築中の停止要求を再現する。
class DConstructorStop final : public DGameObject
{
public:
	// 検証に必要な依存先と初期状態を設定する。
	explicit DConstructorStop(FGameObjectCollection& Collection)
	{
		Collection.Shutdown_Internal();
	}
};
} // namespace
TEST("A collection stopped by an object constructor must reject that spawn")
{
	// 検証対象の要素集合。
	FGameObjectCollection Collection;
	REQUIRE(!Collection.Spawn<DConstructorStop>(Collection));
	REQUIRE(Collection.Size() == 0);
	// ライフサイクルの観測回数。
	FHookCounts Counts;
	REQUIRE(!Collection.Spawn<DHookObject>(Counts));
}

TEST("Invalid sound storage must not alias an existing streamed cache entry")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	// 検証用ファイルの書き込み先。
	auto Stream = Assets.LoadSound("music.wav", {ESoundStorage::Stream});
	REQUIRE(Stream);
	REQUIRE(!Assets.LoadSound("music.wav", {static_cast<ESoundStorage>(99)}));
	REQUIRE(Backend.GetTrace().SoundLoads == 1);
}
