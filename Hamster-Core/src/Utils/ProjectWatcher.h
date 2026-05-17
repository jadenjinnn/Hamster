#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <thread>
#include <vector>

namespace Hamster {

// File system event captured by ProjectWatcher and dispatched to the main
// thread for asset reconciliation. The watcher does not act on its own —
// it only reports what changed; the owner (AssetManager) decides what to do.
struct FileEvent {
    enum class Kind { Added, Removed, Renamed, Modified };
    Kind kind;
    std::filesystem::path path;    // Added / Removed / Modified — the file
    std::filesystem::path oldPath; // Renamed — the previous filename
};

// Watches a single directory on disk for file changes and posts batched
// FileEvents to the main thread via the provided enqueue callable. Win32-
// only for v1 (consistent with the editor's existing Win32-only chrome —
// Aero Snap, drag, borderless subclass). On non-Win32 builds the worker
// thread is a no-op and no events fire.
//
// Lifetime: construct after the directory exists, destruct before the
// directory is deleted. Project owns its instance and tears it down in
// its dtor.
class ProjectWatcher {
public:
    using MainThreadEnqueue = std::function<void(std::function<void()>)>;
    using EventCallback = std::function<void(std::vector<FileEvent>)>;

    ProjectWatcher(const std::filesystem::path &watchDir,
                   MainThreadEnqueue enqueue,
                   EventCallback onEvents);
    ~ProjectWatcher();

    ProjectWatcher(const ProjectWatcher &) = delete;
    ProjectWatcher &operator=(const ProjectWatcher &) = delete;

private:
    void WorkerLoop();

    std::filesystem::path m_WatchDir;
    MainThreadEnqueue m_Enqueue;
    EventCallback m_OnEvents;

#ifdef _WIN32
    void *m_DirHandle = nullptr;  // HANDLE
    void *m_StopEvent = nullptr;  // HANDLE
    void *m_IoEvent = nullptr;    // HANDLE
#endif
    std::thread m_Thread;
    std::atomic<bool> m_Stopping{false};
};

} // namespace Hamster
