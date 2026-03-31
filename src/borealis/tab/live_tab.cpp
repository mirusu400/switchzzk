#include "tab/live_tab.hpp"

#include "chzzk/m3u8.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/image_loader.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);

static void dbgstr(const char* prefix, const std::string& s) {
    if (!g_logfile) return;
    fprintf(g_logfile, "%s: %s\n", prefix, s.c_str());
    fflush(g_logfile);
}

extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;

// ─── LiveCard ───

LiveCard::LiveCard() {
    this->inflateFromXMLRes("xml/views/live_card.xml");
}

void LiveCard::setData(const chzzk::LiveInfo& info) {
    if (this->channelLabel) this->channelLabel->setText(info.channel.channel_name);
    if (this->titleLabel) this->titleLabel->setText(info.live_title);
    if (this->viewerLabel) this->viewerLabel->setText(chzzk::format_viewer_count(info.concurrent_user_count) + "명");
    if (this->categoryLabel) this->categoryLabel->setText(info.live_category_value);
    if (this->thumbnail && !info.live_image_url.empty()) {
        std::string url = info.live_image_url;
        auto pos = url.find("{type}");
        if (pos != std::string::npos) url.replace(pos, 6, "480");
        chzzk::ImageLoader::instance().load(url, this->thumbnail);
    }
}

// ─── LiveCell (1열 리스트용) ───

LiveCell::LiveCell() { this->inflateFromXMLRes("xml/views/live_cell.xml"); }
LiveCell* LiveCell::create() { return new LiveCell(); }

void LiveCell::setData(const chzzk::LiveInfo& info) {
    if (this->channelLabel) this->channelLabel->setText(info.channel.channel_name);
    if (this->titleLabel) this->titleLabel->setText(info.live_title);
    if (this->viewerLabel) this->viewerLabel->setText(chzzk::format_viewer_count(info.concurrent_user_count) + "명");
    if (this->categoryLabel) this->categoryLabel->setText(info.live_category_value);
    if (this->thumbnail && !info.live_image_url.empty()) {
        std::string url = info.live_image_url;
        auto pos = url.find("{type}");
        if (pos != std::string::npos) url.replace(pos, 6, "480");
        chzzk::ImageLoader::instance().load(url, this->thumbnail);
    }
}

// ─── LiveTab ───

LiveTab::LiveTab() {
    this->inflateFromXMLRes("xml/tabs/live.xml");
    dbg("LiveTab: inflated");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);
    dbg("LiveTab: clients created");

    this->registerAction("새로고침", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->fetchLives(); return true;
    });
    this->registerAction("더 보기", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
        this->loadMore(); return true;
    });
    this->registerAction("저지연", brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        lowLatency_ = !lowLatency_;
        if (this->statusLabel)
            this->statusLabel->setText("LIVE " + std::to_string(lives_.size()) + "개" +
                                        (lowLatency_ ? " [LL-HLS]" : ""));
        return true;
    });

    dbg("LiveTab: auto-fetching");
    this->fetchLives();
}

LiveTab::~LiveTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void LiveTab::buildGrid() {
    if (!this->gridBox) return;

    dbg("buildGrid begin");
    this->gridBox->clearViews();

    for (size_t i = 0; i < lives_.size(); i += GRID_COLS) {
        // 한 행 = 가로 Box
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + GRID_COLS && j < lives_.size(); j++) {
            auto* card = new LiveCard();
            card->setData(lives_[j]);

            // 카드 클릭 → 재생
            size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                this->playChannel(lives_[idx]);
                return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));

            row->addView(card);
        }

        this->gridBox->addView(row);
    }
    dbg("buildGrid done");
}

void LiveTab::fetchLives() {
    dbg("fetchLives begin");
    if (this->statusLabel) this->statusLabel->setText("로딩 중...");
    loading_ = true;

    auto result = chzzkClient_->get_popular_lives(20);
    if (!result || result->data.empty()) {
        dbg("fetchLives: FAILED");
        if (this->statusLabel) this->statusLabel->setText("라이브를 불러올 수 없습니다");
        loading_ = false;
        return;
    }

    nextConcurrentUserCount_ = result->next_concurrent_user_count;
    nextLiveId_ = result->next_live_id;

    if (g_logfile) { fprintf(g_logfile, "fetchLives: got %zu lives\n", result->data.size()); fflush(g_logfile); }

    lives_ = std::move(result->data);
    this->buildGrid();

    if (this->statusLabel)
        this->statusLabel->setText("LIVE " + std::to_string(lives_.size()) + "개" +
                                    (lowLatency_ ? " [LL-HLS]" : ""));
    loading_ = false;
    dbg("fetchLives done");
}

void LiveTab::loadMore() {
    if (loading_ || !nextConcurrentUserCount_.has_value()) return;
    loading_ = true;
    dbg("loadMore begin");

    auto result = chzzkClient_->get_popular_lives(20, nextConcurrentUserCount_, nextLiveId_);
    if (!result || result->data.empty()) { loading_ = false; return; }

    nextConcurrentUserCount_ = result->next_concurrent_user_count;
    nextLiveId_ = result->next_live_id;

    lives_.insert(lives_.end(), std::make_move_iterator(result->data.begin()),
                  std::make_move_iterator(result->data.end()));
    this->buildGrid();

    if (this->statusLabel)
        this->statusLabel->setText("LIVE " + std::to_string(lives_.size()) + "개" +
                                    (lowLatency_ ? " [LL-HLS]" : ""));
    loading_ = false;
}

void LiveTab::playChannel(const chzzk::LiveInfo& info) {
    dbgstr("playChannel", info.channel.channel_name);
    if (this->statusLabel) this->statusLabel->setText("스트림 해석 중: " + info.channel.channel_name);

    auto detail = chzzkClient_->get_live_detail(info.channel.channel_id);
    if (!detail) { if (this->statusLabel) this->statusLabel->setText("라이브 상세 조회 실패"); return; }

    chzzk::PlaybackPreference pref{lowLatency_, 720};
    auto resolved = chzzkClient_->resolve_playback(*detail, pref);
    if (!resolved || resolved->selected_url.empty()) { if (this->statusLabel) this->statusLabel->setText("스트림 URL 해석 실패"); return; }

    std::string referer;
    auto pos = resolved->selected_url.find("://");
    if (pos != std::string::npos) {
        auto slash = resolved->selected_url.find('/', pos + 3);
        if (slash != std::string::npos) referer = resolved->selected_url.substr(0, slash + 1);
    }

    g_pending_playback = chzzk::SwitchPlaybackRequest{
        .title = detail->live_title, .url = resolved->selected_url, .referer = referer,
        .http_header_fields = "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache",
    };
    g_has_pending_playback = true;
    brls::Application::quit();
}
