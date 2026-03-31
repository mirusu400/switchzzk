#include "tab/search_tab.hpp"
#include "tab/live_tab.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/image_loader.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;


// ─── SearchTab ───

SearchTab::SearchTab() {
    this->inflateFromXMLRes("xml/tabs/search.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);

    this->registerAction("검색", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        brls::Application::getImeManager()->openForText([this](std::string text) {
            this->doSearch(text);
        }, "채널/라이브 검색", "", 30, lastKeyword_);
        return true;
    });
}

SearchTab::~SearchTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void SearchTab::buildGrid() {
    if (!this->gridBox) return;
    this->gridBox->clearViews();

    for (size_t i = 0; i < results_.size(); i += GRID_COLS) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + GRID_COLS && j < results_.size(); j++) {
            auto* card = new LiveCard();
            card->setData(results_[j]);
            size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                this->playLiveChannel(results_[idx]); return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        this->gridBox->addView(row);
    }
}

void SearchTab::doSearch(const std::string& keyword) {
    if (keyword.empty()) return;
    lastKeyword_ = keyword;
    dbg("SearchTab: doSearch");

    if (this->statusLabel) this->statusLabel->setText("\"" + keyword + "\" 검색 중...");

    // 라이브 검색 우선
    auto liveResult = chzzkClient_->search_lives(keyword, 20);

    results_.clear();
    if (liveResult && !liveResult->data.empty())
        results_ = std::move(liveResult->data);

    if (results_.empty()) {
        if (this->statusLabel) this->statusLabel->setText("\"" + keyword + "\" 라이브 결과 없음");
        return;
    }

    this->buildGrid();
    if (this->statusLabel)
        this->statusLabel->setText("\"" + keyword + "\" " + std::to_string(results_.size()) + "개 라이브");
}

void SearchTab::playLiveChannel(const chzzk::LiveInfo& info) {
    dbg("SearchTab: playLiveChannel");
    if (this->statusLabel) this->statusLabel->setText("스트림 해석 중: " + info.channel.channel_name);

    auto detail = chzzkClient_->get_live_detail(info.channel.channel_id);
    if (!detail) { if (this->statusLabel) this->statusLabel->setText("라이브 상세 조회 실패"); return; }

    chzzk::PlaybackPreference pref{false, 720};
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
