//
// Created by Jake Rieger on 10/4/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Platform.hpp>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Xen {
    /// @brief Runs one command line as a hidden child process and streams its
    /// combined stdout/stderr, a line at a time, to whoever polls it.
    ///
    /// The reading happens on a worker thread; everything the editor touches
    /// (Poll, IsRunning) is safe to call from the UI thread every frame.
    class BuildProcess {
    public:
        BuildProcess() = default;
        ~BuildProcess();

        BuildProcess(const BuildProcess&)            = delete;
        BuildProcess& operator=(const BuildProcess&) = delete;

        /// @brief Starts CommandLine (as given to CreateProcess) in WorkingDir.
        /// False, with Error filled in, if the process couldn't be created or
        /// one is already running.
        bool Start(const std::wstring& CommandLine, const fs::path& WorkingDir, std::string& Error);

        NODISCARD bool IsRunning() const { return _Running; }

        /// @brief Moves the output lines read since the last call into Lines.
        /// Returns true exactly once per run, after the process has exited and
        /// all of its output has been delivered; ExitCode is then valid.
        bool Poll(std::vector<std::string>& Lines, int& ExitCode);

    private:
        void ReadLoop(void* ReadPipe, void* Process);

        std::thread _Reader;
        std::mutex _Mutex;
        std::vector<std::string> _Pending;
        std::atomic<bool> _Running {false};
        std::atomic<bool> _Finished {false};  // set by the worker once output is complete
        int _ExitCode {-1};
        void* _Process {nullptr};  // HANDLE, for terminating on destruction
    };
}  // namespace Xen
