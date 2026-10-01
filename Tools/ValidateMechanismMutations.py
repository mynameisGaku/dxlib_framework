from pathlib import Path
import subprocess,hashlib,json,time,sys
from datetime import datetime,timezone
sys.path.insert(0,str(Path(__file__).resolve().parent))
from ValidationSupport import source_manifest
root=Path(__file__).resolve().parents[1]
import argparse
parser=argparse.ArgumentParser(description='生バイト保存・変異build・挙動失敗・復元build・正常回帰を直列で検証する。ほかのbuildと同時実行しない。')
parser.add_argument('--build',type=Path,required=True)
parser.add_argument('--logs',type=Path,required=True)
parser.add_argument('ids',nargs='*',type=int,default=list(range(1,25)))
args_cli=parser.parse_args()
if any(n not in range(1,25) for n in args_cli.ids):parser.error('ids must be 1..24')
build=args_cli.build.resolve();out=args_cli.logs.resolve();out.mkdir(parents=True,exist_ok=False)
math=root/'Source/Physics/Private/Dxf/MechanismConstraintMath.cpp'
header=math.with_suffix('.h')
worlds=[root/f'Source/Physics/Private/Dxf/PhysicsWorld{d}.cpp' for d in ('2D','3D')]
inls=[root/f'Source/Physics/Private/Dxf/MechanismWorld{d}.inl' for d in ('2D','3D')]
components=[root/f'Source/Gameplay/Private/Dxf/{kind}JointComponent{d}.cpp' for kind in ('Revolute','Fixed','Prismatic') for d in ('2D','3D')]
overlay=root/'Examples/GameplaySample/MechanismOverlay.cpp'
def one(s,a,b):
 assert s.count(a)==1,(a,s.count(a));return s.replace(a,b)
def files_for(n):
 if n==1:return [header]
 if n in (2,3,4,5,6,7,8,9,10,11):return [math]
 if n==13:return inls+[math]
 if n in (14,21,24):return components
 if n in (16,19,20):return inls
 if n==23:return [overlay]
 return worlds
