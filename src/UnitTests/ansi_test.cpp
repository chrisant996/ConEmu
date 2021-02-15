
/*
Copyright (c) 2009-present Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define HIDE_USE_EXCEPTION_INFO
#define SHOWDEBUGSTR

#include "../common/defines.h"
#include "gtest.h"
#include "../common/WConsole.h"
#include "../common/MHandle.h"
#include "../common/WObjects.h"

namespace {
void SetVirtualTermProcessing(const bool enable)
{
	/* Set xterm mode for output */
	const MHandle h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	ASSERT_TRUE(GetConsoleMode(h, &dwMode));
	if (enable)
	{
		ASSERT_TRUE(SetConsoleMode(h, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
	}
	else
	{
		ASSERT_TRUE(SetConsoleMode(h, dwMode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING));
	}
}

void test(void)
{
}

}

TEST(Ansi, CheckXTermInChain)
{
	const MHandle hIn = GetStdHandle(STD_INPUT_HANDLE);
	const MHandle hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	auto getMode = [](const MHandle& h)
	{
		DWORD mode = 0;
		EXPECT_TRUE(GetConsoleMode(h, &mode));
		return mode;
	};

	auto isOutputRedirected = [&hOut]() {
		CONSOLE_SCREEN_BUFFER_INFO sbi = {};
		const BOOL bIsConsole = GetConsoleScreenBufferInfo(hOut, &sbi);
		return bIsConsole;
	};

	enum class TestMode { ParentXTerm, ChildXTerm };
	auto testMode = TestMode::ParentXTerm;
	
	const std::string prefix = "ConOutMode=";
	for (const auto& param : conemu::tests::gTestArgs)
	{
		if (param.substr(0, prefix.length()) != prefix)
			continue;
		cdbg() << "ConOutMode found: " << param.substr(prefix.length()) << std::endl;
		if (param.substr(prefix.length()) == "Child")
		{
			testMode = TestMode::ChildXTerm;
		}
		break;
	}

	if (testMode == TestMode::ChildXTerm)
	{
		// xterm mode for output
		const DWORD dwMode = getMode(hIn);
		ASSERT_TRUE(SetConsoleMode(hIn, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
		return;
	}

	const DWORD inModeOrig = getMode(hIn);
	const DWORD outModeOrig = getMode(hOut);

	// Set output console mode
	DWORD outMode = outModeOrig;
	outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, outMode);

	// Set input console mode
	DWORD inMode = inModeOrig;
	inMode &= ~ENABLE_LINE_INPUT;
	inMode &= ~ENABLE_ECHO_INPUT;
	inMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
	SetConsoleMode(hIn, inMode);

	// Start another process which sets xterm mode enabled
	{
		CEStr exePath;
		EXPECT_LT(0U, GetModulePathName(nullptr, exePath));
		STARTUPINFOW si = { };
		PROCESS_INFORMATION pi = { };
		si.cb = sizeof(si);
		const CEStr commandLine(L"\"", exePath, L"\" --gtest_filter=Ansi.CheckXTermInChain ConOutMode=Child");
		EXPECT_TRUE(CreateProcessW(nullptr, commandLine.data(),
			nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi));
		WaitForSingleObject(pi.hProcess, INFINITE);
		Sleep(500);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	
	{
		DWORD mode = 0;
		EXPECT_TRUE(GetConsoleMode(hIn, &mode));
		if (mode != inMode)
		{
			EXPECT_EQ(mode, inMode);
			EXPECT_TRUE(SetConsoleMode(hIn, inMode));
		}
		EXPECT_TRUE(GetConsoleMode(hOut, &mode));
		if (mode != outMode)
		{
			EXPECT_EQ(mode, outMode);
			EXPECT_TRUE(SetConsoleMode(hOut, outMode));
		}
	}

	{
		DWORD written = 0;
		EXPECT_TRUE(WriteConsoleA(hOut, "\033[?1h", 5, &written, nullptr));
	}

	{ /* Read keys */
		char buffer[32] = "";
		DWORD readCount = 0;
		DWORD i;
		EXPECT_TRUE(ReadConsoleA(hIn, buffer, sizeof(buffer), &readCount, nullptr));
		printf("%d: ", readCount);
		for (i = 0; i < readCount; i++) printf("%02x,", buffer[i]);
		printf("\n");
	}

	SetConsoleMode(hIn, inModeOrig);
	SetConsoleMode(hOut, outModeOrig);
}
