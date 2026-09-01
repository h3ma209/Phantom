#include "wifi_portals.h"

static const char *s_iq =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>IQ Portal</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Welcome to My IQ Portal!</h1>"
    "<p>Connect to access the internet.</p>"
    "<p><a href='/'>Continue</a></p>"
    "</body></html>";

static const char *s_komar =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Komar Portal</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Welcome to My Komar Portal!</h1>"
    "<p>Connect to access the internet.</p>"
    "</body></html>";

static const char *s_mykomar =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>MyKomar Portal</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Welcome to Komar wifi Portal!</h1>"
    "<p>Connect to access the internet.</p>"
    "</body></html>";

static const char *s_komarcap =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Komar Cap</title></head>"
    "<body style='font-family:sans-serif;padding:24px'>"
    "<h1>Welcome to Komar Cap Portal!</h1>"
    "<p>Connect to access the internet.</p>"
    "</body></html>";

static const char *s_names[WIFI_PORTAL_COUNT] = {
    "IQ Login",
    "Google Login",
    "MyKomar",
    "Komar Cap",
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
