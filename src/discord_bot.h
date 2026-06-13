#pragma once

#include <curl/curl.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

class DiscordBot
{
public:
    explicit DiscordBot(std::string bot_token);
    ~DiscordBot();

    // Non-copyable, non-movable (owns raw pointer + detached threads)
    DiscordBot(const DiscordBot&)            = delete;
    DiscordBot& operator=(const DiscordBot&) = delete;

    /**
     * Starts the WebSocket I/O thread and blocks until the bot receives the
     * READY dispatch (fully online) or the timeout elapses.
     *
     * Returns true if the bot came online within the timeout, false otherwise.
     */
    bool ConnectAndWaitUntilReady(std::chrono::seconds timeout = std::chrono::seconds(15));

    bool IsReady() const { return is_ready_; }

    // Queues a raw gateway payload (used by native modules)
    void EnqueuePresenceUpdate(const std::string& payload) { EnqueuePayload(payload); }

private:
    void Shutdown();

    void EnqueuePayload(std::string payload);
    std::string DequeuePayload();

    void SendIdentify();
    void HeartbeatLoop(int interval_ms);
    void HandleGuildCreate(const std::string& data);
    void ProcessPacket(const std::string& packet);
    void IoLoop();

    // ── Members ───────────────────────────────────────────────────────────

    std::string          token_;
    CURL*                ws_curl_;
    std::atomic<bool>    is_running_;

    std::atomic<bool>       is_ready_;
    std::mutex              ready_mutex_;
    std::condition_variable ready_cv_;

    std::mutex              queue_mutex_;
    std::queue<std::string> send_queue_;
};