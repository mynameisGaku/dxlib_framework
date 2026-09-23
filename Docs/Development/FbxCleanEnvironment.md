# FBX配布物を独立したWindows環境で検証する

この手順は検証計画であり、未導入環境での成功記録ではありません。2026-09-23の作業PCには比較用Autodesk FBX SDKが存在します。利用可能な別のWindows環境はなく、SDKを一度も導入していない環境でのSetupからの試験は未実施です。

## 環境を分ける

- **ビルド機**：Autodesk FBX SDKを一度も導入していないWindows x64。Visual StudioのC++デスクトップ開発、Windows SDK（fxc）、CMake 3.24以降、Pythonを用意します。Direct3D11と実画面が必要です。
- **実行機**：Visual Studio、Windows SDK、fxc、Autodesk FBX SDKを導入していないWindows x64。ゲーム用Direct3D11が動作する環境を使用します。Windows標準の描画DLLの存在と、開発用コンパイラーの導入は別に記録します。
- OS・GPU・ドライバー・導入ソフト・環境変数・ZIPのSHA-256を記録します。SDKのフォルダー名変更やPATHの除外だけを「未導入」とは扱いません。

## ソース配布からSetupと両構成を確認する

開発元で `python Tools/PackageRelease.py --output Build/Packages/fbx-combined.zip` を実行し、ZIPと`.sha256`を渡します。新規ディレクトリへ展開し、以前のThirdPartyやBuildをコピーしません。以降は展開先を作業ディレクトリにします。

```bat
Setup.cmd
cmake -S . -B Build/Independent -A x64 -DDXF_BUILD_NATIVE=ON -DDXF_BUILD_NATIVE_SMOKE=ON -DDXF_RUN_DEVICE_TESTS=ON
cmake --build Build/Independent --config Debug --parallel 6
ctest --test-dir Build/Independent -C Debug --output-on-failure
cmake --build Build/Independent --config Release --parallel 6
ctest --test-dir Build/Independent -C Release --output-on-failure
```

各コマンドの終了コード0、両構成の全CTest成功、NativeModelDeviceSmokeの画素判定成功を必要条件とします。生成された画像とログを構成別に保存してください（同じCTest出力先を使うと画像は上書きされます）。`ThirdParty/DxLib-3.25a-source/DxLibFbx.json`で`with_fbx=false`、`model_extension_version>=3`、FBX SDKの参照が空であること、リンク入力にAutodeskライブラリがないことを確認します。

Setupでは`Tools/DxLibFbx/ModelPbr.hlsl`をfxcでコンパイルし、生成したバイトコードをライブラリへ埋め込みます。fxcを用意できないビルド機では明示的に構成失敗するのが正しい動作です。過去に生成したヘッダーやライブラリでSetupの成功を代用しないでください。

## インストールしたライブラリを外部プロジェクトから使う

両構成を同じ新規のインストール先へ配置します。

```bat
cmake --install Build/Independent --config Debug --prefix C:/FbxCheck/Installed
cmake --install Build/Independent --config Release --prefix C:/FbxCheck/Installed
```

別ディレクトリ`C:/FbxCheck/Consumer`へ`Tests/NativeModelSmoke/Main.cpp`をコピーし、次の`CMakeLists.txt`を置きます。フレームワークを`add_subdirectory`してはいけません。

```cmake
cmake_minimum_required(VERSION 3.24)
project(IndependentConsumer LANGUAGES CXX)
find_package(dxlib_framework CONFIG REQUIRED)
add_executable(IndependentConsumer Main.cpp)
target_link_libraries(IndependentConsumer PRIVATE dxf::native)
target_compile_definitions(IndependentConsumer PRIVATE DX_NON_USING_NAMESPACE_DXLIB NOMINMAX)
target_compile_options(IndependentConsumer PRIVATE /UUNICODE /U_UNICODE /utf-8)
dxf_use_static_runtime(IndependentConsumer)
```

`C:/FbxCheck/Source`は実際のソース展開ルートへ置き換えます。

```bat
cmake -S C:/FbxCheck/Consumer -B C:/FbxCheck/ConsumerBuild -A x64 -DCMAKE_PREFIX_PATH=C:/FbxCheck/Installed -DDXF_DXLIB_CUSTOM_ROOT=C:/FbxCheck/Source/ThirdParty/DxLib-3.25a-source
cmake --build C:/FbxCheck/ConsumerBuild --config Debug
C:/FbxCheck/ConsumerBuild/Debug/IndependentConsumer.exe C:/FbxCheck/Source C:/FbxCheck/Images/Debug
cmake --build C:/FbxCheck/ConsumerBuild --config Release
C:/FbxCheck/ConsumerBuild/Release/IndependentConsumer.exe C:/FbxCheck/Source C:/FbxCheck/Images/Release
```

両構成で終了コード0と`REAL_SDK_MODEL_SMOKE_PASSED failures=0`を要求します。通常の`Tools/ValidatePackage.py`の7段階も実行できますが、これは**Native無効のDebug配布・再配置・外部利用**の試験です。実DxLibモデル描画やSDK未導入環境の代わりにはなりません。

## 実行時コンパイラー不要を別に確認する

実行機へReleaseの実行ファイル、`Assets/Models`、`Tests/Assets`だけをディレクトリ構成を保ってコピーします。ThirdParty、Build、Tools、生成ヘッダー、fxc、コンパイラーDLLを同梱しません。コピー先をProjectRoot引数として実行し、上記と同じ画素判定と終了コードを確認します。

PBR実装は`LoadPixelShaderFromMem`へ組み込みバイトコードを渡します。Setup時のコンパイル成功と、コンパイラーのない実行機での成功は独立した結果として残してください。開発PC上でPATHを最小化して実行する試験は配置依存の検査にはなりますが、実行機にコンパイラーが存在しないことの証明ではありません。

新しい結果は日時・環境・コマンド・終了コード・ログ・画像・配布ハッシュを付けた別のDevelopment記録に残します。過去のValidationを更新済みの結果へ書き換えません。
