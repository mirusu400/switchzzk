#if defined(__SWITCH__)
#include <switch.h>
#endif

#include <borealis.hpp>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "activity/main_activity.hpp"
#include "tab/live_tab.hpp"
#include "tab/category_tab.hpp"
#include "tab/search_tab.hpp"
#include "chzzk/switch_player.hpp"

using namespace brls::literals;

FILE* g_logfile = nullptr;
chzzk::SwitchPlaybackRequest g_pending_playback;
bool g_has_pending_playback = false;

void dbg(const char* msg) {
    if (!g_logfile) return;
    fprintf(g_logfile, "%s\n", msg);
    fflush(g_logfile);
}

static bool run_borealis_ui() {
    dbg("borealis: init");
    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;

    if (!brls::Application::init()) {
        dbg("borealis: init FAILED");
        return false;
    }

    dbg("borealis: createWindow");
    brls::Application::createWindow("Switchzzk");
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::setGlobalQuit(false);

    brls::Application::registerXMLView("LiveTab", LiveTab::create);
    brls::Application::registerXMLView("CategoryTab", CategoryTab::create);
    brls::Application::registerXMLView("SearchTab", SearchTab::create);
    brls::Application::pushActivity(new MainActivity());

    dbg("borealis: entering mainLoop");
    while (brls::Application::mainLoop()) {
    }
    dbg("borealis: mainLoop exited");
    return true;
}

int main(int argc, char* argv[]) {
#ifdef __SWITCH__
    g_logfile = fopen("sdmc:/switch/switchzzk.log", "w");
    dbg("=== switch-chzzk borealis start ===");
#endif

    while (true) {
        // 1. Borealis UI 실행
        g_has_pending_playback = false;
        run_borealis_ui();

        // 2. 재생 요청이 있으면 실행
        if (g_has_pending_playback) {
            dbg("main: launching player after borealis exit");
            std::string error;
            bool ok = chzzk::run_switch_player(g_pending_playback, error);
            if (ok) {
                dbg("main: player finished ok");
            } else {
                if (g_logfile) {
                    fprintf(g_logfile, "main: player failed: %s\n", error.c_str());
                    fflush(g_logfile);
                }
            }
            g_has_pending_playback = false;
            // 플레이어 종료 후 Borealis를 다시 시작
            dbg("main: restarting borealis");
            continue;
        }

        // 재생 요청 없이 종료 = 진짜 종료
        dbg("main: no pending playback, exiting");
        break;
    }

    if (g_logfile) fclose(g_logfile);
    return EXIT_SUCCESS;
}

#ifdef __WINRT__
#include <borealis/core/main.hpp>
#endif
