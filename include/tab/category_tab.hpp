#pragma once

#include <borealis.hpp>
#include "chzzk/chzzk_client.hpp"
#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"
#include "tab/live_tab.hpp"  // LiveCell 재사용

class CategoryCell : public brls::RecyclerCell {
  public:
    CategoryCell();
    static CategoryCell* create();
    void setData(const chzzk::CategoryInfo& info);

    BRLS_BIND(brls::Label, nameLabel, "cat/name");
    BRLS_BIND(brls::Label, viewerLabel, "cat/viewers");
};

class CategoryTab;

// 카테고리 목록 데이터소스
class CategoryDataSource : public brls::RecyclerDataSource {
  public:
    explicit CategoryDataSource(CategoryTab* tab) : tab_(tab) {}
    void setData(std::vector<chzzk::CategoryInfo> cats);

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;

  private:
    std::vector<chzzk::CategoryInfo> cats_;
    CategoryTab* tab_;
};

// 카테고리 내 라이브 목록 데이터소스
class CategoryLiveDataSource : public brls::RecyclerDataSource {
  public:
    explicit CategoryLiveDataSource(CategoryTab* tab) : tab_(tab) {}
    void setData(std::vector<chzzk::LiveInfo> lives);
    const chzzk::LiveInfo& getItem(int index) const;

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    float heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;

  private:
    std::vector<chzzk::LiveInfo> lives_;
    CategoryTab* tab_;
};

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
    BRLS_BIND(brls::Label, statusLabel, "cat/status");
    BRLS_BIND(brls::RecyclerFrame, recycler, "cat/recycler");

    CategoryDataSource* catDataSource_ = nullptr;
    CategoryLiveDataSource* liveDataSource_ = nullptr;
    chzzk::HttpsHttpClient* httpClient_ = nullptr;
    chzzk::ChzzkClient* chzzkClient_ = nullptr;
    bool showingLives_ = false;
};