def mutate(p,s,n):
 if n==1:
  return one(s,'return Add(V, Add(Scale(T, Q.W), Cross(U, T)));','const Toolbox::f64 R00 = 1 - 2 * (Q.Y * Q.Y + Q.Z * Q.Z); const Toolbox::f64 R01 = 2 * (Q.X * Q.Y - Q.W * Q.Z); const Toolbox::f64 R02 = 2 * (Q.X * Q.Z + Q.W * Q.Y); const Toolbox::f64 R10 = 2 * (Q.X * Q.Y + Q.W * Q.Z); const Toolbox::f64 R11 = 1 - 2 * (Q.X * Q.X + Q.Z * Q.Z); const Toolbox::f64 R12 = 2 * (Q.Y * Q.Z - Q.W * Q.X); const Toolbox::f64 R20 = 2 * (Q.X * Q.Z - Q.W * Q.Y); const Toolbox::f64 R21 = 2 * (Q.Y * Q.Z + Q.W * Q.X); const Toolbox::f64 R22 = 1 - 2 * (Q.X * Q.X + Q.Y * Q.Y); return {R00 * R00 * V.X + R01 * R01 * V.Y + R02 * R02 * V.Z, R10 * R10 * V.X + R11 * R11 * V.Y + R12 * R12 * V.Z, R20 * R20 * V.X + R21 * R21 * V.Y + R22 * R22 * V.Z};')
 if n==2:return one(s,'Free = AngularRow(Gradient, Out.Coordinate);','Free = AngularRow(Gradient, Out.Coordinate);\nOut.Values[5] = Free;')
 if n==3:
  return one(one(s,'Out.Values[3] = AngularRow(Rotate(QA, GX), Dot(X, Swing));','Out.Values[3] = {};'),'Out.Values[4] = AngularRow(Rotate(QA, GY), Dot(Y, Swing));','Out.Values[4] = {};')
 if n in (4,7):
  kind='Fixed' if n==4 else 'Prismatic'
  return one(s,'if (Kind == EJointKind::Fixed)\n\t{\n\t\treturn Out;',f'if (Kind == EJointKind::{kind}) {{ Out.Values[3] = {{}}; Out.Values[4] = {{}}; Out.Values[5] = {{}}; }}\nif (Kind == EJointKind::Fixed)\n\t{{\n\t\treturn Out;')
 if n==5:return one(s,'const FMechanismVector Error = RotationError(QA, QB);','const FMechanismVector Error = Kind == EJointKind::Fixed ? FMechanismVector{} : RotationError(QA, QB);')
 if n==6:return one(s,'Free = LinearRow(X, RA, RB, D, true);','Out.Values[0] = {}; Out.Values[1] = {};\nFree = LinearRow(X, RA, RB, D, true);')
 if n==8:return one(s,'R.AngularA = Add(R.AngularA, Cross(N, D));','R.AngularA = R.AngularA;')
 if n==9:return one(s,'R.Lower = bUpper ? -1e100 : 0;','R.Lower = bUpper ? 0 : -1e100;').replace('R.Upper = bUpper ? 0 : 1e100;','R.Upper = bUpper ? 1e100 : 0;')
 if n==10:
  s=one(s,'R.Lower = bUpper ? -1e100 : 0;','R.Lower = -1e100;').replace('R.Upper = bUpper ? 0 : 1e100;','R.Upper = 1e100;')
  return s.replace('Out.Coordinate <= S.Lower','Out.Coordinate <= S.Lower + 1e-5').replace('Out.Coordinate >= S.Upper','Out.Coordinate >= S.Upper - 1e-5')
 if n==11:return one(s,'Accumulated = Toolbox::Clamp(Old - (Rate(R, A, B) - R.Target + R.Bias) / K, R.Lower, R.Upper);','Accumulated = Old + Toolbox::Clamp(-(Rate(R, A, B) - R.Target + R.Bias) / K, R.Lower, R.Upper);')
 if n==12:return one(s,'m_pImpl->JointSlice = Slice;','m_pImpl->JointSlice = DeltaSeconds;')
 if n==13:
  if p==math:
   return one(s,'Accumulated = 0;\n\t\treturn;','if (R.Lower == 0 && R.Upper == 0) { ApplyRow(R, A, B, Accumulated, false); }\nreturn;')
  s=one(s,'Record.MechanismCache.Impulses[6] = 0;','(void)Record.MechanismCache.Impulses[6];')
  return one(s,'Cache.Impulses[Index] = R.bUsed ? Toolbox::Clamp(Cache.Impulses[Index], R.Lower, R.Upper) : 0;','if (Index != 6 || R.bUsed) { Cache.Impulses[Index] = R.bUsed ? Toolbox::Clamp(Cache.Impulses[Index], R.Lower, R.Upper) : 0; }')
 if n==14:return one(s,'if (IsConnected_Internal() && Same_Internal(m_Description, m_Requested))','if (false && IsConnected_Internal() && Same_Internal(m_Description, m_Requested))')
 if n==15:
  start=s.index('RegisterJoint_Internal(');end=s.index('\n\t}',start)+3
  a=s[start:end];a=one(a,'Slot = Record;','const auto Stale = Slot.MechanismCache;\nSlot = Record;\nSlot.MechanismCache = Stale;')
  return s[:start]+a+s[end:]
 if n==16:
  assert s.count('auto& Cache = JointSolveStates[Slot].Mechanism;')==2
  return s.replace('auto& Cache = JointSolveStates[Slot].Mechanism;','auto& Cache = Joints[Slot].MechanismCache;')
 if n==17:return one(s,'SolveMechanism_Internal(Constraint.Index);','if (Execution.JobSystem == nullptr) { SolveMechanism_Internal(Constraint.Index); }')
 if n==18:return one(s,'BuildIslands_Internal(Manifolds, Islands, false);','for (Toolbox::size_t Slot = 0; Slot < Joints.Size(); ++Slot) { if (Joints[Slot].bAlive && Joints[Slot].Kind != EJointKind::Distance) { WarmMechanism_Internal(Slot); } }\nBuildIslands_Internal(Manifolds, Islands, true);')
 if n==19:return one(s,'return Joint.Mechanism.bMotor && Joint.Mechanism.Maximum > 0 && Toolbox::Abs(Rows.Rate - Joint.Mechanism.Target) > SpeedTolerance;','return Joint.Mechanism.bMotor;')
 if n==20:
  s=one(s,'return Joint.Mechanism.bMotor && Joint.Mechanism.Maximum > 0 && Toolbox::Abs(Rows.Rate - Joint.Mechanism.Target) > SpeedTolerance;','return false;')
  return s.replace('Wake_Internal(A);','(void)A;').replace('Wake_Internal(B);','(void)B;')
 if n==21:
  token='const auto NewJoint = m_pWorld->Create';assert s.count(token)==1
  return s.replace(token,'Release_Internal();\n'+token)
 if n==22:
  return one(s,'if (!Joint.bAlive || (Joint.BodyA != Id && Joint.BodyB != Id))','if (!Joint.bAlive || Joint.Kind != EJointKind::Distance || (Joint.BodyA != Id && Joint.BodyB != Id))')
 if n==23:
  for d in ('2D','3D'):
   tok='const auto& Course = Scene.GetMechanismCourse();'
  assert s.count(tok)==2
  return s.replace(tok,tok+'\nconst_cast<decltype(Scene.GetMechanismCourse())>(Scene.GetMechanismCourse());',0) if False else s.replace(tok,'auto& Course = const_cast<'+ 'PLACEHOLDER' +'&>(Scene.GetMechanismCourse());\nCourse.FixedUpdate(1.0 / 60.0);',1).replace('PLACEHOLDER','FMechanismCourse2D').replace(tok,'auto& Course = const_cast<FMechanismCourse3D&>(Scene.GetMechanismCourse());\nCourse.FixedUpdate(1.0 / 60.0);',1)
 if n==24:
  tok='m_Observation.Reset();\n\tContext.PrePhysicsStep->Enqueue(*this);'
  return one(s,tok,'if (IsConnected_Internal()) { OnPostPhysicsStep_Internal(Context); }\n\tContext.PrePhysicsStep->Enqueue(*this);')
 raise ValueError(n)
