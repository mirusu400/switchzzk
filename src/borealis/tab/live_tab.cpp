#include "tab/live_tab.hpp"

#include <thread>

#include "chzzk/m3u8.hpp"
#include "chzzk/switch_player.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);

static void dbgstr(const char* prefix, const std::string& s) {
    if (!g_logfile) return;
    fprintf(g_logfile, "%s: %s\n", prefix, s.c_str());
    fflush(g_logfile);
}

using namespace brls::literals;

// ─── LiveCell ───

LiveCell::LiveCell() {
    this->inflateFromXMLRes("xml/views/live_cell.xml");
}

LiveCell* LiveCell::create() {
    return new LiveCell();
}

void LiveCell::setData(const chzzk::LiveInfo& info) {
    if (this->channelLabel)
        this->channelLabel->setText(info.channel.channel_name);
    if (this->titleLabel)
        this->titleLabel->setText(info.live_title);
    if (this->viewerLabel)
        this->viewerLabel->setText(std::to_string(info.concurrent_user_count) + "명");
    if (this->categoryLabel)
        this->categoryLabel->setText(info.live_category_value);
}

// ─── LiveDataSource ───

void LiveDataSource::setData(std::vector<chzzk::LiveInfo> lives) {
    lives_ = std::move(lives);
}

const chzzk::LiveInfo& LiveDataSource::getItem(int index) const {
    return lives_.at(index);
}

int LiveDataSource::numberOfSections(brls::RecyclerFrame* recycler) {
    return 1;
}

int LiveDataSource::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return static_cast<int>(lives_.size());
}

brls::RecyclerCell* LiveDataSource::cellForRow(brls::RecyclerFrame* recycler,
                                                brls::IndexPath index) {
    LiveCell* cell = dynamic_cast<LiveCell*>(recycler->dequeueReusableCell("live_cell"));
    if (!cell) {
        cell = LiveCell::create();
    }
    cell->setData(lives_[index.row]);
    return cell;
}

void LiveDataSource::didSelectRowAt(brls::RecyclerFrame* recycler,
                                     brls::IndexPath index) {
    dbg("didSelectRowAt called");
    if (tab_ && index.row >= 0 && index.row < static_cast<int>(lives_.size())) {
        dbgstr("didSelectRowAt channel", lives_[index.row].channel.channel_name);
        tab_->playChannel(lives_[index.row]);
    } else {
        dbg("didSelectRowAt: out of range or no tab");
    }
}

float LiveDataSource::heightForRow(brls::RecyclerFrame* recycler,
                                    brls::IndexPath index) {
    return 100;
}

// ─── LiveTab ───

LiveTab::LiveTab() {
    this->inflateFromXMLRes("xml/tabs/live.xml");

    dbg("LiveTab: inflated");

    // HTTP + API 클라이언트 초기화
    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);
    dataSource_ = new LiveDataSource(this);

    dbg("LiveTab: clients created");

    if (this->recycler) {
        dbg("LiveTab: recycler bound ok");
        this->recycler->registerCell("live_cell", []() { return LiveCell::create(); });
        this->recycler->setDataSource(dataSource_);
    } else {
        dbg("LiveTab: recycler bind FAILED");
    }

    if (this->statusLabel) {
        dbg("LiveTab: statusLabel bound ok");
        this->statusLabel->setText("CHZZK 라이브 - X로 새로고침");
    } else {
        dbg("LiveTab: statusLabel bind FAILED");
    }

    // X 버튼: 새로고침
    this->registerAction("새로고침", brls::ControllerButton::BUTTON_X, [this](brls::View* view) {
        this->fetchLives();
        return true;
    });

    // Y 버튼: 저지연 토글
    this->registerAction("저지연", brls::ControllerButton::BUTTON_Y, [this](brls::View* view) {
        this->lowLatency_ = !this->lowLatency_;
        if (this->statusLabel) {
            this->statusLabel->setText(
                std::string("CHZZK 라이브") +
                (this->lowLatency_ ? " [LL-HLS]" : " [HLS]"));
        }
        return true;
    });

    dbg("LiveTab: constructor done (no auto-fetch)");
    // 초기 로드는 하지 않음 — 사용자가 X로 새로고침
}

LiveTab::~LiveTab() {
    delete chzzkClient_;
    delete httpClient_;
    // dataSource_는 RecyclerFrame이 소유
}

void LiveTab::fetchLives() {
    dbg("fetchLives begin");

    if (this->statusLabel) {
        this->statusLabel->setText("로딩 중...");
    }

    // 메인 스레드에서 직접 호출 (Switch에서 스레드 안전성 문제 회피)
    dbg("fetchLives: calling get_popular_lives");
    auto result = chzzkClient_->get_popular_lives(20);

    if (!result || result->data.empty()) {
        dbg("fetchLives: FAILED or empty");
        if (this->statusLabel)
            this->statusLabel->setText("라이브를 불러올 수 없습니다");
        return;
    }

    if (g_logfile) { fprintf(g_logfile, "fetchLives: got %zu lives\n", result->data.size()); fflush(g_logfile); }
    dataSource_->setData(std::move(result->data));

    if (this->recycler) {
        dbg("fetchLives: reloadData");
        this->recycler->reloadData();
    }

    if (this->statusLabel) {
        this->statusLabel->setText(
            "CHZZK 라이브 " +
            std::to_string(dataSource_->numberOfRows(nullptr, 0)) + "개" +
            (this->lowLatency_ ? " [LL-HLS]" : " [HLS]"));
    }
    dbg("fetchLives done");
}

void LiveTab::playChannel(const chzzk::LiveInfo& info) {
    dbgstr("playChannel", info.channel.channel_name);

    if (this->statusLabel) {
        this->statusLabel->setText("스트림 해석 중: " + info.channel.channel_name);
    }

    // 1. 라이브 상세 조회
    auto detail = chzzkClient_->get_live_detail(info.channel.channel_id);
    if (!detail) {
        dbg("playChannel: get_live_detail FAILED");
        if (this->statusLabel)
            this->statusLabel->setText("라이브 상세 조회 실패");
        return;
    }

    // 2. 재생 URL 해석
    chzzk::PlaybackPreference pref{lowLatency_, 720};
    auto resolved = chzzkClient_->resolve_playback(*detail, pref);
    if (!resolved || resolved->selected_url.empty()) {
        dbg("playChannel: resolve_playback FAILED");
        if (this->statusLabel)
            this->statusLabel->setText("스트림 URL 해석 실패");
        return;
    }

    // 3. referer 추출
    std::string referer;
    {
        auto pos = resolved->selected_url.find("://");
        if (pos != std::string::npos) {
            auto slash = resolved->selected_url.find('/', pos + 3);
            if (slash != std::string::npos)
                referer = resolved->selected_url.substr(0, slash + 1);
        }
    }

    chzzk::SwitchPlaybackRequest request{
        .title = detail->live_title,
        .url = resolved->selected_url,
        .referer = referer,
        .http_header_fields =
            "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache",
    };

    dbgstr("playChannel: launching player url", request.url);

    // Borealis GL 컨텍스트와 충돌하므로, Borealis를 먼저 종료
    dbg("playChannel: quitting borealis for player");
    brls::Application::quit();

    // 글로벌에 재생 요청을 저장 — main에서 처리
    extern chzzk::SwitchPlaybackRequest g_pending_playback;
    extern bool g_has_pending_playback;
    g_pending_playback = request;
    g_has_pending_playback = true;
}
