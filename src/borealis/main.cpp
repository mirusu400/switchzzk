#if defined(__SWITCH__)
#include <switch.h>
#endif

#include <borealis.hpp>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <vector>

#include "activity/main_activity.hpp"
#include "tab/live_tab.hpp"
#include "tab/category_tab.hpp"
#include "tab/search_tab.hpp"
#include "tab/vod_tab.hpp"
#include "tab/following_tab.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/custom_button.hpp"
#include "view/svg_image.hpp"
#include "view/recycling_grid.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/image_loader.hpp"
#include "chzzk/recent_channels.hpp"

using namespace brls::literals;

FILE* g_logfile = nullptr;
chzzk::SwitchPlaybackRequest g_pending_playback;
bool g_has_pending_playback = false;
std::string g_nid_aut;
std::string g_nid_ses;
std::vector<std::string> g_vod_segments;
std::atomic<bool> g_vod_ready{false};
std::string g_last_playback_error;

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

    // 치지직 다크 테마 커스텀 컬러
    brls::Theme::getDarkTheme().addColor("brls/sidebar/active_item", nvgRGB(0, 255, 163));

    // wiliwili 포팅 컴포넌트용 커스텀 컬러 (color/grey_1, color/grey_2, color/grey_3, color/chzzk)
    brls::Theme::getDarkTheme().addColor("color/grey_1", nvgRGB(24, 26, 31));
    brls::Theme::getDarkTheme().addColor("color/grey_2", nvgRGB(30, 32, 35));
    brls::Theme::getDarkTheme().addColor("color/grey_3", nvgRGBA(160, 160, 160, 160));
    brls::Theme::getDarkTheme().addColor("color/chzzk", nvgRGB(0, 255, 163));
    brls::Theme::getLightTheme().addColor("color/grey_1", nvgRGB(245, 246, 247));
    brls::Theme::getLightTheme().addColor("color/grey_2", nvgRGB(235, 236, 238));
    brls::Theme::getLightTheme().addColor("color/grey_3", nvgRGBA(200, 200, 200, 16));
    brls::Theme::getLightTheme().addColor("color/chzzk", nvgRGB(0, 200, 130));

    // 사이드바 아이콘 전용 너비 (wiliwili main.xml: 100)
    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 100);

    // RecyclingGrid용 span 메트릭 (720p 기준 3열)
    brls::getStyle().addMetric("chzzk/grid/span/4", 3);
    brls::getStyle().addMetric("chzzk/grid/span/3", 2);

    // wiliwili에서 포팅한 커스텀 뷰 등록
    brls::Application::registerXMLView("AutoTabFrame", AutoTabFrame::create);
    brls::Application::registerXMLView("CustomButton", CustomButton::create);
    brls::Application::registerXMLView("SVGImage", SVGImage::create);
    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);

    brls::Application::registerXMLView("LiveTab", LiveTab::create);
    brls::Application::registerXMLView("CategoryTab", CategoryTab::create);
    brls::Application::registerXMLView("SearchTab", SearchTab::create);
    brls::Application::registerXMLView("VodTab", VodTab::create);
    brls::Application::registerXMLView("FollowingTab", FollowingTab::create);
    brls::Application::pushActivity(new MainActivity());

    chzzk::ImageLoader::instance().start();

    // Show error dialog if previous playback failed
    if (!g_last_playback_error.empty()) {
        std::string errMsg = g_last_playback_error;
        g_last_playback_error.clear();
        brls::sync([errMsg]() {
            auto* dialog = new brls::Dialog("재생 실패: " + errMsg);
            dialog->addButton("확인", [dialog]() { dialog->close(); });
            dialog->setCancelable(true);
            dialog->open();
        });
    }

    dbg("borealis: entering mainLoop");
    while (brls::Application::mainLoop()) {
    }
    dbg("borealis: mainLoop exited");

    chzzk::ImageLoader::instance().stop();
    return true;
}

int main(int argc, char* argv[]) {
#ifdef __SWITCH__
    g_logfile = fopen("sdmc:/switch/switchzzk.log", "w");
    dbg("=== switchzzk v20260330-fix5 icon-only+autoload+search ===");
#endif

    // romfs 폰트를 sdmc에 복사 (mpv OSD용)
    {
        FILE* src = fopen("romfs:/font/NanumGothic.ttf", "rb");
        if (src) {
            FILE* dst = fopen("sdmc:/switch/switchzzk_font.ttf", "wb");
            if (dst) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                    fwrite(buf, 1, n, dst);
                fclose(dst);
                dbg("font: copied to sdmc");
            }
            fclose(src);
        }
    }

    // 저장된 인증 쿠키 로드
    {
        FILE* f = fopen("sdmc:/switch/switchzzk_auth.txt", "r");
        if (f) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), f)) {
                g_nid_aut = buf;
                while (!g_nid_aut.empty() && (g_nid_aut.back() == '\n' || g_nid_aut.back() == '\r'))
                    g_nid_aut.pop_back();
            }
            if (fgets(buf, sizeof(buf), f)) {
                g_nid_ses = buf;
                while (!g_nid_ses.empty() && (g_nid_ses.back() == '\n' || g_nid_ses.back() == '\r'))
                    g_nid_ses.pop_back();
            }
            fclose(f);
            if (!g_nid_aut.empty()) dbg("auth: loaded saved cookies");
        }
    }

    while (true) {
        // 1. Borealis UI 실행
        g_has_pending_playback = false;
        run_borealis_ui();

        // 2. 재생 요청이 있으면 실행
        if (g_has_pending_playback) {
            dbg("main: launching player after borealis exit");

            // Save recent channel before playback
            if (!g_pending_playback.channel_id.empty()) {
                chzzk::add_recent_channel(g_pending_playback.channel_id, g_pending_playback.channel_name);
                dbg("main: saved recent channel");
            }

            std::string error;
            bool ok = chzzk::run_switch_player(g_pending_playback, error);
            if (ok) {
                dbg("main: player finished ok");
            } else {
                if (g_logfile) {
                    fprintf(g_logfile, "main: player failed: %s\n", error.c_str());
                    fflush(g_logfile);
                }
                g_last_playback_error = error;
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
