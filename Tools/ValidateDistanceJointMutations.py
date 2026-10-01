from pathlib import Path
import subprocess,hashlib,json,time,re,sys
from datetime import datetime,timezone
sys.path.insert(0,str(Path(__file__).resolve().parent))
from ValidationSupport import source_manifest
root=Path(__file__).resolve().parents[1]
import argparse
parser=argparse.ArgumentParser(description='既存Distance M01〜14の挙動検出と生バイト復元。他のbuildと同時実行しない。')
parser.add_argument('--build',type=Path,required=True)
parser.add_argument('--logs',type=Path,required=True)
parser.add_argument('ids',nargs='*',default=[f'M{n:02d}' for n in range(1,15)])
args_cli=parser.parse_args()
if any(n not in [f'M{i:02d}' for i in range(1,15)] for n in args_cli.ids):parser.error('ids must be M01..M14')
build=args_cli.build.resolve();out=args_cli.logs.resolve();out.mkdir(parents=True,exist_ok=False)
worlds=[root/f'Source/Physics/Private/Dxf/PhysicsWorld{d}.cpp' for d in ('2D','3D')];overlay=root/'Examples/GameplaySample/JointCourseOverlay.cpp'
def one(s,a,b):
 assert s.count(a)==1,(a,s.count(a));return s.replace(a,b)
def mutate(p,s,name):
 dim='2D' if '2D' in p.name else '3D'
 if name=='M11':
  if p!=overlay:return s
  a='void DrawJointCourse3D(FRenderContext& Render, const DInteraction3DScene& Scene)\n{'
  return one(s,a,a+'\n\tconst_cast<FPhysicsWorld3D&>(Scene.GetPhysicsWorld()).Step(1.0 / 60.0);')
 if p==overlay:return s
 if name=='M01':
  if dim=='2D':return s
  return one(one(s,'Out.ArmA = Rotate_Internal(QuaternionA, LocalA);','Out.ArmA = LocalA;'),'Out.ArmB = Rotate_Internal(QuaternionB, LocalB);','Out.ArmB = LocalB;')
 if name=='M02':return one(s,'const Toolbox::f64 Lambda = CDot / Mass;','const Toolbox::f64 Lambda = -CDot / Mass;')
 if name=='M03':
  a=s.index('\tvoid WarmStartJoints_Internal()');b=s.index('\t// 単一Distance Jointの位置' if dim=='3D' else '\t// Anchor点の相対速度',a)
  body=s[a:b]
  matches=list(re.finditer(r'ApplyImpulse_Internal\(\*BodyA, \*BodyB, Frame\.PositionA,[\s\S]*?\);',body));assert len(matches)==2,len(matches)
  for m in reversed(matches):
   body=body[:m.start()]+'const auto SavedAngularA = BodyA->AngularVelocity;\n\t\tconst auto SavedAngularB = BodyB->AngularVelocity;\n\t\t'+m.group()+'\n\t\tBodyA->AngularVelocity = SavedAngularA;\n\t\tBodyB->AngularVelocity = SavedAngularB;'+body[m.end():]
  return s[:a]+body+s[b:]
 if name=='M04':
  if dim=='3D':return one(one(s,'Out.ArmA = Rotate_Internal(QuaternionA, LocalA);','Out.ArmA = Rotate_Internal(QuaternionA, LocalB);'),'Out.ArmB = Rotate_Internal(QuaternionB, LocalB);','Out.ArmB = Rotate_Internal(QuaternionB, LocalA);')
  a=s.index('\tbool BuildJointFrame_Internal');b=s.index('\tstatic void UpdateLastAxis_Internal',a)
  return s[:a]+s[a:b].replace('Joint.LocalAnchorA','Joint.MutationSwap').replace('Joint.LocalAnchorB','Joint.LocalAnchorA').replace('Joint.MutationSwap','Joint.LocalAnchorB')+s[b:]
 if name=='M05':
  a=s.index('RegisterJoint_Internal(');b=s.index('\n\t}',a)+3;t=s[a:b]
  t=one(t,'Slot = Record;','const auto StaleImpulse = Slot.AccumulatedImpulse;\nSlot = Record;\nSlot.AccumulatedImpulse = StaleImpulse;')
  return s[:a]+t+s[b:]
 if name=='M06':
  a=s.index(f'bool FPhysicsWorld{dim}::DestroyBody');b=s.index(f'FJointId{dim} FPhysicsWorld{dim}::CreateDistanceJoint',a);t=s[a:b]
  t=one(t,'if (!Joint.bAlive || (Joint.BodyA != Id && Joint.BodyB != Id))','if (true)')
  return s[:a]+t+s[b:]
 if name in ('M07','M08'):
  a=s.index('// Joint辺。');b=s.index('PhysicsPrivate::FIslandManager::Build',a);t=s[a:b]
  for end in ('A','B'):
   t=one(t,f'Edge.bDynamic{end} = Body{end}->Type == EBodyType::Dynamic;',f'Edge.bDynamic{end} = '+('false' if name=='M07' else 'true')+';')
  return s[:a]+t+s[b:]
 if name=='M09':
  s=one(s,'m_pImpl->WakeIslands_Internal(Manifolds, Islands, Slice);','(void)Slice;')
  return one(s,'m_pImpl->PropagateAwakeDynamics_Internal(Islands);','(void)Islands;')
 if name=='M10':return one(s,'SolveDistanceJoint_Internal(Constraint.Index);','if (Execution.JobSystem == nullptr) { SolveDistanceJoint_Internal(Constraint.Index); }')
 if name=='M12':
  a=s.index('RegisterJoint_Internal(');b=s.index('\n\t}',a)+3;t=s[a:b]
  t=one(t,'Joints.PushBack(Record);','Joints.PushBack(Record);\nToolbox::TVector<Toolbox::int32> LateAllocation;\nLateAllocation.Resize(1);')
  return s[:a]+t+s[b:]
 if name=='M13':
  a=s.index('\tvoid ConsiderSolverPair_Internal');b=s.index('\tvoid GenerateManifolds_Internal',a);t=s[a:b]
  needle='\t\t++ExecutionDiagnostics.CandidatePairCount;'
  t=one(t,needle,'''\t\tfor (const auto& Joint : Joints)
		{
			if (Joint.bAlive && ((Joint.BodyA == RecordA.Body && Joint.BodyB == RecordB.Body) || (Joint.BodyB == RecordA.Body && Joint.BodyA == RecordB.Body))) { return; }
		}
'''+needle)
  return s[:a]+t+s[b:]
 if name=='M14':
  a=s.index(f'bool FPhysicsWorld{dim}::DetachCollider');b=s.index(f'void FPhysicsWorld{dim}::SetColliderShape',a);t=s[a:b]
  t=one(t,'\tRecord->bAlive = false;',f'''\tfor (Toolbox::size_t Slot = 0; Slot < m_pImpl->Joints.Size(); ++Slot)
	{{
		const auto& Joint = m_pImpl->Joints[Slot];
		if (Joint.bAlive && (Joint.BodyA == Id.Body || Joint.BodyB == Id.Body)) {{ DestroyJoint({{m_pImpl->World, Slot, Joint.Generation}}); }}
	}}
\tRecord->bAlive = false;''')
  return s[:a]+t+s[b:]
 raise ValueError(name)
