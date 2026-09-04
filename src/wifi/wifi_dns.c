/**
 * Captive DNS — ESP-IDF captive_portal pattern: answer TYPE A with AP IP.
 */
#include "wifi_dns.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "wifi_dns";

#define DNS_PORT 53
#define DNS_BUFSZ 256
#define DNS_TTL 300

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct {
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct __attribute__((packed)) {
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

#define DNS_OPCODE_MASK 0x7800u
#define DNS_QR_FLAG 0x8000u

static int s_sock = -1;
static uint32_t s_ap_ip;
static TaskHandle_t s_task;
static volatile bool s_run;

static char *parse_dns_name(char *raw, char *parsed, size_t parsed_max)
{
    char *label = raw;
    char *out = parsed;
    size_t used = 0;

    do {
        int sub = *label;
        if (sub <= 0 || (size_t)(sub + 1) > parsed_max - used) {
            return NULL;
        }
        memcpy(out, label + 1, (size_t)sub);
        out[sub] = '.';
        out += sub + 1;
        used += (size_t)sub + 1;
        label += sub + 1;
    } while (*label != 0);

    if (used == 0) {
        return NULL;
    }
    parsed[used - 1] = '\0';
    return label + 1;
}

static int build_reply(const uint8_t *req, int req_len, uint8_t *reply, int reply_max)
{
    if (req_len < (int)sizeof(dns_header_t) || req_len > reply_max) {
        return -1;
    }

    memset(reply, 0, (size_t)reply_max);
    memcpy(reply, req, (size_t)req_len);

    dns_header_t *hdr = (dns_header_t *)reply;
    if (ntohs(hdr->flags) & DNS_QR_FLAG) {
        return 0; /* ignore responses */
    }
    if ((ntohs(hdr->flags) & DNS_OPCODE_MASK) != 0) {
        return 0;
    }

    uint16_t qd_count = ntohs(hdr->qd_count);
    if (qd_count == 0) {
        return 0;
    }

    hdr->flags = htons(ntohs(hdr->flags) | DNS_QR_FLAG);
    hdr->an_count = 0;
    hdr->ns_count = 0;
    hdr->ar_count = 0;

    char *qd_ptr = (char *)reply + sizeof(dns_header_t);
    char *ans_ptr = (char *)reply + req_len;
    int ans_count = 0;

    for (uint16_t qi = 0; qi < qd_count; qi++) {
        char name[128];
        char *name_start = qd_ptr;
        char *next = parse_dns_name(qd_ptr, name, sizeof(name));
        if (!next || (next + (int)sizeof(dns_question_t) > (char *)reply + req_len)) {
            return -1;
        }

        dns_question_t *q = (dns_question_t *)next;
        uint16_t qtype = ntohs(q->type);
        uint16_t qclass = ntohs(q->class);
        qd_ptr = next + sizeof(dns_question_t);

        if (qtype != 0x0001) { /* A only */
            continue;
        }
        if ((int)(sizeof(dns_answer_t) + (ans_ptr - (char *)reply)) > reply_max) {
            return -1;
        }

        dns_answer_t *ans = (dns_answer_t *)ans_ptr;
        ans->ptr_offset = htons(0xC000u | (uint16_t)(name_start - (char *)reply));
        ans->type = htons(qtype);
        ans->class = htons(qclass);
        ans->ttl = htonl(DNS_TTL);
        ans->addr_len = htons(4);
        ans->ip_addr = s_ap_ip;
        ans_ptr += sizeof(dns_answer_t);
        ans_count++;
    }

    hdr->an_count = htons((uint16_t)ans_count);
    return ans_count > 0 ? (int)(ans_ptr - (char *)reply) : 0;
}

static void handle_packet(const uint8_t *buf, int len, const struct sockaddr_in *from, socklen_t fromlen)
{
    uint8_t reply[DNS_BUFSZ];
    int rlen = build_reply(buf, len, reply, sizeof(reply));
    if (rlen > 0) {
        sendto(s_sock, reply, (size_t)rlen, 0, (const struct sockaddr *)from, fromlen);
        ESP_LOGD(TAG, "DNS A reply -> %s", inet_ntoa(from->sin_addr));
    }
}

static void dns_task(void *arg)
{
    (void)arg;
    uint8_t buf[DNS_BUFSZ];

    while (s_run) {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        int n = recvfrom(s_sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
        if (n <= 0) {
            if (!s_run) {
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        handle_packet(buf, n, &from, fromlen);

        for (int drained = 0; drained < 16; drained++) {
            fromlen = sizeof(from);
            n = recvfrom(s_sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
            if (n <= 0) {
                break;
            }
            handle_packet(buf, n, &from, fromlen);
        }
        vTaskDelay(0);
    }

    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t wifi_dns_start(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    wifi_dns_stop();
    ip4_addr_t ap;
    IP4_ADDR(&ap, a, b, c, d);
    s_ap_ip = ap.addr;

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
        ESP_LOGE(TAG, "bind :53 errno=%d", errno);
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    int flags = fcntl(s_sock, F_GETFL, 0);
    fcntl(s_sock, F_SETFL, flags | O_NONBLOCK);

    s_run = true;
    if (xTaskCreatePinnedToCore(dns_task, "wifi_dns", 4096, NULL, 5, &s_task, 1) != pdPASS) {
        ESP_LOGE(TAG, "task create");
        s_run = false;
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DNS hijack :53 -> %d.%d.%d.%d", a, b, c, d);
    return ESP_OK;
}

void wifi_dns_stop(void)
{
    s_run = false;
    if (s_sock >= 0) {
        shutdown(s_sock, SHUT_RDWR);
        close(s_sock);
        s_sock = -1;
    }
    for (int i = 0; i < 50 && s_task; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void wifi_dns_poll(void)
{
}
