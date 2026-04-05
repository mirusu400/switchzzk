#include "activity/main_activity.hpp"

#include <cstdio>

#include "view/custom_button.hpp"

extern std::string g_nid_aut;
extern std::string g_nid_ses;
extern void dbg(const char* msg);

void MainActivity::onContentAvailable() {
    auto* loginBtn   = dynamic_cast<CustomButton*>(this->getView("main/login"));
    auto* settingBtn = dynamic_cast<CustomButton*>(this->getView("main/setting"));

    if (loginBtn) {
        loginBtn->registerClickAction([this](brls::View*) {
            this->showLoginDialog();
            return true;
        });
        loginBtn->addGestureRecognizer(new brls::TapGestureRecognizer(loginBtn));
    }

    if (settingBtn) {
        settingBtn->registerClickAction([this](brls::View*) {
            this->showSettingsDialog();
            return true;
        });
        settingBtn->addGestureRecognizer(new brls::TapGestureRecognizer(settingBtn));
    }
}

void MainActivity::showSettingsDialog() {
    std::string body =
        "Switchzzk v0.3.0\n"
        "치지직 라이브 스트리밍 for Nintendo Switch\n\n"
        "좌측 하단 로그인 버튼으로 NID_AUT / NID_SES 쿠키 입력";
    auto* dialog = new brls::Dialog(body);
    dialog->addButton("확인", [dialog]() { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void MainActivity::showLoginDialog() {
    brls::Application::getImeManager()->openForText(
        [](std::string nidAut) {
            if (nidAut.empty()) return;
            brls::Application::getImeManager()->openForText(
                [nidAut](std::string nidSes) {
                    if (nidSes.empty()) return;
                    g_nid_aut = nidAut;
                    g_nid_ses = nidSes;
                    FILE* f = fopen("sdmc:/switch/switchzzk_auth.txt", "w");
                    if (f) {
                        fprintf(f, "%s\n%s\n", g_nid_aut.c_str(), g_nid_ses.c_str());
                        fclose(f);
                    }
                    dbg("auth: saved cookies via login button");
                    auto* ok = new brls::Dialog("로그인 정보 저장됨. 팔로잉 탭에서 새로고침(X) 해주세요.");
                    ok->addButton("확인", [ok]() { ok->close(); });
                    ok->setCancelable(true);
                    ok->open();
                },
                "NID_SES 입력", "PC 브라우저에서 복사", 500, "");
        },
        "NID_AUT 입력", "PC chzzk.naver.com 쿠키에서 복사", 500, "");
}
