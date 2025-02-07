#include "IntegratedDemonlist.hpp"
#include <Geode/utils/ranges.hpp>
#include <queue>

using namespace geode::prelude;

#define TSL_BASE_URL "https://tsl.pages.dev/data"
#define TSL_PLUS_BASE_URL "https://tslplus.pages.dev/data"

void isOk(const std::string& url, EventListener<web::WebTask>&& listenerRef, const bool head, const std::function<void(bool, int)>& callback) {
    auto&& listener = std::move(listenerRef);
    listener.bind([callback](web::WebTask::Event* e) {
        if (const auto res = e->getValue()) callback(res->ok(), res->code());
    });
    listenerRef.setFilter(head ? web::WebRequest().send("HEAD", url) : web::WebRequest().downloadRange({ 0, 0 }).get(url));
}

static std::mutex g_resultsMutex;

struct LoadState {
    std::atomic<size_t> pendingRequests = 0;
    std::vector<IDListDemon> results;
    std::vector<std::unique_ptr<EventListener<web::WebTask>>> listeners;
    std::function<void()> successCallback;
};

void IntegratedDemonlist::load(
    const std::string& url,
    bool pemonlist,
    EventListener<web::WebTask>&& listenerRef,
    EventListener<web::WebTask>&& okListener,
    const std::function<void()>& success,
    const std::function<void(int)>& failure
) {
    auto state = std::make_shared<LoadState>();
    state->successCallback = success;

    auto&& mainListener = std::move(listenerRef);

    // Handler for individual level data requests
    auto handleLevelData = [state, pemonlist](web::WebTask::Event* e, size_t position) {
        if (const auto res = e->getValue()) {
            if (res->ok()) {
                auto json = res->json().unwrapOr(matjson::Value());
                if (json.contains("id") && json.contains("name")) {
                    // Lock mutex when modifying shared data
                    std::lock_guard lock(g_resultsMutex);
                    state->results.push_back(IDListDemon{
                        static_cast<int>(json["id"].asInt().unwrap()),
                        json["name"].asString().unwrap(),
                        static_cast<int>(position + 1)  // 1-based position
                    });
                }
            }

            // Atomically decrease pending requests
            if (--state->pendingRequests == 0) {
                std::lock_guard lock(g_resultsMutex);

                std::vector<IDListDemon> sortedResults = state->results;

                std::ranges::sort(sortedResults,
                    [](const IDListDemon& a, const IDListDemon& b) {
                        return a.position < b.position;
                    });

                if (pemonlist) {
                    PEMONLIST = std::move(sortedResults);
                    PEMONLIST_LOADED = true;
                } else {
                    AREDL = std::move(sortedResults);
                    AREDL_LOADED = true;
                }

                // Call success callback after data is safely copied
                if (state->successCallback) {
                    state->successCallback();
                }
            }
        }
    };

    // Main listener for the list file
    mainListener.bind([state, url, pemonlist, handleLevelData, failure](web::WebTask::Event* e) {
        if (const auto res = e->getValue()) {
            if (!res->ok()) return failure(res->code());

            auto json = res->json().unwrapOr(matjson::Value());
            if (!json.isArray()) return failure(400);

            {
                // Lock mutex when clearing data
                std::lock_guard lock(g_resultsMutex);
                state->results.clear();
                state->listeners.clear();
                if (pemonlist) {
                    PEMONLIST.clear();
                    PEMONLIST_LOADED = false;
                } else {
                    AREDL.clear();
                    AREDL_LOADED = false;
                }
            }

            const auto& levelNames = json.asArray().unwrap();
            state->pendingRequests = 0;  // Reset counter before counting valid entries

            for (size_t i = 0; i < levelNames.size(); ++i) {
                if (!levelNames[i].isString()) {
                    continue;
                }

                ++state->pendingRequests;  // Increment atomically for valid entries
                std::string levelName = levelNames[i].asString().unwrap();
                std::string levelUrl = fmt::format("{}/{}.json", url, levelName);

                auto listener = std::make_unique<EventListener<web::WebTask>>();

                // Bind the listener
                listener->bind([handleLevelData, i](web::WebTask::Event* e) {
                    handleLevelData(e, i);
                });

                // Start the request
                listener->setFilter(web::WebRequest().get(levelUrl));
                state->listeners.push_back(std::move(listener));
            }

            // Handle case where no valid entries were found
            if (state->pendingRequests == 0) {
                std::lock_guard lock(g_resultsMutex);
                if (pemonlist) PEMONLIST_LOADED = true;
                else AREDL_LOADED = true;
                if (state->successCallback) {
                    state->successCallback();
                }
            }
        }
    });

    const auto listURL = fmt::format("{}/_list.json", url);
    isOk(listURL, std::move(okListener), true, [&mainListener, failure, listURL](bool ok, int code) {
        if (ok) mainListener.setFilter(web::WebRequest().get(listURL));
        else failure(code);
    });
}

void IntegratedDemonlist::loadTSL(
    EventListener<web::WebTask>&& listenerRef,
    EventListener<web::WebTask>&& okListener,
    const std::function<void()>& success,
    const std::function<void(int)>& failure
) {
    load(TSL_BASE_URL, false, std::move(listenerRef), std::move(okListener), success, failure);
}

void IntegratedDemonlist::loadTSLPlus(
    EventListener<web::WebTask>&& listenerRef,
    EventListener<web::WebTask>&& okListener,
    const std::function<void()>& success,
    const std::function<void(int)>& failure
) {
    load(TSL_PLUS_BASE_URL, true, std::move(listenerRef), std::move(okListener), success, failure);
}
