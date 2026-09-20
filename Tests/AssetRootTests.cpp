#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Toolbox/ProjectPaths.h"
#include "Dxf/AssetService.h"
#include "Dxf/Application.h"
#include "SandboxGame.h"

using namespace Dxf;
using namespace Dxf::Testing;

TEST("asset resolver joins the project root with relative paths")
{
	// 解決対象の基準ディレクトリ。
	Toolbox::FAssetPathResolver Resolver;
	REQUIRE(Resolver.SetRoot(Toolbox::FPath("C:/Project")));
	// 解決した絶対パス。
	Toolbox::FPath Resolved;
	REQUIRE(Resolver.Resolve("Assets/player.bmp", Resolved));
	REQUIRE(Resolved.ToUtf8() == "C:/Project/Assets/player.bmp");
}

TEST("asset resolver normalizes dot segments inside the root")
{
	// 解決対象の基準ディレクトリ。
	Toolbox::FAssetPathResolver Resolver;
	REQUIRE(Resolver.SetRoot(Toolbox::FPath("C:/Project")));
	// 解決した絶対パス。
	Toolbox::FPath Resolved;
	REQUIRE(Resolver.Resolve("Assets/./Sub/../player.bmp", Resolved));
	REQUIRE(Resolved.ToUtf8() == "C:/Project/Assets/player.bmp");
}

TEST("asset resolver rejects paths escaping the project root")
{
	// 解決対象の基準ディレクトリ。
	Toolbox::FAssetPathResolver Resolver;
	REQUIRE(Resolver.SetRoot(Toolbox::FPath("C:/Project")));
	// 解決した絶対パス。
	Toolbox::FPath Resolved;
	REQUIRE(!Resolver.Resolve("Assets/../../outside.bmp", Resolved));
	REQUIRE(!Resolver.Resolve("../outside.bmp", Resolved));
}

TEST("asset resolver rejects ambiguous and empty paths")
{
	// 解決対象の基準ディレクトリ。
	Toolbox::FAssetPathResolver Resolver;
	REQUIRE(Resolver.SetRoot(Toolbox::FPath("C:/Project")));
	// 解決した絶対パス。
	Toolbox::FPath Resolved;
	REQUIRE(!Resolver.Resolve("", Resolved));
	REQUIRE(!Resolver.Resolve("C:Assets/player.bmp", Resolved));
	REQUIRE(!Resolver.Resolve("\\Assets\\player.bmp", Resolved));
	REQUIRE(!Resolver.Resolve("/Assets/player.bmp", Resolved));
}

TEST("asset resolver passes absolute paths through without the root")
{
	// 解決対象の基準ディレクトリ。
	Toolbox::FAssetPathResolver Resolver;
	REQUIRE(Resolver.SetRoot(Toolbox::FPath("C:/Project")));
	// 解決した絶対パス。
	Toolbox::FPath Resolved;
	REQUIRE(Resolver.Resolve("D:/Shared/player.bmp", Resolved));
	REQUIRE(Resolved.ToUtf8() == "D:/Shared/player.bmp");
	REQUIRE(Resolver.Resolve("//Server/Share/player.bmp", Resolved));
	REQUIRE(Resolved.ToUtf8() == "//Server/Share/player.bmp");
}

TEST("asset resolver keeps japanese paths intact")
{
	// 解決対象の基準ディレクトリ。
	Toolbox::FAssetPathResolver Resolver;
	REQUIRE(Resolver.SetRoot(Toolbox::FPath("C:/プロジェクト")));
	// 解決した絶対パス。
	Toolbox::FPath Resolved;
	REQUIRE(Resolver.Resolve("Assets/画像/🐦.bmp", Resolved));
	REQUIRE(Resolved.ToUtf8() == "C:/プロジェクト/Assets/画像/🐦.bmp");
}

TEST("project path settings parse the development file")
{
	// 読み取った開発パス設定。
	Toolbox::FProjectPathSettings Settings;
	REQUIRE(Toolbox::ParseProjectPathSettings("Version=1\nMode=Development\nProjectRootRelative=../..\n", Settings));
	REQUIRE(Settings.Version == 1);
	REQUIRE(Settings.Mode == "Development");
	REQUIRE(Settings.ProjectRootRelative == "../..");
	REQUIRE(!Toolbox::ParseProjectPathSettings("Version=2\nMode=Development\nProjectRootRelative=..\n", Settings));
	REQUIRE(!Toolbox::ParseProjectPathSettings("Mode=Development\n", Settings));
	REQUIRE(!Toolbox::ParseProjectPathSettings("", Settings));
}

TEST("asset service with a root hands resolved paths to the backend")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(Assets.SetProjectRoot(Toolbox::FPath("C:/Project")));
	REQUIRE(Assets.LoadTexture("Assets/player.bmp"));
	REQUIRE(Assets.LoadSound("Assets/confirm.wav"));
	REQUIRE(Backend.GetTrace().TexturePaths.Size() == 1);
	REQUIRE(Backend.GetTrace().TexturePaths[0] == "C:/Project/Assets/player.bmp");
	REQUIRE(Backend.GetTrace().SoundPaths.Size() == 1);
	REQUIRE(Backend.GetTrace().SoundPaths[0] == "C:/Project/Assets/confirm.wav");
}

