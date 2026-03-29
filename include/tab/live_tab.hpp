#pragma once

#include <borealis.hpp>

#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

// 개별 카드 셀
class LiveCell : public brls::RecyclerCell {
  public:
    LiveCell();
    static LiveCell* create();

    void setData(const chzzk::LiveInfo& info);

  private:
    BRLS_BIND(brls::Image, thumbnail, "cell/thumbnail");
    BRLS_BIND(brls::Label, titleLabel, "cell/title");
    BRLS_BIND(brls::Label, channelLabel, "cell/channel");
    BRLS_BIND(brls::Label, viewerLabel, "cell/viewers");
    BRLS_BIND(brls::Label, categoryLabel, "cell/category");
};

class LiveTab;

// 데이터 소스
class LiveDataSource : public brls::RecyclerDataSource {
  public:
    explicit LiveDataSource(LiveTab* tab) : tab_(tab) {}

    void setData(std::vector<chzzk::LiveInfo> lives);
    void appendData(std::vector<chzzk::LiveInfo> lives);
    const chzzk::LiveInfo& getItem(int index) const;
    int totalItems() const { return static_cast<int>(lives_.size()); }

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;

  private:
    std::vector<chzzk::LiveInfo> lives_;
    LiveTab* tab_;
};

// 라이브 탭
class LiveTab : public brls::Box {
  public:
    LiveTab();
    ~LiveTab() override;

    static brls::View* create() { return new LiveTab(); }

    void fetchLives();
    void loadMore();
    void playChannel(const chzzk::LiveInfo& info);

  private:
    BRLS_BIND(brls::Label, statusLabel, "live/status");
    BRLS_BIND(brls::RecyclerFrame, recycler, "live/recycler");

    LiveDataSource* dataSource_ = nullptr;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
    bool lowLatency_ = false;

    // 페이지네이션
    std::optional<int> nextConcurrentUserCount_;
    std::optional<std::int64_t> nextLiveId_;
    bool loading_ = false;
};
