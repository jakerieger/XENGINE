//
// Created by Jake Rieger on 10/4/2026.
//

#include "BuildProcess.hpp"

#include <Windows.h>

namespace Xen {
    BuildProcess::~BuildProcess() {
        if (_Running && !_Finished && _Process) { ::TerminateProcess(_Process, 1); }
        if (_Reader.joinable()) _Reader.join();
        if (_Process) ::CloseHandle(_Process);
    }

    bool BuildProcess::Start(const std::wstring& CommandLine, const fs::path& WorkingDir, std::string& Error) {
        if (_Running) {
            Error = "a build is already running";
            return false;
        }
        if (_Reader.joinable()) _Reader.join();  // a previous, finished run
        if (_Process) {
            ::CloseHandle(_Process);
            _Process = nullptr;
        }

        SECURITY_ATTRIBUTES Security {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE ReadPipe  = nullptr;
        HANDLE WritePipe = nullptr;
        if (!::CreatePipe(&ReadPipe, &WritePipe, &Security, 0)) {
            Error = "failed to create output pipe";
            return false;
        }
        // The read end stays ours - the child must not inherit it, or the pipe never reports EOF.
        ::SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW Startup {};
        Startup.cb         = sizeof(Startup);
        Startup.dwFlags    = STARTF_USESTDHANDLES;
        Startup.hStdOutput = WritePipe;
        Startup.hStdError  = WritePipe;
        Startup.hStdInput  = nullptr;

        PROCESS_INFORMATION Info {};
        std::wstring Mutable = CommandLine;  // CreateProcessW may modify its command line
        const BOOL Created   = ::CreateProcessW(nullptr,
                                              Mutable.data(),
                                              nullptr,
                                              nullptr,
                                              TRUE,
                                              CREATE_NO_WINDOW,
                                              nullptr,
                                              WorkingDir.c_str(),
                                              &Startup,
                                              &Info);
        ::CloseHandle(WritePipe);  // the child holds its own copy now

        if (!Created) {
            ::CloseHandle(ReadPipe);
            Error = "CreateProcess failed (error " + std::to_string(::GetLastError()) + ")";
            return false;
        }
        ::CloseHandle(Info.hThread);

        {
            std::lock_guard Lock(_Mutex);
            _Pending.clear();
        }
        _ExitCode = -1;
        _Finished = false;
        _Running  = true;
        _Process  = Info.hProcess;
        _Reader   = std::thread([this, ReadPipe, Process = Info.hProcess] { ReadLoop(ReadPipe, Process); });
        return true;
    }

    void BuildProcess::ReadLoop(void* ReadPipe, void* Process) {
        std::string Partial;
        char Buffer[4096];
        DWORD Read = 0;

        const auto Push = [this](std::string Line) {
            while (!Line.empty() && (Line.back() == '\r' || Line.back() == '\n')) Line.pop_back();
            std::lock_guard Lock(_Mutex);
            _Pending.push_back(std::move(Line));
        };

        // ReadFile fails with ERROR_BROKEN_PIPE once every writer has exited.
        while (::ReadFile(ReadPipe, Buffer, sizeof(Buffer), &Read, nullptr) && Read > 0) {
            Partial.append(Buffer, Read);
            for (size_t Newline; (Newline = Partial.find('\n')) != std::string::npos;) {
                Push(Partial.substr(0, Newline));
                Partial.erase(0, Newline + 1);
            }
        }
        if (!Partial.empty()) Push(std::move(Partial));
        ::CloseHandle(ReadPipe);

        ::WaitForSingleObject(Process, INFINITE);
        DWORD Code = 1;
        ::GetExitCodeProcess(Process, &Code);

        // The process handle stays open (and in _Process) until Poll/the
        // destructor joins this thread, so TerminateProcess never sees a stale one.
        _ExitCode = static_cast<int>(Code);
        _Finished = true;
    }

    bool BuildProcess::Poll(std::vector<std::string>& Lines, int& ExitCode) {
        {
            std::lock_guard Lock(_Mutex);
            for (auto& Line : _Pending) Lines.push_back(std::move(Line));
            _Pending.clear();
        }

        if (_Running && _Finished) {
            // The worker only sets _Finished after its last push, so nothing is lost: drain once more.
            {
                std::lock_guard Lock(_Mutex);
                for (auto& Line : _Pending) Lines.push_back(std::move(Line));
                _Pending.clear();
            }
            ExitCode = _ExitCode;
            _Running = false;
            if (_Reader.joinable()) _Reader.join();
            if (_Process) {
                ::CloseHandle(_Process);
                _Process = nullptr;
            }
            return true;
        }
        return false;
    }
}  // namespace Xen
