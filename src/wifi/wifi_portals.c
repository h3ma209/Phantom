#include "wifi_portals.h"

static const char s_iq[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ISP Portal</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
    "background:#0f0f0f;color:#fff;min-height:100vh;display:flex;align-items:center;"
    "justify-content:center;padding:24px}"
    ".card{max-width:360px;width:100%;text-align:center}"
    ".logo{font-size:3rem;line-height:1;margin:0 auto 24px;display:block}"
    "h1{font-size:1.2rem;font-weight:500;margin-bottom:8px;color:#eee}"
    "p{font-size:.875rem;color:#888;margin-bottom:24px}"
    "form{text-align:left}"
    "label{display:block;font-size:.75rem;color:#999;margin-bottom:4px}"
    "input{width:100%;padding:12px;border:1px solid #333;border-radius:8px;"
    "background:#1a1a1a;color:#fff;margin-bottom:16px;font-size:1rem}"
    "button{width:100%;padding:12px;border:0;border-radius:8px;background:#fff;"
    "color:#111;font-size:1rem;font-weight:600}"
    "</style></head><body><div class='card'>"
    "<div class='logo' aria-hidden='true'>&#128246;</div>"
    "<h1>Sign in to your ISP</h1>"
    "<p>Enter your password to connect to the network.</p>"
    "<form action='/' method='get'>"
    "<label>Password</label><input type='password' name='p' "
    "autocomplete='current-password' required>"
    "<button type='submit'>Connect</button>"
    "</form></div></body></html>";

static const char *s_komar =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Google Sign-in</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Sign in with Google</h1>"
    "<p>Connect to access the internet.</p>"
    "</body></html>";

static const char *s_mykomar =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>University Portal</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Welcome to University WiFi</h1>"
    "<p>Connect to access the internet.</p>"
    "</body></html>";

static const char *s_komarcap =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Guest WiFi</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Welcome to Guest WiFi</h1>"
    "<p>Connect to access the internet.</p>"
    "</body></html>";

static const char *s_names[WIFI_PORTAL_COUNT] = {
    "ISP Portal",
    "Google Login",
    "University Portal",
    "Guest Portal",
};

const char *wifi_portal_html(int index)
{
    switch (index) {
    case 1:
        return s_komar;
    case 2:
        return s_mykomar;
    case 3:
        return s_komarcap;
    default:
        return s_iq;
    }
}

const char *wifi_portal_name(int index)
{
    if (index < 0 || index >= WIFI_PORTAL_COUNT) {
        return s_names[0];
    }
    return s_names[index];
}
