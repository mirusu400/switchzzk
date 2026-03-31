#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

class FollowingTab : public brls::Box {
  public:
    FollowingTab();
    ~FollowingTab() override;
    static brls::View* create() { return new FollowingTab(); }

    void fetchFollowing();
    void playChannel(const chzzk::LiveInfo& info);
    void showLoginDialog();

  private:
    void buildGrid();

    BRLS_BIND(brls::Label, statusLabel, "following/status");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "following/scroll");
    BRLS_BIND(brls::Box, gridBox, "following/grid");

    std::vector<chzzk::LiveInfo> lives_;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
};
