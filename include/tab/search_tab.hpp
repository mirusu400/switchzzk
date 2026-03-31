#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

class SearchTab : public brls::Box {
  public:
    SearchTab();
    ~SearchTab() override;
    static brls::View* create() { return new SearchTab(); }

    void doSearch(const std::string& keyword);
    void playLiveChannel(const chzzk::LiveInfo& info);

  private:
    void buildGrid();

    BRLS_BIND(brls::Label, statusLabel, "search/statusbar");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "search/scroll");
    BRLS_BIND(brls::Box, gridBox, "search/grid");

    std::vector<chzzk::LiveInfo> results_;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
    std::string lastKeyword_;
};
