#include "tab/vod_tab.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/image_loader.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;

// ─── VodCell ───

VodCell::VodCell() {
    this->inflateFromXMLRes("xml/views/vod_cell.xml");
}
VodCell* VodCell::create() { return new VodCell(); }

void VodCell::setData(const chzzk::VodInfo& info) {
    if (this->titleLabel) this->titleLabel->setText(info.video_title);
    if (this->channelLabel) this->channelLabel->setText(info.channel.channel_name);
    if (this->durationLabel) this->durationLabel->setText(chzzk::format_duration(info.duration));
    if (this->viewsLabel) this->viewsLabel->setText(chzzk::format_viewer_count(info.read_count) + "회");

    // 썸네일 비동기 로딩 (전용 워커 스레드)
    if (this->thumbnail && !info.thumbnail_image_url.empty()) {
        chzzk::ImageLoader::instance().load(info.thumbnail_image_url, this->thumbnail);
    }
}

// ─── VodDataSource ───

void VodDataSource::setData(std::vector<chzzk::VodInfo> vods) { vods_ = std::move(vods); }
const chzzk::VodInfo& VodDataSource::getItem(int index) const { return vods_.at(index); }
int VodDataSource::numberOfSections(brls::RecyclerFrame*) { return 1; }
int VodDataSource::numberOfRows(brls::RecyclerFrame*, int) { return static_cast<int>(vods_.size()); }
float VodDataSource::heightForRow(brls::RecyclerFrame*, brls::IndexPath) { return 110; }

brls::RecyclerCell* VodDataSource::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    auto* cell = dynamic_cast<VodCell*>(recycler->dequeueReusableCell("vod_cell"));
    if (!cell) cell = VodCell::create();
    cell->setData(vods_[index.row]);
    return cell;
}

void VodDataSource::didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) {
    if (tab_ && index.row >= 0 && index.row < static_cast<int>(vods_.size()))
        tab_->playVod(vods_[index.row]);
}

// ─── VodTab ───

VodTab::VodTab() {
    this->inflateFromXMLRes("xml/tabs/vod.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);
    dataSource_ = new VodDataSource(this);

    if (this->recycler) {
        this->recycler->registerCell("vod_cell", []() { return VodCell::create(); });
        this->recycler->setDataSource(dataSource_);
    }

    this->registerAction("새로고침", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->fetchVods();
        return true;
    });

    if (this->statusLabel)
        this->statusLabel->setText("X로 인기 VOD 불러오기");
}

VodTab::~VodTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void VodTab::fetchVods() {
    dbg("VodTab: fetchVods");
    if (this->statusLabel) this->statusLabel->setText("VOD 로딩 중...");

    auto result = chzzkClient_->get_popular_vods(20);
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText("VOD를 불러올 수 없습니다");
        return;
    }

    if (g_logfile) { fprintf(g_logfile, "VodTab: got %zu vods\n", result->data.size()); fflush(g_logfile); }

    dataSource_->setData(std::move(result->data));
    if (this->recycler) this->recycler->reloadData();
    if (this->statusLabel)
        this->statusLabel->setText("인기 VOD " + std::to_string(dataSource_->numberOfRows(nullptr, 0)) + "개");
}

void VodTab::playVod(const chzzk::VodInfo& info) {
    dbg("VodTab: playVod");
    if (this->statusLabel)
        this->statusLabel->setText("VOD 로딩: " + info.video_title);

    // 1. VOD 상세 → videoId + inKey
    auto detail = chzzkClient_->get_vod_detail(info.video_no);
    if (!detail) {
        if (this->statusLabel) this->statusLabel->setText("VOD 상세 조회 실패");
        return;
    }

    // 2. Naver VOD playback → HLS URL
    auto hls_url = chzzkClient_->get_vod_playback_url(*detail);
    if (!hls_url || hls_url->empty()) {
        if (this->statusLabel) this->statusLabel->setText("VOD 재생 URL 획득 실패");
        return;
    }

    if (g_logfile) { fprintf(g_logfile, "VodTab: hls_url=%s\n", hls_url->substr(0, 80).c_str()); fflush(g_logfile); }

    g_pending_playback = chzzk::SwitchPlaybackRequest{
        .title = detail->video_title,
        .url = *hls_url,
        .referer = "",
        .http_header_fields = "",
    };
    g_has_pending_playback = true;
    brls::Application::quit();
}
