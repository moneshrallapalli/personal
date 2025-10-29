// src/http_server.c
#include <stdio.h>
#include <string.h>     // only using strlen/strncmp as allowed
#include <sys/stat.h>   // stat
#include <netinet/in.h> // IPPROTO_TCP
#include "include/sans.h"

#define MAX_BUF 1024

// minimal helper: check if ".." appears in path
static int contains_dotdot(const char *s) {
    if (!s) return 0;
    for (int i = 0; s[i] && s[i+1]; i++) {
        if (s[i] == '.' && s[i+1] == '.') return 1;
    }
    return 0;
}

// minimal helper: strip a single leading '/' (so "/a.html" -> "a.html")
static const char* strip_leading_slash(const char *p) {
    if (p && p[0] == '/') return p + 1;
    return p;
}

// REQUIRED SIGNATURE (adjust if your header declares differently)
int http_server(const char* host, int port) {
    // 1) Accept exactly one connection
    int sock = sans_accept(host, port, IPPROTO_TCP);
    if (sock < 0) {
        // Not graded on prints, but helpful during dev
        printf("accept failed\n");
        return -1;
    }

    // 2) Receive a single request (fits in one 1024-byte packet by spec)
    char req[MAX_BUF];
    int nread = sans_recv_pkt(sock, req, MAX_BUF - 1);
    if (nread <= 0) {
        sans_disconnect(sock);
        return 0;
    }
    req[nread] = '\0';

    // 3) Parse request line: "GET <path> HTTP/1.1"
    char method[8] = {0};
    char raw_path[512] = {0};
    char version[16] = {0};
    // Using sscanf is allowed
    if (sscanf(req, "%7s %511s %15s", method, raw_path, version) != 3) {
        // Malformed → treat like 404 per minimal spec
        // (Only GET is required; anything else -> 404)
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        char hdr[MAX_BUF];
        int hlen = snprintf(hdr, MAX_BUF,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n",
            (int)strlen(body));
        if (hlen > MAX_BUF) hlen = MAX_BUF;
        sans_send_pkt(sock, hdr, hlen);
        sans_send_pkt(sock, body, (int)strlen(body));
        sans_disconnect(sock);
        return 0;
    }

    // Only GET is supported
    if (strncmp(method, "GET", 3) != 0) {
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        char hdr[MAX_BUF];
        int hlen = snprintf(hdr, MAX_BUF,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n",
            (int)strlen(body));
        if (hlen > MAX_BUF) hlen = MAX_BUF;
        sans_send_pkt(sock, hdr, hlen);
        sans_send_pkt(sock, body, (int)strlen(body));
        sans_disconnect(sock);
        return 0;
    }

    // Handle "/" as "index.html" so browsers work as hinted in the prompt
    const char *use_path = raw_path;
    if (raw_path[0] == '/' && raw_path[1] == '\0') {
        use_path = "index.html";
    } else {
        use_path = strip_leading_slash(raw_path);
    }

    // Minimal sanitization recommended in the writeup:
    // reject any ".." to avoid leaving current dir
    if (contains_dotdot(use_path) || use_path[0] == '\0') {
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        char hdr[MAX_BUF];
        int hlen = snprintf(hdr, MAX_BUF,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n",
            (int)strlen(body));
        if (hlen > MAX_BUF) hlen = MAX_BUF;
        sans_send_pkt(sock, hdr, hlen);
        sans_send_pkt(sock, body, (int)strlen(body));
        sans_disconnect(sock);
        return 0;
    }

    // 4) Try to stat/open the file
    struct stat st;
    if (stat(use_path, &st) != 0) {
        // 404
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        char hdr[MAX_BUF];
        int hlen = snprintf(hdr, MAX_BUF,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n",
            (int)strlen(body));
        if (hlen > MAX_BUF) hlen = MAX_BUF;
        sans_send_pkt(sock, hdr, hlen);
        sans_send_pkt(sock, body, (int)strlen(body));
        sans_disconnect(sock);
        return 0;
    }

    FILE *fp = fopen(use_path, "rb");
    if (!fp) {
        // 404 if cannot open
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        char hdr[MAX_BUF];
        int hlen = snprintf(hdr, MAX_BUF,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n",
            (int)strlen(body));
        if (hlen > MAX_BUF) hlen = MAX_BUF;
        sans_send_pkt(sock, hdr, hlen);
        sans_send_pkt(sock, body, (int)strlen(body));
        sans_disconnect(sock);
        return 0;
    }

    // 5) Send HTTP/1.1 200 OK with headers
    // Content-Type must be exactly "text/html; charset=utf-8" per spec
    // Content-Length must be the exact file size in bytes
    char hdr[MAX_BUF];
    // st.st_size is off_t; cast to int for snprintf arg that we constrain to spec
    int content_len = (int)st.st_size;
    int hlen = snprintf(hdr, MAX_BUF,
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n",
        content_len);
    if (hlen > MAX_BUF) hlen = MAX_BUF;
    sans_send_pkt(sock, hdr, hlen);

    // 6) Stream the file in ≤1024-byte chunks until all bytes are sent
    char buf[MAX_BUF];
    for (;;) {
        size_t r = fread(buf, 1, MAX_BUF, fp);
        if (r == 0) break; // EOF or read error treated as EOF
        // Each packet must be ≤ 1024 bytes
        sans_send_pkt(sock, buf, (int)r);
        if (r < MAX_BUF) break; // EOF
    }
    fclose(fp);

    // 7) Close connection
    sans_disconnect(sock);
    return 0;
}
