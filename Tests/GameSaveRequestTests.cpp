// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/GameSaveRequest.h"
#include "Toolbox/FileError.h"
#include "Toolbox/FileLock.h"
#include "Toolbox/JsonWriter.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 試験ごとに新規Rootを確定し、以前の保存を共有しない。
FPath Root()
{
	return FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("request-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
}
// 所有側の採取が完成したあとの不変値。
FSaveDocument Document()
{
	FSaveDocument Value;
	Value.GameId = "request_game";
	Value.ContentRevision = "trusted";
	Value.SceneKey = "main";
	Value.CheckpointKey = "safe";
	Value.Payload = "{\"score\":42}";
	return Value;
}
// 同期と非同期で公開境界・終端を同じ期待値へ照合する。
void Exercise(bool Async)
{
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FSaveStore Store(Root());
	FGameSaveRequest Request(Store, Async ? &Tasks : nullptr);
	const auto Saved = Request.RequestSave("manual", Document());
	if (Async)
	{
		Tasks.WaitForPrepares();
		Tasks.PumpCommits();
	}
	REQUIRE(Saved.GetState() == EGameSaveRequestState::Prepared);
	// Task Commitが動いただけでは面は公開されない。Cancel後に排他も解放する。
	REQUIRE(Request.Cancel());
	Request.Poll();
	REQUIRE(Saved.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
	const auto Normal = Request.RequestSave("manual", Document());
	if (Async)
	{
		Tasks.WaitForPrepares();
		Tasks.PumpCommits();
	}
	Request.Poll();
	REQUIRE(Normal.GetState() == EGameSaveRequestState::Committed);
	REQUIRE(!Request.Cancel());
	REQUIRE(Store.Load("manual").Document->Generation == 1);
	const auto Loaded = Request.RequestLoad("manual", Document());
	if (Async)
	{
		Tasks.WaitForPrepares();
		Tasks.PumpCommits();
	}
	REQUIRE(Loaded.GetState() == EGameSaveRequestState::Prepared);
	REQUIRE(Request.GetAcceptedSequence() == 0);
	Request.Poll();
	REQUIRE(Request.GetAcceptedSequence() == Loaded.GetSequence());
	REQUIRE(Loaded.GetState() == EGameSaveRequestState::Prepared);
	REQUIRE(Request.GetLoaded().Document->Payload == "{\"score\":42}");
	// 再開完了はゲームadapterの別確認。値読込だけではCompletedにしない。
	Request.MarkTransitionRequested(Loaded.GetSequence());
	REQUIRE(Loaded.GetState() == EGameSaveRequestState::TransitionRequested);
	REQUIRE(!Request.Cancel());
	Request.MarkResumeCompleted(Loaded.GetSequence());
	REQUIRE(Loaded.GetState() == EGameSaveRequestState::Completed);
	FSaveDocument Wrong = Document();
	Wrong.ContentRevision = "other";
	const auto Failed = Request.RequestLoad("manual", Wrong);
	if (Async)
	{
		Tasks.WaitForPrepares();
		Tasks.PumpCommits();
	}
	Request.Poll();
	REQUIRE(Failed.GetState() == EGameSaveRequestState::Failed);
	REQUIRE(!Failed.GetReason().IsEmpty());
	REQUIRE(Request.GetLoaded().Document->ContentRevision == "trusted");
	REQUIRE(Request.GetAcceptedSequence() == Loaded.GetSequence());
}
} // namespace
TEST("Save request synchronous ownership and publication boundaries")
{
	Exercise(false);
}
TEST("Save request asynchronous ownership and publication boundaries")
{
	Exercise(true);
}
TEST("Save request newest load supersedes prior delivered value")
{
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FSaveStore Store(Root());
	Store.Save("A", Document());
	FSaveDocument Other = Document();
	Other.Payload = "{\"score\":99}";
	Store.Save("B", Other);
	FGameSaveRequest Request(Store, &Tasks);
	const auto A = Request.RequestLoad("A", Document());
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
	const auto B = Request.RequestLoad("B", Document());
	REQUIRE(A.GetState() == EGameSaveRequestState::Superseded);
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
	Request.Poll();
	REQUIRE(Request.GetAcceptedSequence() == B.GetSequence());
	REQUIRE(Request.GetLoaded().Document->Payload == "{\"score\":99}");
	REQUIRE(A.GetState() == EGameSaveRequestState::Superseded);
}
TEST("Save request cancellation before worker pickup is terminal")
{
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FSaveStore Store(Root());
	FGameSaveRequest Request(Store, &Tasks);
	const auto Ticket = Request.RequestSave("manual", Document());
	REQUIRE(Request.Cancel());
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
	Request.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
}
TEST("Save request rejects competing manual save without canceling first")
{
	FSaveStore Store(Root());
	FGameSaveRequest Request(Store);
	const auto A = Request.RequestSave("manual", Document());
	bool Busy = false;
	try
	{
		Request.RequestSave("manual", Document());
	}
	catch (const FFileError& Error)
	{
		REQUIRE(Error.GetOperation() == EFileOperation::Busy);
		Busy = true;
	}
	REQUIRE(Busy);
	REQUIRE(A.GetState() == EGameSaveRequestState::Prepared);
	Request.Poll();
	REQUIRE(A.GetState() == EGameSaveRequestState::Committed);
}
TEST("Save request Scope cancellation cannot publish prepared file")
{
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	const auto Parent = Tasks.CreateScope();
	FSaveStore Store(Root());
	FGameSaveRequest Request(Store, &Tasks, Parent);
	const auto Ticket = Request.RequestSave("manual", Document());
	Tasks.WaitForPrepares();
	Tasks.PumpCommits();
	Tasks.Cancel(Parent);
	Request.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
}
TEST("Save request destruction terminates ticket and releases prepared lock")
{
	FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FSaveStore Store(Root());
	FGameSaveTicket Ticket;
	{
		FGameSaveRequest Request(Store, &Tasks);
		Ticket = Request.RequestSave("manual", Document());
		Tasks.WaitForPrepares();
		Tasks.PumpCommits();
	}
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Canceled);
	REQUIRE(Store.Save("manual", Document()) == 1);
}
TEST("Save request rejected dispatcher is Failed with diagnostic")
{
	FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs, {0});
	FSaveStore Store(Root());
	FGameSaveRequest Request(Store, &Tasks);
	const auto Ticket = Request.RequestSave("manual", Document());
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Failed);
	REQUIRE(!Ticket.GetReason().IsEmpty());
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
}
#if defined(_WIN32)
TEST("Save request unknown publication waits for explicit retry instead of every Poll")
{
	FSaveStore Store(Root());
	Store.Save("manual", Document());
	FGameSaveRequest Request(Store);
	const auto Ticket = Request.RequestSave("manual", Document());
	{
		// 公開先だけをこの試験の排他で読戻し不能にする。
		FFileLock Destination(Store.GetRoot() / FPath("manual.b"));
		Request.Poll();
		REQUIRE(Ticket.GetState() == EGameSaveRequestState::PublicationUnknown);
		REQUIRE(!Request.Cancel());
	}
	Request.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::PublicationUnknown);
	REQUIRE(Request.RetryPublication());
	Request.Poll();
	REQUIRE(Ticket.GetState() == EGameSaveRequestState::Committed);
	REQUIRE(!Request.RetryPublication());
	REQUIRE(Store.Load("manual").Document->Generation == 2);
}
#endif
