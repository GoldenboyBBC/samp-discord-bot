#include <stdint.h>
#include <stddef.h>
#include "samp-plugin-sdk/plugincommon.h"
#include "samp-plugin-sdk/amx/amx.h"

#include <curl/curl.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

// ─── Globals ────────────────────────────────────────────────────────────────

// Defined by the SDK in amxplugin.cpp — we only declare it here.
extern void* pAMXFunctions;

// ─── Helpers ────────────────────────────────────────────────────────────────

/**
 * Extracts a primitive (non-nested) JSON value for the given key.
 * Returns std::nullopt when the key is absent or malformed.
 */
static std::optional<std::string> GetJsonValue(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return std::nullopt;

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return std::nullopt;

    pos = json.find_first_not_of(" \t\n\r\"", pos + 1);
    if (pos == std::string::npos) return std::nullopt;

    size_t end = json.find_first_of(" ,}\"", pos);
    return (end == std::string::npos) ? json.substr(pos)
                                      : json.substr(pos, end - pos);
}

/**
 * Reads "discord_token" / "DISCORD_TOKEN" from server.cfg.
 * Returns an empty string when missing.
 */
static std::string GetTokenFromConfig()
{
    std::ifstream file("server.cfg");
    if (!file.is_open()) return {};

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key != "discord_token" && key != "DISCORD_TOKEN") continue;

        std::string token;
        if (!(iss >> token)) continue;

        if (!token.empty() && token.front() == '"') token.erase(token.begin());
        if (!token.empty() && token.back()  == '"') token.pop_back();

        return token;
    }
    return {};
}

// ─── DiscordBot ─────────────────────────────────────────────────────────────

class DiscordBot
{
public:
    explicit DiscordBot(std::string bot_token)
        : token_(std::move(bot_token))
        , ws_curl_(nullptr)
        , is_running_(false)
        , is_ready_(false)
    {}

    ~DiscordBot()
    {
        Shutdown();
    }

    DiscordBot(const DiscordBot&)            = delete;
    DiscordBot& operator=(const DiscordBot&) = delete;

    /**
     * Starts the I/O thread and blocks until the bot receives the READY event
     * (meaning it is fully online) or the timeout elapses.
     *
     * Returns true if the bot came online within the timeout, false otherwise.
     */
    bool ConnectAndWaitUntilReady(std::chrono::seconds timeout = std::chrono::seconds(15))
    {
        ws_curl_ = curl_easy_init();
        if (!ws_curl_) return false;

        curl_easy_setopt(ws_curl_, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
        curl_easy_setopt(ws_curl_, CURLOPT_URL, "wss://gateway.discord.gg/?v=10&encoding=json");
        curl_easy_setopt(ws_curl_, CURLOPT_CONNECT_ONLY, 2L);

        std::thread(&DiscordBot::IoLoop, this).detach();

        // Block the calling thread (Load) until READY or timeout
        std::unique_lock<std::mutex> lock(ready_mutex_);
        bool came_online = ready_cv_.wait_for(lock, timeout, [this] { return is_ready_.load(); });

        if (!came_online)
            std::cerr << "[DiscordBot] Timed out waiting for READY event.\n";

        return came_online;
    }

    bool IsReady() const { return is_ready_; }

private:
    // ── Internal helpers ──────────────────────────────────────────────────

    void Shutdown()
    {
        is_running_ = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (ws_curl_) {
            curl_easy_cleanup(ws_curl_);
            ws_curl_ = nullptr;
        }
    }

    void EnqueuePayload(std::string payload)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        send_queue_.push(std::move(payload));
    }

