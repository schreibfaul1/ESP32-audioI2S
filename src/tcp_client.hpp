#pragma once

#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <esp_timer.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <mbedtls/ssl.h>
#include <stdio.h>

namespace {
constexpr int        SOCKET_INVALID = -1;       ///< parameter for "no descriptor open"
constexpr size_t     PORT_STR_LEN = 8;          ///< buffer for the port as text during name resolution
constexpr uint32_t   MS_PER_SECOND = 1000;      ///< milliseconds per second
constexpr uint32_t   US_PER_MS = 1000;          ///< microseconds per millisecond
constexpr uint32_t   CONNECT_TIMEOUT_MS = 3000; ///< window for establishing a connection, as specified by Arduino
constexpr size_t     RECV_CHUNK_MAX = 1436;     ///< upper limit per recv call; a TCP segment size similar to that of Arduino
constexpr TickType_t SEND_RETRY_DELAY = 1;      ///< wait time in ticks between two transmission attempts when the buffer is full

#define ANSI_ESC_RESET   "\033[0m"
#define ANSI_ESC_BLACK   "\033[30m"
#define ANSI_ESC_RED     "\033[31m"
#define ANSI_ESC_GREEN   "\033[32m"
#define ANSI_ESC_YELLOW  "\033[33m"
#define ANSI_ESC_BLUE    "\033[34m"
#define ANSI_ESC_MAGENTA "\033[35m"
#define ANSI_ESC_CYAN    "\033[36m"
#define ANSI_ESC_WHITE   "\033[37m"
} // namespace

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  N E T W O R K - C L I E N T   📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

class Network_Client {
  public:
    virtual ~Network_Client() = default;
    virtual bool connect(const char* host, uint16_t port, uint32_t timeoutMs = 0) = 0;
    virtual void stop() = 0;
    virtual bool connected() = 0;
    virtual int  available() = 0;
    virtual int  read() = 0;
    virtual int  read(uint8_t* buf, size_t len) = 0;
    virtual int  print(const char* str) = 0;
    virtual void setTimeout(uint32_t ms) = 0;
};

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  T C P - C L I E N T   📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

class TCP_Client : public Network_Client {
  private:
    int      m_fd = -1;         ///< Socket-Deskriptor, -1 wenn keine Verbindung offen ist
    uint32_t m_timeoutMs = 250; ///< Zeitgrenze für Lesen und Schreiben in Millisekunden

  public:
    TCP_Client() = default;
    ~TCP_Client() override { stop(); };

