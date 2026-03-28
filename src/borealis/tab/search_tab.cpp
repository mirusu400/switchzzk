#include "tab/search_tab.hpp"
#include "chzzk/switch_player.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;

// ─── SearchCell ───

SearchCell::SearchCell() {
    this->inflateFromXMLRes("xml/views/search_cell.xml");
}

SearchCell* SearchCell::create() { return new SearchCell(); }

void SearchCell::setData(const chzzk::SearchChannelResult& result) {
    if (this->channelLabel)
        this->channelLabel->setText(result.channel.channel_name);
    if (this->statusLabel) {
        if (result.is_live)
            this->statusLabel->setText("LIVE " + chzzk::format_viewer_count(result.concurrent_user_count) + "명");
        else
            this->statusLabel->setText("오프라인");
    }
    if (this->titleLabel) {
        this->titleLabel->setText(result.is_live ? result.live_title : "");
    }
}

// ─── SearchDataSource ───

void SearchDataSource::setData(std::vector<chzzk::SearchChannelResult> results) {
    results_ = std::move(results);
}

const chzzk::SearchChannelResult& SearchDataSource::getItem(int index) const {
    return results_.at(index);
}

int SearchDataSource::numberOfSections(brls::RecyclerFrame* recycler) { return 1; }
int SearchDataSource::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return static_cast<int>(results_.size());
}

brls::RecyclerCell* SearchDataSource::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    auto* cell = dynamic_cast<SearchCell*>(recycler->dequeueReusableCell("search_cell"));
    if (!cell) cell = SearchCell::create();
    cell->setData(results_[index.row]);
    return cell;
}

void SearchDataSource::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    if (tab_ && index.row >= 0 && index.row < static_cast<int>(results_.size())) {
        if (results_[index.row].is_live) {
            tab_->playLiveChannel(results_[index.row]);
        }
    }
}

float SearchDataSource::heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    return 80;
}

// ─── SearchTab ───

SearchTab::SearchTab() {
    this->inflateFromXMLRes("xml/tabs/search.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);
    dataSource_ = new SearchDataSource(this);

    if (this->recycler) {
        this->recycler->registerCell("search_cell", []() { return SearchCell::create(); });
        this->recycler->setDataSource(dataSource_);
    }

    // X: 검색어 입력 (Borealis 키보드)
    this->registerAction("검색", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        brls::Application::getImeManager()->openForText([this](std::string text) {
            this->doSearch(text);
        }, "채널 검색", "", 30, lastKeyword_);
        return true;
    });
}

SearchTab::~SearchTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void SearchTab::doSearch(const std::string& keyword) {
    if (keyword.empty()) return;
    lastKeyword_ = keyword;
    dbg("SearchTab: doSearch");

    if (this->statusLabel) this->statusLabel->setText("\"" + keyword + "\" 검색 중...");

    auto result = chzzkClient_->search_channels(keyword, 20);
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText("\"" + keyword + "\" 결과 없음");
        return;
    }

    dataSource_->setData(std::move(result->data));
    if (this->recycler) this->recycler->reloadData();
    if (this->statusLabel)
        this->statusLabel->setText("\"" + keyword + "\" " +
                                    std::to_string(dataSource_->numberOfRows(nullptr, 0)) + "개 채널");
}

void SearchTab::playLiveChannel(const chzzk::SearchChannelResult& result) {
    dbg("SearchTab: playLiveChannel");
    if (this->statusLabel)
        this->statusLabel->setText("스트림 해석 중: " + result.channel.channel_name);

    auto detail = chzzkClient_->get_live_detail(result.channel.channel_id);
    if (!detail) {
        if (this->statusLabel) this->statusLabel->setText("라이브 상세 조회 실패");
        return;
    }

    chzzk::PlaybackPreference pref{false, 720};
    auto resolved = chzzkClient_->resolve_playback(*detail, pref);
    if (!resolved || resolved->selected_url.empty()) {
        if (this->statusLabel) this->statusLabel->setText("스트림 URL 해석 실패");
        return;
    }

    std::string referer;
    auto pos = resolved->selected_url.find("://");
    if (pos != std::string::npos) {
        auto slash = resolved->selected_url.find('/', pos + 3);
        if (slash != std::string::npos)
            referer = resolved->selected_url.substr(0, slash + 1);
    }

    g_pending_playback = chzzk::SwitchPlaybackRequest{
        .title = detail->live_title,
        .url = resolved->selected_url,
        .referer = referer,
        .http_header_fields = "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache",
    };
    g_has_pending_playback = true;
    brls::Application::quit();
}