def run(cmd,name):
 t=time.monotonic();started=datetime.now(timezone.utc).isoformat()
 binaries={str(exe.relative_to(root)):hashlib.sha256(exe.read_bytes()).hexdigest()} if exe.exists() else {}
 fingerprint=source_manifest(root)[0]
 with (out/(name+'.log')).open('w',encoding='utf-8') as f:r=subprocess.run(cmd,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300)
 return dict(exit=r.returncode,seconds=round(time.monotonic()-t,3),log=name+'.log',command=cmd,started_utc=started,finished_utc=datetime.now(timezone.utc).isoformat(),source_sha256=fingerprint,executable_sha256=binaries,native=False,config='Release',sdk=None)
for name in args_cli.ids:
 files=[overlay] if name=='M11' else worlds
 saved={p:p.read_bytes() for p in files}
 backup=out/(name+'-backup');backup.mkdir()
 for p,b in saved.items():
  destination=backup/p.relative_to(root);destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(b)
 result=dict(id=name,source_hash={str(p.relative_to(root)):hashlib.sha256(b).hexdigest() for p,b in saved.items()})
 target='dxf_interaction_sample_tests' if name=='M11' else 'dxf_joint_component_fault' if name=='M12' else 'dxf_physics_tests'
 exe=build/'Release'/f'{target}.exe';args=[str(exe)]+([] if name in ('M11','M12') else ['--joint-all'])
 cmd=['cmake','--build',str(build),'--config','Release','--target',target,'--parallel','4']
 try:
  changed=0
  for p,b in saved.items():
   s=b.decode('utf-8').replace('\r\n','\n');new=mutate(p,s,name)
   if new!=s:changed+=1;p.write_bytes(new.replace('\n','\r\n').encode('utf-8'))
  assert changed,name
  result['mutant_hash']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
  result['build']=run(cmd,name+'-build')
  if result['build']['exit']==0:result['red']=run(args,name+'-red')
 finally:
  for p,b in saved.items():
   p.write_bytes(b)
   # 保存時刻を復元せず、新しい更新時刻でMSBuildへ再コンパイルを要求する。
   assert p.read_bytes()==b
  result['restored_mtime_ns']={str(p.relative_to(root)):p.stat().st_mtime_ns for p in files}
  result['restored_hash']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
  assert result['restored_hash']==result['source_hash']
  result['restore_build']=run(cmd,name+'-restore-build')
  if result['restore_build']['exit']==0:result['green']=run(args,name+'-green')
  (out/(name+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
 print(name,'build',result['build']['exit'],'Red',result.get('red',{}).get('exit'),'restore',result['restore_build']['exit'],'Green',result.get('green',{}).get('exit'),flush=True)
 if result.get('build',{}).get('exit')!=0 or result.get('red',{}).get('exit')!=1 or result.get('green',{}).get('exit')!=0:raise SystemExit('Mutation failed or survived: '+name)