TEST("asset service shares cache entries for equal normalized paths")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(Assets.SetProjectRoot(Toolbox::FPath("C:/Project")));
	// 先に読み込んだ実体。弱参照キャッシュの再利用に必要。
	auto First = Assets.LoadTexture("Assets/player.bmp");
	REQUIRE(First);
	REQUIRE(Assets.LoadTexture("Assets/./Sub/../player.bmp"));
	REQUIRE(Backend.GetTrace().TextureLoads == 1);
}

TEST("asset service rejects escaping paths without backend access")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(Assets.SetProjectRoot(Toolbox::FPath("C:/Project")));
	// 解決に失敗した読み込み結果。
	auto Result = Assets.LoadTexture("Assets/../../outside.bmp");
	REQUIRE(!Result && Result.Error().Code == EErrorCode::InvalidArgument);
	REQUIRE(Backend.GetTrace().TextureLoads == 0);
}

TEST("asset service rejects a second project root")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証に使用する資源管理。
	FAssetService Assets(Backend, Backend, Backend);
	REQUIRE(Assets.SetProjectRoot(Toolbox::FPath("C:/Project")));
	REQUIRE(!Assets.SetProjectRoot(Toolbox::FPath("D:/Other")));
	REQUIRE(Assets.GetProjectRoot().ToUtf8() == "C:/Project");
}

TEST("asset services with different roots do not share resources")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 一つ目の基準で使う資源管理。
	FAssetService First(Backend, Backend, Backend);
	// 二つ目の基準で使う資源管理。
	FAssetService Second(Backend, Backend, Backend);
	REQUIRE(First.SetProjectRoot(Toolbox::FPath("C:/First")));
	REQUIRE(Second.SetProjectRoot(Toolbox::FPath("C:/Second")));
	REQUIRE(First.LoadTexture("Assets/player.bmp"));
	REQUIRE(Second.LoadTexture("Assets/player.bmp"));
	REQUIRE(Backend.GetTrace().TextureLoads == 2);
	REQUIRE(Backend.GetTrace().TexturePaths[0] == "C:/First/Assets/player.bmp");
	REQUIRE(Backend.GetTrace().TexturePaths[1] == "C:/Second/Assets/player.bmp");
}
TEST("development settings resolve the project root from the exe directory")
{
	// 解決したProjectRoot。
	Toolbox::FPath Root;
	REQUIRE(Toolbox::ResolveDevelopmentRoot(Toolbox::FPath("C:/Work/Build/Debug"),
	                                        "Version=1\nMode=Development\nProjectRootRelative=../..\n", Root));
	REQUIRE(Root.ToUtf8() == "C:/Work");
	// 実生成ファイルの末尾区切り付き相対パス。
	REQUIRE(Toolbox::ResolveDevelopmentRoot(Toolbox::FPath("C:/Work/Build/Debug"),
	                                        "Version=1\nMode=Development\nProjectRootRelative=../../\n", Root));
	REQUIRE(Root.ToUtf8() == "C:/Work");
	REQUIRE(!Toolbox::ResolveDevelopmentRoot(Toolbox::FPath("C:/Work/Build/Debug"), "Version=1\n", Root));
	REQUIRE(!Toolbox::ResolveDevelopmentRoot(Toolbox::FPath("C:/Work/Build/Debug"), "broken", Root));
	// 別ボリューム等で相対化できない構成の開発専用絶対パス。
	REQUIRE(Toolbox::ResolveDevelopmentRoot(Toolbox::FPath("C:/Work/Build/Debug"),
	                                        "Version=1\nMode=Development\nProjectRootRelative=D:/Other/Root\n", Root));
	REQUIRE(Root.ToUtf8() == "D:/Other/Root");
}
TEST("platform reports directories and rejects missing entries")
{
	REQUIRE(Toolbox::IsDirectory(Toolbox::CurrentDirectory()));
	REQUIRE(!Toolbox::IsDirectory(Toolbox::CurrentDirectory() / "DefinitelyMissingDirectory12345"));
}
TEST("application resolves scene assets against the explicit project root")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// サービス参照をまとめた起動引数。
	Dxf::FBackendServices Services{Backend, Backend, Backend, Backend, Backend, Backend};
	// 初期化に使用する設定。
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = "C:/TestRoot";
	// 検証するアプリケーション。
	Dxf::FApplication App(Services, Settings, Toolbox::MakeUnique<Dxf::Sandbox::DSandboxGameInstance>());
	REQUIRE(App.Start(Toolbox::MakeUnique<Dxf::Sandbox::DSandboxScene>("Assets")));
	REQUIRE(Backend.GetTrace().TexturePaths.Size() == 1);
	REQUIRE(Backend.GetTrace().TexturePaths[0] == "C:/TestRoot/Assets/player.bmp");
	REQUIRE(Backend.GetTrace().SoundPaths.Size() == 1);
	REQUIRE(Backend.GetTrace().SoundPaths[0] == "C:/TestRoot/Assets/confirm.wav");
}
TEST("application rejects a relative explicit project root")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// サービス参照をまとめた起動引数。
	Dxf::FBackendServices Services{Backend, Backend, Backend, Backend, Backend, Backend};
	// 初期化に使用する設定。
	Dxf::FApplicationSettings Settings;
	Settings.ProjectRoot = "relative/path";
	// 検証するアプリケーション。
	Dxf::FApplication App(Services, Settings, Toolbox::MakeUnique<Dxf::Sandbox::DSandboxGameInstance>());
	// 起動に失敗した結果。
	auto Result = App.Start(Toolbox::MakeUnique<Dxf::Sandbox::DSandboxScene>("Assets"));
	REQUIRE(!Result && Result.Error().Code == EErrorCode::InvalidArgument);
}
