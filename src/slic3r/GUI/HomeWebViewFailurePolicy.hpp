#ifndef slic3r_GUI_HomeWebViewFailurePolicy_hpp_
#define slic3r_GUI_HomeWebViewFailurePolicy_hpp_

namespace Slic3r { namespace GUI {

// Keep this policy independent of wxWidgets so the retained-page behaviour can
// be covered by a fast unit test.  WebViewDialog.cpp maps wxWebView failures to
// these categories and owns the localized notification copy.
enum class HomeWebFailureKind {
    Network,
    CloudAuthentication,
    RouteNotFound,
    SecureConnection,
    ServiceUnavailable,
    UserCancelled,
};

struct HomeWebFailureDecision {
    bool retain_current_page {true};
    bool show_notification {true};
    bool offer_retry {true};
};

constexpr HomeWebFailureDecision home_web_failure_decision(HomeWebFailureKind kind)
{
    if (kind == HomeWebFailureKind::UserCancelled)
        return {true, false, false};
    return {true, true, true};
}

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_HomeWebViewFailurePolicy_hpp_