    std::string DequeuePayload()
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (send_queue_.empty()) return {};
        std::string payload = std::move(send_queue_.front());
        send_queue_.pop();
        return payload;
    }

    void SendIdentify()
    {
        EnqueuePayload(
            "{\"op\":2,\"d\":{"
            "\"token\":\"" + token_ + "\","
            "\"intents\":33280,"
            "\"properties\":{\"os\":\"windows\",\"browser\":\"libcurl\",\"device\":\"libcurl\"}"
            "}}"
        );
    }

    void HeartbeatLoop(int interval_ms)
    {
        while (is_running_) {
            const int slice_ms = 100;
            for (int elapsed = 0; elapsed < interval_ms && is_running_; elapsed += slice_ms)
                std::this_thread::sleep_for(std::chrono::milliseconds(slice_ms));

            if (is_running_)
                EnqueuePayload("{\"op\":1,\"d\":null}");
        }
    }

    void ProcessPacket(const std::string& packet)
    {
        auto op = GetJsonValue(packet, "op");
        if (!op) return;

        if (*op == "10") {                      // Hello — start heartbeat and identify
            SendIdentify();

            auto interval_str = GetJsonValue(packet, "heartbeat_interval");
            if (interval_str) {
                try {
                    int interval = std::stoi(*interval_str);
                    std::thread(&DiscordBot::HeartbeatLoop, this, interval).detach();
                } catch (const std::exception& e) {
                    std::cerr << "[DiscordBot] Bad heartbeat_interval: " << e.what() << '\n';
                }
            }
            return;
        }

        // op 0 = Dispatch; t = "READY" means the bot is fully online
        if (*op == "0") {
            auto t = GetJsonValue(packet, "t");
            if (t && *t == "READY") {
                std::cout << "[DiscordBot] Bot is online and ready.\n";
                {
                    std::lock_guard<std::mutex> lock(ready_mutex_);
                    is_ready_ = true;
                }
                ready_cv_.notify_all();
            }
        }
    }

    void IoLoop()
    {
        CURLcode res = curl_easy_perform(ws_curl_);
        if (res != CURLE_OK) {
            std::cerr << "[DiscordBot] curl_easy_perform failed: "
                      << curl_easy_strerror(res) << '\n';
            ready_cv_.notify_all(); // unblock Load so it doesn't hang forever
            return;
        }

        is_running_ = true;

        constexpr size_t kBufferSize = 8192;
        char buffer[kBufferSize];
        std::string payload_builder;

        while (is_running_) {
            // ── Send ──────────────────────────────────────────────────────
            std::string to_send = DequeuePayload();
            if (!to_send.empty()) {
                size_t sent = 0;
                CURLcode send_res = curl_ws_send(
                    ws_curl_, to_send.c_str(), to_send.size(), &sent, 0, CURLWS_TEXT);
                if (send_res != CURLE_OK) {
                    std::cerr << "[DiscordBot] Send error: "
                              << curl_easy_strerror(send_res) << '\n';
                }
            }

            // ── Receive ───────────────────────────────────────────────────
            size_t rlen = 0;
            const struct curl_ws_frame* meta = nullptr;
            CURLcode recv_res = curl_ws_recv(
                ws_curl_, buffer, kBufferSize, &rlen, &meta);

            if (recv_res == CURLE_OK && rlen > 0) {
                payload_builder.append(buffer, rlen);
                if (!meta || meta->bytesleft == 0) {
                    ProcessPacket(payload_builder);
                    payload_builder.clear();
                }
            } else if (recv_res == CURLE_AGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::cerr << "[DiscordBot] Receive error: "
                          << curl_easy_strerror(recv_res) << '\n';
                is_running_ = false;
                ready_cv_.notify_all(); // unblock Load if we die before READY
            }
        }
    }

    // ── Members ───────────────────────────────────────────────────────────

    std::string             token_;
    CURL*                   ws_curl_;
    std::atomic<bool>       is_running_;

    std::atomic<bool>       is_ready_;
    std::mutex              ready_mutex_;
    std::condition_variable ready_cv_;

    std::mutex              queue_mutex_;
    std::queue<std::string> send_queue_;
};

// ─── Plugin-level bot instance ───────────────────────────────────────────────

static DiscordBot* g_bot = nullptr;

// ─── Plugin entry points ─────────────────────────────────────────────────────

extern "C" {

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void** ppData)
{
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    curl_global_init(CURL_GLOBAL_ALL);

    std::string token = GetTokenFromConfig();
    if (token.empty()) {
        std::cerr << "[DiscordBot] No token found in server.cfg — aborting.\n";
        return false;
    }

    g_bot = new DiscordBot(std::move(token));

    // Block here until READY (or timeout). Returning false aborts the plugin load.
    if (!g_bot->ConnectAndWaitUntilReady()) {
        delete g_bot;
        g_bot = nullptr;
        return false;
    }

    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
    delete g_bot;
    g_bot = nullptr;
    curl_global_cleanup();
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX* amx)
{
    return AMX_ERR_NONE; // No natives registered — connect is automatic now
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX* /*amx*/)
{
    return AMX_ERR_NONE;
}

} // extern "C"