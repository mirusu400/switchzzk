#include "tab/category_tab.hpp"
#include "chzzk/switch_player.hpp"

extern FILE* g_logfile;
extern void dbg(const char* msg);
extern chzzk::SwitchPlaybackRequest g_pending_playback;
extern bool g_has_pending_playback;

// ─── CategoryCell ───

CategoryCell::CategoryCell() {
    dbg("CategoryCell: inflating");
    this->inflateFromXMLRes("xml/views/category_cell.xml");
    dbg("CategoryCell: inflated ok");
}
CategoryCell* CategoryCell::create() { return new CategoryCell(); }

void CategoryCell::setData(const chzzk::CategoryInfo& info) {
    if (this->nameLabel) this->nameLabel->setText(info.category_value);
    if (this->viewerLabel)
        this->viewerLabel->setText(chzzk::format_viewer_count(info.concurrent_user_count) + "명");
}

// ─── CategoryDataSource ───

void CategoryDataSource::setData(std::vector<chzzk::CategoryInfo> cats) { cats_ = std::move(cats); }
int CategoryDataSource::numberOfSections(brls::RecyclerFrame*) { return 1; }
int CategoryDataSource::numberOfRows(brls::RecyclerFrame*, int) { return static_cast<int>(cats_.size()); }
float CategoryDataSource::heightForRow(brls::RecyclerFrame*, brls::IndexPath) { return 60; }

brls::RecyclerCell* CategoryDataSource::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    dbg("CategoryDS: cellForRow");
    auto* cell = dynamic_cast<CategoryCell*>(recycler->dequeueReusableCell("cat_cell"));
    if (!cell) {
        dbg("CategoryDS: creating new cell");
        cell = CategoryCell::create();
    }
    dbg("CategoryDS: setData");
    cell->setData(cats_[index.row]);
    dbg("CategoryDS: cellForRow done");
    return cell;
}

void CategoryDataSource::didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) {
    if (tab_ && index.row >= 0 && index.row < static_cast<int>(cats_.size()))
        tab_->openCategory(cats_[index.row]);
}

// ─── CategoryLiveDataSource ───

void CategoryLiveDataSource::setData(std::vector<chzzk::LiveInfo> lives) { lives_ = std::move(lives); }
const chzzk::LiveInfo& CategoryLiveDataSource::getItem(int index) const { return lives_.at(index); }
int CategoryLiveDataSource::numberOfSections(brls::RecyclerFrame*) { return 1; }
int CategoryLiveDataSource::numberOfRows(brls::RecyclerFrame*, int) { return static_cast<int>(lives_.size()); }
float CategoryLiveDataSource::heightForRow(brls::RecyclerFrame*, brls::IndexPath) { return 100; }

brls::RecyclerCell* CategoryLiveDataSource::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    auto* cell = dynamic_cast<LiveCell*>(recycler->dequeueReusableCell("live_cell"));
    if (!cell) cell = LiveCell::create();
    cell->setData(lives_[index.row]);
    return cell;
}

void CategoryLiveDataSource::didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) {
    if (tab_ && index.row >= 0 && index.row < static_cast<int>(lives_.size()))
        tab_->playChannel(lives_[index.row]);
}

// ─── CategoryTab ───

CategoryTab::CategoryTab() {
    this->inflateFromXMLRes("xml/tabs/category.xml");

    httpClient_ = new chzzk::HttpsHttpClient();
    chzzkClient_ = new chzzk::ChzzkClient(*httpClient_);
    catDataSource_ = new CategoryDataSource(this);
    liveDataSource_ = new CategoryLiveDataSource(this);

    if (this->recycler) {
        this->recycler->registerCell("cat_cell", []() { return CategoryCell::create(); });
        this->recycler->registerCell("live_cell", []() { return LiveCell::create(); });
        this->recycler->setDataSource(catDataSource_);
    }

    // X: 새로고침
    this->registerAction("새로고침", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        if (showingLives_)
            goBackToCategories();
        else
            fetchCategories();
        return true;
    });

    // B: 라이브 목록에서 카테고리 목록으로 복귀
    this->registerAction("뒤로", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        if (showingLives_) {
            goBackToCategories();
            return true;
        }
        return false;  // 기본 동작 (탭 전환)
    });

    dbg("CategoryTab: constructor done");
}

CategoryTab::~CategoryTab() {
    delete chzzkClient_;
    delete httpClient_;
}

void CategoryTab::fetchCategories() {
    dbg("CategoryTab: fetchCategories begin");
    if (this->statusLabel) this->statusLabel->setText("카테고리 로딩 중...");

    dbg("CategoryTab: calling API");
    auto result = chzzkClient_->get_live_categories(30);
    dbg("CategoryTab: API returned");

    if (!result || result->data.empty()) {
        dbg("CategoryTab: no data");
        if (this->statusLabel) this->statusLabel->setText("카테고리를 불러올 수 없습니다");
        return;
    }

    if (g_logfile) { fprintf(g_logfile, "CategoryTab: got %zu categories\n", result->data.size()); fflush(g_logfile); }

    catDataSource_->setData(std::move(result->data));
    dbg("CategoryTab: setData done");

    if (this->recycler) {
        dbg("CategoryTab: reloadData");
        this->recycler->reloadData();
        dbg("CategoryTab: reloadData done");
    }
    showingLives_ = false;
    if (this->statusLabel)
        this->statusLabel->setText("카테고리 " + std::to_string(catDataSource_->numberOfRows(nullptr, 0)) + "개");
    dbg("CategoryTab: fetchCategories done");
}

void CategoryTab::openCategory(const chzzk::CategoryInfo& cat) {
    dbg("CategoryTab: openCategory");
    if (this->statusLabel) this->statusLabel->setText(cat.category_value + " 로딩 중...");

    auto result = chzzkClient_->get_category_lives(cat.category_type, cat.category_id, 20);
    if (!result || result->data.empty()) {
        if (this->statusLabel) this->statusLabel->setText(cat.category_value + " - 라이브 없음");
        return;
    }

    int count = static_cast<int>(result->data.size());
    liveDataSource_->setData(std::move(result->data));
    if (this->recycler) {
        this->recycler->setDataSource(liveDataSource_);
        this->recycler->reloadData();
    }
    showingLives_ = true;
    if (this->statusLabel)
        this->statusLabel->setText(cat.category_value + " " + std::to_string(count) + "개 라이브  (B: 뒤로)");
}

void CategoryTab::playChannel(const chzzk::LiveInfo& info) {
    dbg("CategoryTab: playChannel");
    if (this->statusLabel)
        this->statusLabel->setText("스트림 해석 중: " + info.channel.channel_name);

    auto detail = chzzkClient_->get_live_detail(info.channel.channel_id);
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

void CategoryTab::goBackToCategories() {
    showingLives_ = false;
    if (this->recycler) {
        this->recycler->setDataSource(catDataSource_);
        this->recycler->reloadData();
    }
    if (this->statusLabel)
        this->statusLabel->setText("카테고리 " + std::to_string(catDataSource_->numberOfRows(nullptr, 0)) + "개");
}