def execute(cmd,name,exe=None):
 start=time.monotonic();r={'command':cmd,'cwd':str(root),'configuration':'Release','started_utc':datetime.now(timezone.utc).isoformat(),'source_sha256':source_manifest(root)[0]}
 if exe is not None and exe.exists():r['exe_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
 with (out/(name+'.log')).open('w',encoding='utf-8') as f:
  try:r['exit']=subprocess.run(cmd,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
  except subprocess.TimeoutExpired:r['exit']=124
 r.update(seconds=round(time.monotonic()-start,3),finished_utc=datetime.now(timezone.utc).isoformat())
 (out/(name+'-result.json')).write_text(json.dumps(r,indent=2),encoding='utf-8');return r
for raw in args_cli.ids:
 n=int(raw);name=f'K-M{n:02d}';files=files_for(n);saved={p:p.read_bytes() for p in files}
 backup=out/(name+'-backup');backup.mkdir()
 for p,b in saved.items():
  destination=backup/p.relative_to(root);destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(b)
 target='dxf_tests' if n in (14,21,24) else 'dxf_physics_tests_overlap_fault' if n==16 else 'dxf_interaction_sample_tests' if n==23 else 'dxf_physics_tests'
 exe=build/'Release'/f'{target}.exe';args=[str(exe)]+(['--mechanisms'] if target=='dxf_physics_tests' else [])
 cmd=['cmake','--build',str(build),'--config','Release','--target',target,'--parallel','4']
 result={'id':name,'baseline_hashes':{str(p.relative_to(root)):hashlib.sha256(b).hexdigest() for p,b in saved.items()}}
 try:
  for p,b in saved.items():
   text=mutate(p,b.decode('utf-8-sig').replace('\r\n','\n'),n)
   assert text!=b.decode('utf-8-sig').replace('\r\n','\n'),(name,p)
   p.write_bytes((b'\xef\xbb\xbf' if b.startswith(b'\xef\xbb\xbf') else b'')+text.replace('\n','\r\n').encode('utf-8'))
  result['build']=execute(cmd,name+'-build')
  if result['build']['exit']==0:result['red']=execute(args,name+'-red',exe)
 finally:
  for p,b in saved.items():
   p.write_bytes(b)
   # 保存時刻を復元せず、新しい更新時刻でMSBuildへ再コンパイルを要求する。
   assert p.read_bytes()==b
  result['restored_mtime_ns']={str(p.relative_to(root)):p.stat().st_mtime_ns for p in files}
  result['restored_hashes']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
  assert result['restored_hashes']==result['baseline_hashes']
  result['restore_build']=execute(cmd,name+'-restore-build')
  if result['restore_build']['exit']==0:result['green']=execute(args,name+'-green',exe)
  (out/(name+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
 print(name,'build',result.get('build',{}).get('exit'),'red',result.get('red',{}).get('exit'),'restore',result['restore_build']['exit'],'green',result.get('green',{}).get('exit'),flush=True)
 if result.get('build',{}).get('exit')!=0 or result.get('red',{}).get('exit')!=1 or result.get('green',{}).get('exit')!=0:raise SystemExit('Mutation failed or survived '+name)