    TCP_Client(const TCP_Client&) = delete;
    TCP_Client& operator=(const TCP_Client&) = delete;
    TCP_Client(TCP_Client&&) = delete;
    TCP_Client& operator=(TCP_Client&&) = delete;

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    bool connect(const char* host, uint16_t port, uint32_t timeoutMs = 0) override {

        if (!host || host[0] == '\0') {
            printf("[%s:%i]" ANSI_ESC_RED " connect without host name" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
            return false;
        }

        stop();

        const uint32_t effectiveTimeout = (timeoutMs > 0) ? timeoutMs : CONNECT_TIMEOUT_MS;

        char      portStr[PORT_STR_LEN];
        const int written = snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));
        if (written < 0 || static_cast<size_t>(written) >= sizeof(portStr)) {
            printf("[%s:%i]" ANSI_ESC_RED " Port %u is invalid" ANSI_ESC_RESET "\n", __FILE__, __LINE__, static_cast<unsigned>(port));
            return false;
        }

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* res = nullptr;
        if (lwip_getaddrinfo(host, portStr, &hints, &res) != 0 || !res) {
            printf("[%s:%i]" ANSI_ESC_RED " unknown host: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host);
            return false;
        }

        m_fd = lwip_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_fd == SOCKET_INVALID) {
            printf("[%s:%i]" ANSI_ESC_RED " socket error, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
            lwip_freeaddrinfo(res);
            return false;
        }

        const int savedFlags = lwip_fcntl(m_fd, F_GETFL, 0);
        lwip_fcntl(m_fd, F_SETFL, savedFlags | O_NONBLOCK);

        int rc = lwip_connect(m_fd, res->ai_addr, res->ai_addrlen);
        lwip_freeaddrinfo(res);

        if (rc < 0 && errno != EINPROGRESS) {
            printf("[%s:%i]" ANSI_ESC_RED " connection to \"%s:%u\" failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host, static_cast<unsigned>(port), errno);
            stop();
            return false;
        }

        if (rc < 0) {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(m_fd, &writeSet);

            struct timeval tv;
            tv.tv_sec = effectiveTimeout / MS_PER_SECOND;
            tv.tv_usec = (effectiveTimeout % MS_PER_SECOND) * US_PER_MS;

            rc = lwip_select(m_fd + 1, nullptr, &writeSet, nullptr, &tv);
            if (rc <= 0) {
                printf("[%s:%i]" ANSI_ESC_RED " timeout, host \"%s:%u\" ist not responding after %u ms" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host, static_cast<unsigned>(port),
                       static_cast<unsigned>(effectiveTimeout));
                stop();
                return false;
            }

            int       soError = 0;
            socklen_t soErrorLen = sizeof(soError);
            if (lwip_getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &soError, &soErrorLen) < 0 || soError != 0) {
                printf("[%s:%i]" ANSI_ESC_RED " connection to %s:%u rejected, SO_ERROR %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host, static_cast<unsigned>(port), soError);
                stop();
                return false;
            }
        }

        lwip_fcntl(m_fd, F_SETFL, savedFlags);

        if (!applySocketOptions()) {
            stop();
            return false;
        }

        printf("[%s:%i]" ANSI_ESC_GREEN "connected with %s:%u" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host, static_cast<unsigned>(port));
        return true;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    void stop() override {
        if (m_fd == SOCKET_INVALID) return;

        lwip_shutdown(m_fd, SHUT_RDWR);
        uint8_t drainBuf[64];
        int     guard = 0;
        while (lwip_recv(m_fd, drainBuf, sizeof(drainBuf), MSG_DONTWAIT) > 0 && ++guard < 512) {}

        lwip_close(m_fd);
        m_fd = SOCKET_INVALID;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int available() override {
        if (m_fd == SOCKET_INVALID) return 0;

        int count = 0;
        if (lwip_ioctl(m_fd, FIONREAD, &count) < 0) return 0;

        return count;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int read() override {
        uint8_t   b = 0;
        const int rc = read(&b, 1);
        if (rc != 1) return -1;

        return static_cast<int>(b);
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int read(uint8_t* buf, size_t len) override {
        if (m_fd == SOCKET_INVALID || !buf || len == 0) return 0;

        if (len > RECV_CHUNK_MAX) len = RECV_CHUNK_MAX;

        const int rc = lwip_recv(m_fd, buf, len, MSG_DONTWAIT);
        if (rc >= 0) return rc;

        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;

        printf("[%s:%i]" ANSI_ESC_RED " recv failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
        return -1;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int print(const char* str) override {
        if (m_fd == SOCKET_INVALID || !str) return -1;

        const size_t total = strlen(str);
        size_t       sent = 0;

        while (sent < total) {
            const int rc = lwip_send(m_fd, str + sent, total - sent, 0);
            if (rc > 0) {
                sent += static_cast<size_t>(rc);
                continue;
            }

            if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                printf("[%s:%i]" ANSI_ESC_RED " send timeout after %u from %u bytes" ANSI_ESC_RESET "\n", __FILE__, __LINE__, static_cast<unsigned>(sent), static_cast<unsigned>(total));
                return -1;
            }

            printf("[%s:%i]" ANSI_ESC_RED " send failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
            return -1;
        }

        return static_cast<int>(sent);
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    void setTimeout(uint32_t ms) override {
        m_timeoutMs = ms;

        if (m_fd != SOCKET_INVALID) applySocketOptions();
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
  private:
    bool applySocketOptions() {
        if (m_fd == SOCKET_INVALID) return false;

        struct timeval tv;
        tv.tv_sec = m_timeoutMs / MS_PER_SECOND;
        tv.tv_usec = (m_timeoutMs % MS_PER_SECOND) * US_PER_MS;

        if (lwip_setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            printf("[%s:%i]" ANSI_ESC_RED " SO_RCVTIMEO failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
            return false;
        }

        if (lwip_setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            printf("[%s:%i]" ANSI_ESC_RED " SO_SNDTIMEO failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
            return false;
        }

        // Without TCP_NODELAY, the stack aggregates short request headers and delays them by up to 200 ms
        int flag = 1;
        if (lwip_setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) { //
            printf("[%s:%i]" ANSI_ESC_RED " TCP_NODELAY failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
        }

        return true;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
  public:
    bool connected() override {
        if (m_fd == SOCKET_INVALID) return false;

        uint8_t   dummy = 0;
        const int rc = lwip_recv(m_fd, &dummy, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rc > 0) return true;

        switch (errno) {
            case ENOTCONN:
            case EPIPE:
            case ECONNRESET:
            case ECONNREFUSED:
            case ECONNABORTED: return false;
            default: return true;
        }
    }
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  T C P - C L I E N T S E C U R E   📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

class TCP_ClientSecure : public Network_Client {
  private:
    uint32_t m_timeoutMs = 250;    ///< Zeitgrenze für Lesen und Schreiben in Millisekunden
    esp_tls* m_tls = nullptr;      ///< esp-tls-Handle, nullptr wenn keine Verbindung offen ist
    bool     m_fatalError = false; ///< true nach einem harten TLS-Fehler, connected() meldet dann false

  public:
    TCP_ClientSecure() = default;
    ~TCP_ClientSecure() override { stop(); };

    TCP_ClientSecure(const TCP_ClientSecure&) = delete;
    TCP_ClientSecure& operator=(const TCP_ClientSecure&) = delete;
    TCP_ClientSecure(TCP_ClientSecure&&) = delete;
    TCP_ClientSecure& operator=(TCP_ClientSecure&&) = delete;

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    bool connect(const char* host, uint16_t port, uint32_t timeoutMs = 0) override {

        if (!host || host[0] == '\0') {
            printf("[%s:%i]" ANSI_ESC_RED " connect without host name" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
            return false;
        }

        stop();
        const uint32_t effectiveTimeout = (timeoutMs > 0) ? timeoutMs : CONNECT_TIMEOUT_MS;

        m_tls = esp_tls_init();
        if (!m_tls) {
            printf("[%s:%i]" ANSI_ESC_RED " esp_tls_init out of memory" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
            return false;
        }

        esp_tls_cfg_t cfg = {};
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.timeout_ms = static_cast<int>(effectiveTimeout);

        const int rc = esp_tls_conn_new_sync(host, static_cast<int>(strlen(host)), port, &cfg, m_tls);
        if (rc != 1) {
            int                    tlsCode = 0;
            int                    certFlags = 0;
            esp_tls_error_handle_t errHandle = nullptr;
            if (esp_tls_get_error_handle(m_tls, &errHandle) == ESP_OK && errHandle) esp_tls_get_and_clear_last_error(errHandle, &tlsCode, &certFlags);
            printf("[%s:%i]" ANSI_ESC_RED " Failed to establish a TLS connection to %s:%u, mbedtls -0x%04x, Certificate Flags 0x%x" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host,
                   static_cast<unsigned>(port), static_cast<unsigned>(-tlsCode), static_cast<unsigned>(certFlags));
            stop();
            return false;
        }

        int fd = -1;
        if (esp_tls_get_conn_sockfd(m_tls, &fd) != ESP_OK || fd < 0) {
            printf("[%s:%i]" ANSI_ESC_RED " TLS socket descriptor cannot be queried" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
            stop();
            return false;
        }
        const int flags = lwip_fcntl(fd, F_GETFL, 0);
        lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        printf("[%s:%i]" ANSI_ESC_GREEN "connected with %s:%u" ANSI_ESC_RESET "\n", __FILE__, __LINE__, host, static_cast<unsigned>(port));
        return true;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    void stop() override {

        if (!m_tls) { return; }

        esp_tls_conn_destroy(m_tls);
        m_tls = nullptr;
        m_fatalError = false;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int available() override {

        if (!m_tls) return 0;

        auto* ssl = static_cast<mbedtls_ssl_context*>(esp_tls_get_ssl_context(m_tls));
        if (!ssl) return 0;

        unsigned char dummy = 0;
        const int     rc = mbedtls_ssl_read(ssl, &dummy, 0);
        if (rc < 0 && rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE && rc != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            if (!m_fatalError) printf("[%s:%i]" ANSI_ESC_RED " TLS read error, mbedtls -0x%04x" ANSI_ESC_RESET "\n", __FILE__, __LINE__, static_cast<unsigned>(-rc));
            m_fatalError = true;
            return 0;
        }

        return static_cast<int>(mbedtls_ssl_get_bytes_avail(ssl));
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int read() override {
        uint8_t   b = 0;
        const int rc = read(&b, 1);
        if (rc != 1) return -1;

        return static_cast<int>(b);
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int read(uint8_t* buf, size_t len) override {
        if (!m_tls || !buf || len == 0) return 0;

        const ssize_t rc = esp_tls_conn_read(m_tls, buf, len);
        if (rc > 0) return static_cast<int>(rc);

        if (rc == 0 || rc == ESP_TLS_ERR_SSL_WANT_READ || rc == ESP_TLS_ERR_SSL_WANT_WRITE || rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) { //
            return 0;
        }
        if (!m_fatalError) { //
            printf("[%s:%i]" ANSI_ESC_RED " TLS read error, mbedtls -0x%04x" ANSI_ESC_RESET "\n", __FILE__, __LINE__, static_cast<unsigned>(-rc));
        }
        m_fatalError = true;
        return -1;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    int print(const char* str) override {

        if (!m_tls || !str) return -1;

        const size_t  total = strlen(str);
        size_t        sent = 0;
        const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(m_timeoutMs) * US_PER_MS;

        while (sent < total) {
            const ssize_t rc = esp_tls_conn_write(m_tls, str + sent, total - sent);
            if (rc > 0) {
                sent += static_cast<size_t>(rc);
                continue;
            }

            if (rc == 0 || rc == ESP_TLS_ERR_SSL_WANT_READ || rc == ESP_TLS_ERR_SSL_WANT_WRITE) {
                if (esp_timer_get_time() >= deadline) {
                    printf("[%s:%i]" ANSI_ESC_RED " send timeout after %u from %u bytes" ANSI_ESC_RESET "\n", __FILE__, __LINE__, static_cast<unsigned>(sent), static_cast<unsigned>(total));
                    return -1;
                }
                vTaskDelay(SEND_RETRY_DELAY);
                continue;
            }

            if (!m_fatalError) { //
                printf("[%s:%i]" ANSI_ESC_RED " send failed, errno %d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, errno);
            }
            m_fatalError = true;
            return -1;
        }

        return static_cast<int>(sent);
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    void setTimeout(uint32_t ms) override { //
        m_timeoutMs = ms;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
  public:
    bool connected() override { //
        return m_tls != nullptr && !m_fatalError;
    }
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
