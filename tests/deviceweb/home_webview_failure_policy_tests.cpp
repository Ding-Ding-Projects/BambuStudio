#include <catch_main.hpp>

#include "slic3r/GUI/HomeWebViewFailurePolicy.hpp"

using Slic3r::GUI::HomeWebFailureKind;
using Slic3r::GUI::home_web_failure_decision;

TEST_CASE("cloud web failures retain the last usable page and offer retry", "[deviceweb][webview]")
{
    for (const HomeWebFailureKind kind : {
             HomeWebFailureKind::Network,
             HomeWebFailureKind::CloudAuthentication,
             HomeWebFailureKind::RouteNotFound,
             HomeWebFailureKind::SecureConnection,
             HomeWebFailureKind::ServiceUnavailable,
         }) {
        const auto decision = home_web_failure_decision(kind);
        CHECK(decision.retain_current_page);
        CHECK(decision.show_notification);
        CHECK(decision.offer_retry);
    }
}

TEST_CASE("cancelled cloud navigation does not raise a false error", "[deviceweb][webview]")
{
    const auto decision = home_web_failure_decision(HomeWebFailureKind::UserCancelled);
    CHECK(decision.retain_current_page);
    CHECK_FALSE(decision.show_notification);
    CHECK_FALSE(decision.offer_retry);
}
