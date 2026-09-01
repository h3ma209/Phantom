/**
 * Captive DNS: answer every A query with AP IPv4 (pengz0 DNSServer "*").
 */
#include "wifi_dns.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

static const char *TAG = "wifi_dns";

#define DNS_PORT 53
#define DNS_BUFSZ 512

static int s_sock = -1;
static uint8_t s_octets[4];

static int build_a_reply(const uint8_t *q, int qlen, uint8_t *out, int outmax)
{
    if (qlen < 12 || qlen > 300 || outmax < qlen + 16) {
        return -1;
    }

    memcpy(out, q, qlen);
    out[2] = (uint8_t)(0x80 | (q[2] & 0x01)); /* QR + RD */
    out[3] = 0x80;                            /* RA */

    /* QDCOUNT stays; ANCOUNT = 1 */
    out[6] = 0;
    out[7] = 1;
    out[8] = 0;
    out[9] = 0;
    out[10] = 0;
    out[11] = 0;

    int p = qlen;
    /* pointer to question name at 0x0C */
    out[p++] = 0xC0;
    out[p++] = 0x0C;
    out[p++] = 0x00;
    out[p++] = 0x01; /* A */
    out[p++] = 0x00;
    out[p++] = 0x01; /* IN */
    out[p++] = 0x00;
    out[p++] = 0x00;
    out[p++] = 0x00;
    out[p++] = 30; /* TTL */
    out[p++] = 0x00;
    out[p++] = 0x04;
    out[p++] = s_octets[0];
    out[p++] = s_octets[1];
    out[p++] = s_octets[2];
    out[p++] = s_octets[3];
    return p;
}

esp_err_t wifi_dns_start(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    wifi_dns_stop();
    s_octets[0] = a;
    s_octets[1] = b;
    s_octets[2] = c;
    s_octets[3] = d;

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "socket");
        return ESP_FAIL;
    }

    int yes = 1;
    setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind :53");
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    int flags = fcntl(s_sock, F_GETFL, 0);
    fcntl(s_sock, F_SETFL, flags | O_NONBLOCK);
    ESP_LOGI(TAG, "DNS hijack on :53");
    return ESP_OK;
}

void wifi_dns_stop(void)
{
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}

void wifi_dns_poll(void)
{
    if (s_sock < 0) {
        return;
    }

    uint8_t buf[DNS_BUFSZ];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int n = recvfrom(s_sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    if (n <= 0) {
        return;
    }

    uint8_t reply[DNS_BUFSZ];
    int rlen = build_a_reply(buf, n, reply, sizeof(reply));
    if (rlen > 0) {
        sendto(s_sock, reply, rlen, 0, (struct sockaddr *)&from, fromlen);
    }
}
