#include "HamsterPCH.h"

#include "ProjectWatcher.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Hamster {

ProjectWatcher::ProjectWatcher(const std::filesystem::path &watchDir,
                               MainThreadEnqueue enqueue,
                               EventCallback onEvents)
    : m_WatchDir(watchDir),
      m_Enqueue(std::move(enqueue)),
      m_OnEvents(std::move(onEvents)) {
#ifdef _WIN32
    // FILE_FLAG_OVERLAPPED so ReadDirectoryChangesW can be cancelled by
    // signalling our stop event. FILE_LIST_DIRECTORY is the minimum access
    // mode needed to watch a directory.
    m_DirHandle = CreateFileW(
        watchDir.wstring().c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (m_DirHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "ProjectWatcher: CreateFileW failed for "
                  << watchDir.string() << " (err " << GetLastError() << ")"
                  << std::endl;
        m_DirHandle = nullptr;
        return;
    }

    // Auto-reset events. m_IoEvent is signalled by ReadDirectoryChangesW
    // when its overlapped IO completes. m_StopEvent is signalled by the
    // dtor to break the worker loop.
    m_IoEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_StopEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    m_Thread = std::thread([this]() { WorkerLoop(); });
#endif
}

ProjectWatcher::~ProjectWatcher() {
#ifdef _WIN32
    m_Stopping.store(true);

    if (m_StopEvent) SetEvent(static_cast<HANDLE>(m_StopEvent));

    // CancelIoEx unblocks any pending ReadDirectoryChangesW so the worker's
    // wait returns promptly rather than sitting until the next FS event.
    if (m_DirHandle) {
        CancelIoEx(static_cast<HANDLE>(m_DirHandle), nullptr);
    }

    if (m_Thread.joinable()) m_Thread.join();

    if (m_DirHandle) CloseHandle(static_cast<HANDLE>(m_DirHandle));
    if (m_StopEvent) CloseHandle(static_cast<HANDLE>(m_StopEvent));
    if (m_IoEvent) CloseHandle(static_cast<HANDLE>(m_IoEvent));
#endif
}

#ifdef _WIN32
void ProjectWatcher::WorkerLoop() {
    constexpr DWORD kNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME |
                                     FILE_NOTIFY_CHANGE_DIR_NAME |
                                     FILE_NOTIFY_CHANGE_LAST_WRITE;

    // 64 KiB buffer — plenty for a handful of file events at once. If a
    // single batch overflows this, Windows reports ERROR_NOTIFY_ENUM_DIR
    // and we recover by re-issuing the watch (events are still seen by the
    // next call when files keep changing).
    std::vector<unsigned char> buffer(64 * 1024);

    HANDLE dir = static_cast<HANDLE>(m_DirHandle);
    HANDLE ioEvent = static_cast<HANDLE>(m_IoEvent);
    HANDLE stopEvent = static_cast<HANDLE>(m_StopEvent);

    HANDLE waitHandles[2] = {stopEvent, ioEvent};

    while (!m_Stopping.load()) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = ioEvent;

        DWORD bytesReturned = 0;
        BOOL ok = ReadDirectoryChangesW(
            dir, buffer.data(), static_cast<DWORD>(buffer.size()),
            /*watchSubtree*/ TRUE, kNotifyFilter, &bytesReturned,
            &overlapped, nullptr);

        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) break; // cancelled in dtor
            std::cerr << "ProjectWatcher: ReadDirectoryChangesW failed (err "
                      << err << ")" << std::endl;
            return;
        }

        DWORD wait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0 || m_Stopping.load()) {
            CancelIoEx(dir, &overlapped);
            // Drain the cancellation so the OVERLAPPED isn't reused mid-op.
            GetOverlappedResult(dir, &overlapped, &bytesReturned, TRUE);
            break;
        }

        if (!GetOverlappedResult(dir, &overlapped, &bytesReturned, FALSE)) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) break;
            std::cerr << "ProjectWatcher: GetOverlappedResult failed (err "
                      << err << ")" << std::endl;
            return;
        }

        if (bytesReturned == 0) {
            // Buffer overflowed — events were lost. Just re-arm.
            continue;
        }

        // Parse FILE_NOTIFY_INFORMATION records into FileEvents. Renames
        // arrive as consecutive OLD_NAME + NEW_NAME entries; we coalesce
        // them here into a single Renamed event.
        std::vector<FileEvent> batch;

        unsigned char *p = buffer.data();
        std::filesystem::path pendingRenameOld;
        bool havePendingRename = false;

        for (;;) {
            FILE_NOTIFY_INFORMATION *info =
                reinterpret_cast<FILE_NOTIFY_INFORMATION *>(p);

            std::wstring wname(info->FileName,
                               info->FileNameLength / sizeof(wchar_t));
            std::filesystem::path full = m_WatchDir / wname;

            switch (info->Action) {
            case FILE_ACTION_ADDED:
                batch.push_back({FileEvent::Kind::Added, full, {}});
                break;
            case FILE_ACTION_REMOVED:
                batch.push_back({FileEvent::Kind::Removed, full, {}});
                break;
            case FILE_ACTION_MODIFIED:
                batch.push_back({FileEvent::Kind::Modified, full, {}});
                break;
            case FILE_ACTION_RENAMED_OLD_NAME:
                pendingRenameOld = full;
                havePendingRename = true;
                break;
            case FILE_ACTION_RENAMED_NEW_NAME:
                if (havePendingRename) {
                    batch.push_back({FileEvent::Kind::Renamed, full,
                                     pendingRenameOld});
                    havePendingRename = false;
                } else {
                    // NEW_NAME with no preceding OLD_NAME — treat as add.
                    batch.push_back({FileEvent::Kind::Added, full, {}});
                }
                break;
            default:
                break;
            }

            if (info->NextEntryOffset == 0) break;
            p += info->NextEntryOffset;
        }

        // Orphan OLD_NAME with no NEW_NAME — treat as remove.
        if (havePendingRename) {
            batch.push_back({FileEvent::Kind::Removed, pendingRenameOld, {}});
        }

        if (!batch.empty()) {
            // Move-capture the batch into the main-thread lambda so we don't
            // race with the next loop iteration that reuses the buffer.
            auto cb = m_OnEvents;
            auto events = std::move(batch);
            m_Enqueue([cb = std::move(cb), events = std::move(events)]() {
                cb(events);
            });
        }
    }
}
#else
void ProjectWatcher::WorkerLoop() {}
#endif

} // namespace Hamster
