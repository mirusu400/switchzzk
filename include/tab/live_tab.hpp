#pragma once

#include <borealis.hpp>

#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

inline constexpr int GRID_COLS = 4;

// 개별 카드 (그리드 내)
class LiveCard : public brls::Box {
  public:
    LiveCard();
    void setData(const chzzk::LiveInfo& info);

  private:
    BRLS_BIND(brls::Image, thumbnail, "cell/thumbnail");
    BRLS_BIND(brls::Label, titleLabel, "cell/title");
    BRLS_BIND(brls::Label, channelLabel, "cell/channel");
    BRLS_BIND(brls::Label, viewerLabel, "cell/viewers");
    BRLS_BIND(brls::Label, categoryLabel, "cell/category");
};

// 1열 리스트용 셀 (카테고리/팔로잉 등)
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

// 라이브 탭 (ScrollingFrame + Box 그리드)
class LiveTab : public brls::Box {
  public:
    LiveTab();
    ~LiveTab() override;

    static brls::View* create() { return new LiveTab(); }

    void fetchLives();
    void loadMore();
    void playChannel(const chzzk::LiveInfo& info);

  private:
    void buildGrid();

    BRLS_BIND(brls::Label, statusLabel, "live/status");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "live/scroll");
    BRLS_BIND(brls::Box, gridBox, "live/grid");

    std::vector<chzzk::LiveInfo> lives_;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
    bool lowLatency_ = false;

    std::optional<int> nextConcurrentUserCount_;
    std::optional<std::int64_t> nextLiveId_;
    bool loading_ = false;
};
