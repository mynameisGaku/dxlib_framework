# dxlib_framework：2D/3D描画入口とデバッグ図形の実装

対象main: `1bb654826cb6b23de4837d744ab2634b25fb9979`。
GitHubへの書き込みはしていません。これはソース・移行器・テスト・検証記録です。

## 適用

ZIPはリポジトリ外（Downloads等）へ展開してください。個々のChangedFilesを上書きしないでください。

リポジトリのPowerShellから:

```powershell
# まず検査のみ。失敗した場合は次へ進まない。
python C:\展開先\apply_render_views.py --root .
if ($LASTEXITCODE -ne 0) { throw "移行確認に失敗" }

# 検査済み計画を適用。変更前ファイルはリポジトリの隣に退避。
python C:\展開先\apply_render_views.py --root . --apply
if ($LASTEXITCODE -ne 0) { throw "移行に失敗" }

git diff --check
.\GenerateProjectFiles.bat -Development
if ($LASTEXITCODE -ne 0) { throw "生成に失敗" }
.\Build.cmd -Clean
```

適用器はcleanな起点を要求します。別コミットやユーザーの編集へ無理に上書きしません。未コミット作業は先に保存してください。force push、reset、clean、stashは行いません。commit/pushも自動で行いません。

## 入口

```cpp
auto Result = Render.Get2D().DrawSprite(Texture, Position);
auto Text = Render.Get2D().DrawText(Font, "Hello", Position);
auto Line = Render.Get3D().DrawLine({0, 0, 0}, {1, 0, 0});
```

旧Render.Draw/DrawText/FillRectangle/SubmitGeneratedは削除しています。管理者名もFRenderSystemに改名しました。Applicationの共有JobSystemを自動接続するため、通常の並列生成は `Render.Get2D().SubmitGenerated(Count, Generate)` です。

## 対応範囲

2D: 画像・文字・矩形・線・円・三角形。
3D: 線・三角形・箱・球・CPUの三角形／線メッシュ。
表示: 通常塗り、形状のワイヤー、塗り＋辺。照明は単一方向光＋環境色のCPUフラット計算、Unlit、LightsOffを区別。
デバッグ: カテゴリ、選択ID、Scope、ゲーム／実時間の寿命、件数制限、描画アダプター。

**MV1モデル、アニメーション、任意の複数ライト・影、GPUワイヤー、法線／深度／オーバードローの専用モード、物理Snapshotの自動採取、デバッグUIは未実装です。** 名前だけ対応済みのAPIは追加していません。

詳しい契約と使用例は `ChangedFiles/Docs/Rendering/ViewsAndDebug.md`。

## 検証の境界

実際の描画・Job・Native変換cppをコンパイルし、GCC/ClangとSanitizerで検証しています。ただし、全リポジトリのApplication/Gameplay/Asset/Physicsの回帰、Windows/MSVC、実DxLib SDK、実画面は未実施です。

適用器の構文・変換・退避は別のテストで確認しています。この成功を全リポジトリの適用後ビルド成功と扱っていません。検証後のmain反映は、実際の開発環境で行ってください。

既にinstallしたincludeディレクトリには旧RenderSystem2D.hが残り得ます。外部利用の検証は新しい空のinstall prefixで行ってください。CMakeのinstallは、利用者の既存ヘッダーを削除する処理には変更していません。
