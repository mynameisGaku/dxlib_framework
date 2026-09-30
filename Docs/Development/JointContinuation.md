# Jointの継続点

J1〜J4の距離拘束基盤に、J5〜J7のBody参照・ゲーム用Component・2D／3D仕掛け・外部利用・測定・故障回帰を接続しています。現行仕様は[距離Joint](../Physics/Joints.md)、今回の試行と未実施範囲は[2026-10-01の記録](JointGameplayCompletion-2026-10-01.md)。

次の種類（Hinge／Revolute、Fixed、Prismatic）は未実装です。今回の依頼完了後に自動では着手しません。CCD中の連続長保証・未処理時間の独立進行・Body全体の巻き戻し保証は追加していません。NativeModelのProcessMessage=-1原因、SDK未導入PC、人のF5／物理入力、TSan、全D3D／COMリーク検査は別の残課題です。
