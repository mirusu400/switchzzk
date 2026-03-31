#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

class CategoryTab : public brls::Box {
  public:
    CategoryTab();
    ~CategoryTab() override;
    static brls::View* create() { return new CategoryTab(); }

    void fetchCategories();
    void openCategory(const chzzk::CategoryInfo& cat);
    void playChannel(const chzzk::LiveInfo& info);
    void goBackToCategories();

  private:
    void buildCategoryList();
    void buildLiveGrid();

    BRLS_BIND(brls::Label, statusLabel, "cat/status");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "cat/scroll");
    BRLS_BIND(brls::Box, gridBox, "cat/grid");

    std::vector<chzzk::CategoryInfo> categories_;
    std::vector<chzzk::LiveInfo> categoryLives_;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
    bool showingLives_ = false;
};
