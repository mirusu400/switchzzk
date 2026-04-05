#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"
#include "tab/live_tab.hpp"

class VodCard : public brls::Box {
  public:
    VodCard();
    void setData(const chzzk::VodInfo& info);

  private:
    BRLS_BIND(brls::Image, thumbnail, "vod/thumbnail");
    BRLS_BIND(brls::Label, titleLabel, "vod/title");
    BRLS_BIND(brls::Label, channelLabel, "vod/channel");
    BRLS_BIND(brls::Label, durationLabel, "vod/duration");
    BRLS_BIND(brls::Label, viewsLabel, "vod/views");
};

class VodTab : public brls::Box {
  public:
    VodTab();
    ~VodTab() override;
    static brls::View* create() { return new VodTab(); }

    void fetchVods();
    void playVod(const chzzk::VodInfo& info);

  private:
    void buildGrid();

    BRLS_BIND(brls::Label, statusLabel, "vod/status");
    BRLS_BIND(brls::ProgressSpinner, spinner, "vod/spinner");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "vod/scroll");
    BRLS_BIND(brls::Box, gridBox, "vod/grid");

    std::vector<chzzk::VodInfo> vods_;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
};
