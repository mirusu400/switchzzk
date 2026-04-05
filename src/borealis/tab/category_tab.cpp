#include "tab/category_tab.hpp"
#include "tab/live_tab.hpp"
#include "chzzk/switch_player.hpp"
#include "chzzk/image_loader.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;


// ─── CategoryTab ───

CategoryTab::CategoryTab() {
    this->inflateFromXMLRes("xml/tabs/category.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);

    this->registerAction("새로고침", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        if (showingLives_) goBackToCategories();
        else fetchCategories();
        return true;
    });

    this->registerAction("뒤로", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        if (showingLives_) { goBackToCategories(); return true; }
        return false;
    });

    dbg("CategoryTab: auto-fetching");
    this->fetchCategories();
}

CategoryTab::~CategoryTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void CategoryTab::buildCategoryList() {
    if (!this->gridBox) return;
    this->gridBox->clearViews();

    for (size_t i = 0; i < categories_.size(); i++) {
        auto* item = new brls::Box(brls::Axis::ROW);
        item->setMarginBottom(6);
        item->setPadding(10, 16, 10, 16);
        item->setCornerRadius(10);
        item->setBackgroundColor(nvgRGBA(30, 32, 35, 255));
        item->setFocusable(true);
        item->setAlignItems(brls::AlignItems::CENTER);
        item->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);

        auto* name = new brls::Label();
        name->setText(categories_[i].category_value);
        name->setFontSize(16);
        name->setTextColor(nvgRGBA(255, 255, 255, 255));

        auto* viewers = new brls::Label();
        viewers->setText(chzzk::format_viewer_count(categories_[i].concurrent_user_count) + "명");
        viewers->setFontSize(13);
        viewers->setTextColor(nvgRGBA(0, 255, 163, 255));

        item->addView(name);
        item->addView(viewers);

        size_t idx = i;
        item->registerClickAction([this, idx](brls::View*) {
            this->openCategory(categories_[idx]); return true;
        });
        item->addGestureRecognizer(new brls::TapGestureRecognizer(item));

        this->gridBox->addView(item);
    }
}

void CategoryTab::buildLiveGrid() {
    if (!this->gridBox) return;
    this->gridBox->clearViews();

    for (size_t i = 0; i < categoryLives_.size(); i += GRID_COLS) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + GRID_COLS && j < categoryLives_.size(); j++) {
            auto* card = new LiveCard();
            card->setData(categoryLives_[j]);
            size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                this->playChannel(categoryLives_[idx]); return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        this->gridBox->addView(row);
    }
}

void CategoryTab::fetchCategories() {
    dbg("CategoryTab: fetchCategories");
    if (this->statusLabel) this->statusLabel->setText("카테고리 로딩 중...");
    if (this->spinner) this->spinner->setVisibility(brls::Visibility::VISIBLE);

    auto result = chzzkClient_->get_live_categories(30);
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText("카테고리를 불러올 수 없습니다");
        if (this->spinner) this->spinner->setVisibility(brls::Visibility::GONE);
        return;
    }

    categories_ = std::move(result->data);
    showingLives_ = false;
    this->buildCategoryList();
    if (this->spinner) this->spinner->setVisibility(brls::Visibility::GONE);
    if (this->statusLabel)
        this->statusLabel->setText("카테고리 " + std::to_string(categories_.size()) + "개");
}

void CategoryTab::openCategory(const chzzk::CategoryInfo& cat) {
    dbg("CategoryTab: openCategory");
    if (this->statusLabel) this->statusLabel->setText(cat.category_value + " 로딩 중...");
    if (this->spinner) this->spinner->setVisibility(brls::Visibility::VISIBLE);

    auto result = chzzkClient_->get_category_lives(cat.category_type, cat.category_id, 20);
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText(cat.category_value + " - 라이브 없음");
        if (this->spinner) this->spinner->setVisibility(brls::Visibility::GONE);
        return;
    }

    categoryLives_ = std::move(result->data);
    showingLives_ = true;
    this->buildLiveGrid();
    if (this->spinner) this->spinner->setVisibility(brls::Visibility::GONE);
    if (this->statusLabel)
        this->statusLabel->setText(cat.category_value + " " + std::to_string(categoryLives_.size()) + "개 라이브  (B: 뒤로)");
}

void CategoryTab::playChannel(const chzzk::LiveInfo& info) {
    dbg("CategoryTab: playChannel");
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
        .channel_id = info.channel.channel_id, .channel_name = info.channel.channel_name,
        .chat_channel_id = detail->chat_channel_id,
    };
    g_has_pending_playback = true;
    brls::Application::quit();
}

void CategoryTab::goBackToCategories() {
    showingLives_ = false;
    this->buildCategoryList();
    if (this->statusLabel)
        this->statusLabel->setText("카테고리 " + std::to_string(categories_.size()) + "개");
}
