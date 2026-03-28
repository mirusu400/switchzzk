#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

class SearchCell : public brls::RecyclerCell {
  public:
    SearchCell();
    static SearchCell* create();
    void setData(const chzzk::SearchChannelResult& result);

  private:
    BRLS_BIND(brls::Label, channelLabel, "search/channel");
    BRLS_BIND(brls::Label, statusLabel, "search/status");
    BRLS_BIND(brls::Label, titleLabel, "search/title");
};

class SearchTab;

class SearchDataSource : public brls::RecyclerDataSource {
  public:
    explicit SearchDataSource(SearchTab* tab) : tab_(tab) {}
    void setData(std::vector<chzzk::SearchChannelResult> results);
    const chzzk::SearchChannelResult& getItem(int index) const;

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;

  private:
    std::vector<chzzk::SearchChannelResult> results_;
    SearchTab* tab_;
};

class SearchTab : public brls::Box {
  public:
    SearchTab();
    ~SearchTab() override;
    static brls::View* create() { return new SearchTab(); }

    void doSearch(const std::string& keyword);
    void playLiveChannel(const chzzk::SearchChannelResult& result);

  private:
    BRLS_BIND(brls::Label, statusLabel, "search/statusbar");
    BRLS_BIND(brls::RecyclerFrame, recycler, "search/recycler");

    SearchDataSource* dataSource_ = nullptr;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
    std::string lastKeyword_;
};
