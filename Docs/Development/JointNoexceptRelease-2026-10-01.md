# Joint破棄時の追加確保（2026-10-01）

J5〜J7作業の確保失敗注入で、CreateDistanceJointの成功後に旧Jointを破棄する境界を調べた。
2D/3DのDestroyJointはnoexceptだが、JointFree.PushBackが領域を確保していた。
注入試行3/4の2D countdown=4はこの破棄中の失敗へ進み異常停止した（中断、成功扱いにしない）。
新登録を公開する前に、全Joint slotの空き番号を保持できる領域を倍増方式で確保する。
以後のDestroyJointと、Body破棄によるJoint失効でJointFreeの確保は発生しない。
Body全体やColliderのnoexcept解放契約まで修復したという意味ではない。

追加回帰は両次元各33登録を、次の確保を必ず失敗させる設定で全て破棄する。
注入未到達・累計確保数不変・同slotの新世代を確認する。
Release PhysicsContinuation/PhysicsOverlapFault/JointComponentFaultは3/3、終了0（19.27秒）。
Debug PhysicsOverlapFault/JointComponentFaultは2/2、終了0。
Component注入では両次元各4地点で旧接続保持と回復後の新接続を確認した。
これはJ5〜J7の中間検証であり、最終全群と配布4構成の完了記録ではない。

生ログ：Build/JointGameplayCompletion-20261001/。生成物はcommitしない。
