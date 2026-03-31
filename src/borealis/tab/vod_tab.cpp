#include "tab/vod_tab.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/m3u8.hpp"
#include <sstream>
#include <atomic>
#include "chzzk/image_loader.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;


// ─── VodCard ───

VodCard::VodCard() { this->inflateFromXMLRes("xml/views/vod_card.xml"); }

void VodCard::setData(const chzzk::VodInfo& info) {
    if (this->titleLabel) this->titleLabel->setText(info.video_title);
    if (this->channelLabel) this->channelLabel->setText(info.channel.channel_name);
    if (this->durationLabel) this->durationLabel->setText(chzzk::format_duration(info.duration));
    if (this->viewsLabel) this->viewsLabel->setText(chzzk::format_viewer_count(info.read_count) + "회");
    if (this->thumbnail && !info.thumbnail_image_url.empty())
        chzzk::ImageLoader::instance().load(info.thumbnail_image_url, this->thumbnail);
}

// ─── VodTab ───

VodTab::VodTab() {
    this->inflateFromXMLRes("xml/tabs/vod.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);

    this->registerAction("새로고침", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->fetchVods(); return true;
    });

    dbg("VodTab: auto-fetching");
    this->fetchVods();
}

VodTab::~VodTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void VodTab::buildGrid() {
    if (!this->gridBox) return;
    this->gridBox->clearViews();

    for (size_t i = 0; i < vods_.size(); i += GRID_COLS) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + GRID_COLS && j < vods_.size(); j++) {
            auto* card = new VodCard();
            card->setData(vods_[j]);
            size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                this->playVod(vods_[idx]); return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        this->gridBox->addView(row);
    }
}

void VodTab::fetchVods() {
    dbg("VodTab: fetchVods");
    if (this->statusLabel) this->statusLabel->setText("VOD 로딩 중...");

    auto result = chzzkClient_->get_popular_vods(20);
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText("VOD를 불러올 수 없습니다");
        return;
    }

    vods_ = std::move(result->data);
    this->buildGrid();
    if (this->statusLabel)
        this->statusLabel->setText("인기 VOD " + std::to_string(vods_.size()) + "개");
}

void VodTab::playVod(const chzzk::VodInfo& info) {
    dbg("VodTab: playVod");
    if (g_logfile) { fprintf(g_logfile, "VodTab: videoNo=%d title=%s\n", info.video_no, info.video_title.c_str()); fflush(g_logfile); }
    if (this->statusLabel) this->statusLabel->setText("VOD 로딩: " + info.video_title);

    auto detail = chzzkClient_->get_vod_detail(info.video_no);
    if (!detail) { dbg("VodTab: get_vod_detail FAILED"); if (this->statusLabel) this->statusLabel->setText("VOD 상세 조회 실패"); return; }

    std::string play_url;

    // 방법 1: liveRewindPlaybackJson (akamaized CDN — Switch에서 재생 가능)
    if (!detail->media.empty()) {
        dbg("VodTab: using liveRewind path");
        chzzk::PlaybackPreference pref{false, 720};
        auto resolved = chzzkClient_->resolve_playback_from_media(detail->media, pref);
        if (resolved.has_value())
            play_url = resolved->selected_url;
    }

    // 방법 2: neonplayer — 앱이 variant m3u8을 다운받아서 chzzkvod:// 스트림으로 제공
    if (play_url.empty()) {
        dbg("VodTab: using neonplayer via chzzkvod stream");
        auto hls_url = chzzkClient_->get_vod_playback_url(*detail);
        if (hls_url.has_value() && !hls_url->empty()) {
            // variant 선택
            std::string master_query;
            auto qpos = hls_url->find('?');
            if (qpos != std::string::npos) master_query = hls_url->substr(qpos);

            auto master_data = httpClient_->get(*hls_url);
            if (master_data) {
                auto variants = chzzk::parse_variant_playlist(*master_data, *hls_url);
                chzzk::PlaybackPreference pref{false, 720};
                auto selected = chzzk::choose_variant(variants, pref.max_height);
                std::string variant_url = selected ? selected->uri : *hls_url;
                if (variant_url.find('?') == std::string::npos && !master_query.empty())
                    variant_url += master_query;

                // variant m3u8에서 segment 목록 추출
                auto variant_data = httpClient_->get(variant_url);
                if (variant_data) {
                    std::string vbase = variant_url;
                    auto vq = vbase.find('?');
                    if (vq != std::string::npos) vbase = vbase.substr(0, vq);
                    std::string vdir = vbase.substr(0, vbase.rfind('/') + 1);

                    // 전역에 segment URL 목록 저장 — stream_cb에서 사용
                    g_vod_segments.clear();

                    std::istringstream iss(*variant_data);
                    std::string line;
                    while (std::getline(iss, line)) {
                        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                        if (line.empty() || line[0] == '#') continue;
                        std::string seg = (line.find("://") != std::string::npos) ? line : vdir + line;
                        if (seg.find('?') == std::string::npos && !master_query.empty()) seg += master_query;
                        g_vod_segments.push_back(seg);
                    }
                    g_vod_ready = true;
                    play_url = "chzzkvod://play";
                    if (g_logfile) { fprintf(g_logfile, "VodTab: %zu segments prepared\n", g_vod_segments.size()); fflush(g_logfile); }
                }
            }
        }
    }

    if (play_url.empty()) {
        if (this->statusLabel) this->statusLabel->setText("VOD URL 획득 실패");
        return;
    }

    if (g_logfile) { fprintf(g_logfile, "VodTab: play_url=%s\n", play_url.substr(0,100).c_str()); fflush(g_logfile); }

    std::string referer;
    auto pos = play_url.find("://");
    if (pos != std::string::npos) {
        auto slash = play_url.find('/', pos + 3);
        if (slash != std::string::npos) referer = play_url.substr(0, slash + 1);
    }

    g_pending_playback = chzzk::SwitchPlaybackRequest{
        .title = detail->video_title,
        .url = play_url,
        .referer = referer,
        .http_header_fields = "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache",
    };
    g_has_pending_playback = true;
    brls::Application::quit();
}
