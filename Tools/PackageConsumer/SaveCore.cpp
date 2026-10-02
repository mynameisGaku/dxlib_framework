// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SaveStore.h"
#include "Dxf/SaveCodec.h"
#include "Toolbox/JsonWriter.h"
namespace
{
// 保存コアへゲームの規則を固定せず、独自の論理値を渡す。
Dxf::FSaveDocument Expected()
{
	Dxf::FSaveDocument Document;
	Document.GameId = "external_save_core";
	Document.ContentRevision = "fixture-v1";
	Document.SceneKey = "main";
	Document.CheckpointKey = "safe";
	Document.Payload = "{\"collected\":true,\"counter\":\"9007199254740993\",\"score\":42}";
	return Document;
}
// 独立した二つのプロセスが同じRootを公開APIで照合する。
Toolbox::int32 Run(const Toolbox::FString& Mode, const Toolbox::FPath& Root)
{
	Dxf::FSaveStore Store(Root);
	if (Mode == "write")
	{
		if (Store.Save("manual", Expected()) != 1)
		{
			throw Toolbox::FException("Core consumer requires empty dedicated slot");
		}
	}
	else if (Mode == "read")
	{
		const auto Loaded = Store.Load("manual");
		if (Loaded.State != Dxf::ESaveLoadState::Loaded || !Loaded.Document || Loaded.Document->Generation != 1 || Loaded.Document->Payload != Expected().Payload)
		{
			throw Toolbox::FException("Core consumer restored values differ");
		}
		Dxf::FSaveCodec::RequireCompatible(*Loaded.Document, Expected());
		const auto Payload = Toolbox::FJsonDocument::Parse(Loaded.Document->Payload);
		if (Toolbox::FJsonWriter::ParseUnsigned(Payload.Get(Payload.Find(0, "counter")).Text) != 9007199254740993ULL)
		{
			throw Toolbox::FException("Core consumer lost exact counter");
		}
	}
	else
	{
		throw Toolbox::FException("Unknown core consumer mode");
	}
	Toolbox::Out << "SAVE_CORE_CONSUMER_PASSED " << Mode << '\n';
	return 0;
}
} // namespace
#if defined(_WIN32)
// OSが渡すUnicodeのパスをToolboxへ一度だけ変換する実行入口。
int wmain(int Count, wchar_t** Arguments)
{
	try
	{
		if (Count != 3)
		{
			throw Toolbox::FException("Use write/read and absolute dedicated SaveRoot");
		}
		return Run(Toolbox::FromWide(Toolbox::FWideString(Arguments[1])), Toolbox::FPath(Arguments[2]));
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << '\n';
		return 1;
	}
}
#else
// POSIXの実行入口。保存コア以外の実装ソースは追加しない。
int main(int Count, char** Arguments)
{
	try
	{
		if (Count != 3)
		{
			throw Toolbox::FException("Use write/read and absolute dedicated SaveRoot");
		}
		return Run(Toolbox::FString(Arguments[1]), Toolbox::FPath(Arguments[2]));
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << '\n';
		return 1;
	}
}
#endif
