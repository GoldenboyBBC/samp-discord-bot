// src/rest_client.cpp
#include "rest_client.h"

#include <curl/curl.h>
#include <iostream>
#include <string>

namespace rest {

static std::string s_token;
static const std::string kBaseUrl = "https://discord.com/api/v10";

void Init(const std::string& bot_token) {
    s_token = bot_token;
}

// libcurl write callback — appends received data into a std::string
static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string Request(const std::string& method,
                           const std::string& endpoint,
                           const std::string& body = "")
{
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string response;
    std::string url = kBaseUrl + endpoint;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bot " + s_token).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS,    CURLSSLOPT_NATIVE_CA);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST,       1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET is the default

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        std::cerr << "[REST] " << method << " " << endpoint
                  << " failed: " << curl_easy_strerror(res) << '\n';

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

std::string Get   (const std::string& endpoint)                          { return Request("GET",    endpoint);       }
std::string Post  (const std::string& endpoint, const std::string& body) { return Request("POST",   endpoint, body); }
std::string Patch (const std::string& endpoint, const std::string& body) { return Request("PATCH",  endpoint, body); }
std::string Delete(const std::string& endpoint)                          { return Request("DELETE", endpoint);       }

} // namespace rest