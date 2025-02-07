#pragma once
#include <Geode/utils/web.hpp>

struct IDListDemon {
    int id;
    std::string name;
    int position;
};

struct IDDemonPack {
    std::string name;
    double points;
    std::vector<int> levels;
};

class IntegratedDemonlist {
    static void load(const std::string& url,
                     bool pemonlist,
                     geode::EventListener<geode::Task<geode::utils::web::WebResponse, geode::utils::web::WebProgress>>&&
                     listenerRef, geode::EventListener<geode::Task<geode::utils::web::WebResponse, geode::utils::web::WebProgress>>&&
                     okListener, const std::function<void()>& success, const std::function<void(int)>& failure);

public:
    inline static std::vector<IDListDemon> AREDL = {};
    inline static std::vector<IDListDemon> PEMONLIST = {};
    inline static bool AREDL_LOADED = false;
    inline static bool PEMONLIST_LOADED = false;

    static void loadTSL(
        geode::EventListener<geode::utils::web::WebTask>&&,
        geode::EventListener<geode::utils::web::WebTask>&&,
        const std::function<void()>&,
        const std::function<void(int)>&
    );
    static void loadTSLPlus(
        geode::EventListener<geode::utils::web::WebTask>&&,
        geode::EventListener<geode::utils::web::WebTask>&&,
        const std::function<void()>&,
        const std::function<void(int)>&
    );
};
