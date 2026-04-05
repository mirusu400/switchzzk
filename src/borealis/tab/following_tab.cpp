#include "tab/following_tab.hpp"
#include "tab/live_tab.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/image_loader.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;
extern std::string g_nid_aut;
extern std::string g_nid_ses;


FollowingTab::FollowingTab() {
    this->inflateFromXMLRes("xml/tabs/following.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);

    this->registerAction("새로고침/로그인", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        if (g_nid_aut.empty()) this->showLoginDialog();
        else this->fetchFollowing();
        return true;
    });

    if (!g_nid_aut.empty()) this->fetchFollowing();
    else if (this->statusLabel) this->statusLabel->setText("X 버튼으로 로그인");
}

FollowingTab::~FollowingTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void FollowingTab::buildGrid() {
    if (!this->gridBox) return;
    this->gridBox->clearViews();

    for (size_t i = 0; i < lives_.size(); i += GRID_COLS) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + GRID_COLS && j < lives_.size(); j++) {
            auto* card = new LiveCard();
            card->setData(lives_[j]);
            size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                this->playChannel(lives_[idx]); return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        this->gridBox->addView(row);
    }
}

void FollowingTab::showLoginDialog() {
    brls::Application::getImeManager()->openForText([this](std::string nidAut) {
        if (nidAut.empty()) return;
        brls::Application::getImeManager()->openForText([this, nidAut](std::string nidSes) {
            if (nidSes.empty()) return;
            g_nid_aut = nidAut;
            g_nid_ses = nidSes;
            FILE* f = fopen("sdmc:/switch/switchzzk_auth.txt", "w");
            if (f) { fprintf(f, "%s\n%s\n", g_nid_aut.c_str(), g_nid_ses.c_str()); fclose(f); }
            this->httpClient_->setAuthCookies(g_nid_aut, g_nid_ses);
            this->fetchFollowing();
        }, "NID_SES 입력", "PC 브라우저에서 복사", 500, "");
    }, "NID_AUT 입력", "PC chzzk.naver.com 쿠키에서 복사", 500, "");
}

void FollowingTab::fetchFollowing() {
    dbg("FollowingTab: fetchFollowing");
    httpClient_->setAuthCookies(g_nid_aut, g_nid_ses);
    if (this->statusLabel) this->statusLabel->setText("팔로잉 로딩 중...");
    if (this->spinner) this->spinner->setVisibility(brls::Visibility::VISIBLE);

    auto result = chzzkClient_->get_following_lives();
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText("팔로잉 라이브 없음 (X: 로그인)");
        if (this->spinner) this->spinner->setVisibility(brls::Visibility::GONE);
        return;
    }

    lives_ = std::move(result->data);
    this->buildGrid();
    if (this->spinner) this->spinner->setVisibility(brls::Visibility::GONE);
    if (this->statusLabel)
        this->statusLabel->setText("팔로잉 라이브 " + std::to_string(lives_.size()) + "개");
}

void FollowingTab::playChannel(const chzzk::LiveInfo& info) {
    dbg("FollowingTab: playChannel");
    httpClient_->setAuthCookies(g_nid_aut, g_nid_ses);

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
        .channel_id = info.channel.channel_id, .channel_name = info.channel.channel_name,
        .chat_channel_id = detail->chat_channel_id,
    };
    g_has_pending_playback = true;
    brls::Application::quit();
}
