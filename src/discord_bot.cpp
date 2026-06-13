#include "discord_bot.h"
#include "json_utils.h"
#include "channel_store.h"

#include <chrono>
#include <iostream>
#include <thread>

// ─── Construction / destruction ──────────────────────────────────────────────

DiscordBot::DiscordBot(std::string bot_token)
    : token_(std::move(bot_token))
    , ws_curl_(nullptr)
    , is_running_(false)
    , is_ready_(false)
{}

DiscordBot::~DiscordBot()
{
    Shutdown();
}

// ─── Public ──────────────────────────────────────────────────────────────────

bool DiscordBot::ConnectAndWaitUntilReady(std::chrono::seconds timeout)
{
    ws_curl_ = curl_easy_init();
    if (!ws_curl_) return false;

    curl_easy_setopt(ws_curl_, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(ws_curl_, CURLOPT_URL, "wss://gateway.discord.gg/?v=10&encoding=json");
    curl_easy_setopt(ws_curl_, CURLOPT_CONNECT_ONLY, 2L);

    std::thread(&DiscordBot::IoLoop, this).detach();

    std::unique_lock<std::mutex> lock(ready_mutex_);
    bool came_online = ready_cv_.wait_for(lock, timeout, [this] { return is_ready_.load(); });

    if (!came_online)
        std::cerr << "[DiscordBot] Timed out waiting for READY event.\n";

    return came_online;
}

// ─── Private ─────────────────────────────────────────────────────────────────

void DiscordBot::Shutdown()
{
    is_running_ = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (ws_curl_) {
        curl_easy_cleanup(ws_curl_);
        ws_curl_ = nullptr;
    }
}

void DiscordBot::EnqueuePayload(std::string payload)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    send_queue_.push(std::move(payload));
}

std::string DiscordBot::DequeuePayload()
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (send_queue_.empty()) return {};
    std::string payload = std::move(send_queue_.front());
    send_queue_.pop();
    return payload;
}

void DiscordBot::SendIdentify()
{
    EnqueuePayload(
        "{\"op\":2,\"d\":{"
        "\"token\":\"" + token_ + "\","
        "\"intents\":33280,"
        "\"properties\":{\"os\":\"windows\",\"browser\":\"libcurl\",\"device\":\"libcurl\"}"
        "}}"
    );
}

void DiscordBot::HeartbeatLoop(int interval_ms)
{
    while (is_running_) {
        const int slice_ms = 100;
        for (int elapsed = 0; elapsed < interval_ms && is_running_; elapsed += slice_ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(slice_ms));

        if (is_running_)
            EnqueuePayload("{\"op\":1,\"d\":null}");
    }
}

void DiscordBot::HandleGuildCreate(const std::string& data)
{
    auto guild_id = GetJsonValue(data, "id");
    if (!guild_id) return;

    // Find the "channels" array
    size_t pos = data.find("\"channels\"");
    if (pos == std::string::npos) return;

    pos = data.find('[', pos);
    if (pos == std::string::npos) return;

    // Walk each channel object in the array
    int depth = 0;
    size_t obj_start = std::string::npos;

    for (size_t i = pos; i < data.size(); ++i) {
        if (data[i] == '{') {
            if (depth == 0) obj_start = i;
            ++depth;
        } else if (data[i] == '}') {
            --depth;
            if (depth == 0 && obj_start != std::string::npos) {
                std::string obj = data.substr(obj_start, i - obj_start + 1);

                auto id   = GetJsonValue(obj, "id");
                auto name = GetJsonValue(obj, "name");
                auto type = GetJsonValue(obj, "type");

                if (id && name && type) {
                    try {
                        Channel ch;
                        ch.id       = *id;
                        ch.name     = *name;
                        ch.guild_id = *guild_id;
                        ch.type     = std::stoi(*type);
                        channel_store::Add(ch);
                    } catch (...) {}
                }
                obj_start = std::string::npos;
            }
        } else if (data[i] == ']' && depth == 0) {
            break; // end of channels array
        }
    }

    std::cout << "[DiscordBot] Guild " << *guild_id << " channels loaded.\n";
}

void DiscordBot::ProcessPacket(const std::string& packet)
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

    if (*op == "0") {                       // Dispatch
        auto t = GetJsonValue(packet, "t");
        if (t && *t == "READY") {
            std::cout << "[DiscordBot] Bot is online and ready.\n";
            {
                std::lock_guard<std::mutex> lock(ready_mutex_);
                is_ready_ = true;
            }
            ready_cv_.notify_all();
        } else if (t && *t == "GUILD_CREATE") {
            // Extract the "d" object and pass it to the guild handler
            size_t d_pos = packet.find("\"d\":");
            if (d_pos != std::string::npos)
            {
                HandleGuildCreate(packet.substr(d_pos + 4));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
}

void DiscordBot::IoLoop()
{
    CURLcode res = curl_easy_perform(ws_curl_);
    if (res != CURLE_OK) {
        std::cerr << "[DiscordBot] curl_easy_perform failed: "
                  << curl_easy_strerror(res) << '\n';
        ready_cv_.notify_all();
        return;
    }

    is_running_ = true;

    constexpr size_t kBufferSize = 65536;
    char buffer[kBufferSize];
    std::string payload_builder;

    while (is_running_) {
        // ── Send ──────────────────────────────────────────────────────────
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

        // ── Receive ───────────────────────────────────────────────────────
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
            ready_cv_.notify_all();
        }
    }
}