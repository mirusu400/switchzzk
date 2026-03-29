#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

class VodCell : public brls::RecyclerCell {
  public:
    VodCell();
    static VodCell* create();
    void setData(const chzzk::VodInfo& info);

  private:
    BRLS_BIND(brls::Image, thumbnail, "vod/thumbnail");
    BRLS_BIND(brls::Label, titleLabel, "vod/title");
    BRLS_BIND(brls::Label, channelLabel, "vod/channel");
    BRLS_BIND(brls::Label, durationLabel, "vod/duration");
    BRLS_BIND(brls::Label, viewsLabel, "vod/views");
};

class VodTab;

class VodDataSource : public brls::RecyclerDataSource {
  public:
    explicit VodDataSource(VodTab* tab) : tab_(tab) {}
    void setData(std::vector<chzzk::VodInfo> vods);
    const chzzk::VodInfo& getItem(int index) const;

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;

  private:
    std::vector<chzzk::VodInfo> vods_;
    VodTab* tab_;
};

class VodTab : public brls::Box {
  public:
    VodTab();
    ~VodTab() override;
    static brls::View* create() { return new VodTab(); }

    void fetchVods();
    void playVod(const chzzk::VodInfo& info);
    chzzk::HttpClient* getHttpClient() { return httpClient_; }

  private:
    BRLS_BIND(brls::Label, statusLabel, "vod/status");
    BRLS_BIND(brls::RecyclerFrame, recycler, "vod/recycler");

    VodDataSource* dataSource_ = nullptr;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
};
