#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <strings.h>   // for strncasecmp
#include "include/sans.h"

#define MAX_BUFFER_SIZE 1024

int http_client(const char* host, int port) {
    // Variables for input parsing
    char input[MAX_BUFFER_SIZE];
    char method[10];
    char path[256];

    // Network buffers - limited to 1024 bytes as required
    char request[MAX_BUFFER_SIZE];
    char response[MAX_BUFFER_SIZE];

    // Network variables
    int socket_fd;
    int bytes_sent;
    int bytes_received;

    // Step 1: Read user input (method and path)
    if (!fgets(input, sizeof(input), stdin)) {
        fprintf(stderr, "Failed to read input\n");
        return -1;
    }

    // Remove newline character
    char* newline = strchr(input, '\n');
    if (newline) *newline = '\0';

    // Parse method and path from input
    if (sscanf(input, "%9s %255s", method, path) != 2) {
        fprintf(stderr, "Invalid input format\n");
        return -1;
    }

    // Step 2: Connect to server using sans_connect
    socket_fd = sans_connect(host, port, IPPROTO_TCP);
    if (socket_fd < 0) {
        fprintf(stderr, "Connection failed\n");
        return -1;
    }

    // Normalize path: avoid double slashes if user already gave leading '/'
    const char* normalized_path = path;
    if (normalized_path[0] == '/') normalized_path++;

    // Step 3: Build HTTP request with all required headers
    int request_length = snprintf(request, MAX_BUFFER_SIZE,
        "%s /%s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: sans/1.0\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Accept: */*\r\n"
        "\r\n",
        method, normalized_path, host, port);

    if (request_length >= MAX_BUFFER_SIZE) {
        fprintf(stderr, "Request too large\n");
        sans_disconnect(socket_fd);
        return -1;
    }

    // Step 4: Send HTTP request
    bytes_sent = sans_send_pkt(socket_fd, request, request_length);
    if (bytes_sent < 0) {
        fprintf(stderr, "Send failed\n");
        sans_disconnect(socket_fd);
        return -1;
    }

    // Step 5: Receive response with precise Content-Length handling
    char full_response[4096] = {0};   // temporary accumulator to locate header end
    int total_received = 0;
    int header_end_pos = -1;

    // Phase A: Read until end of headers (\r\n\r\n)
    while (1) {
        bytes_received = sans_recv_pkt(socket_fd, response, MAX_BUFFER_SIZE - 1);
        if (bytes_received <= 0) break;

        response[bytes_received] = '\0';
        // Print as we go (headers + any body that may have arrived)
        printf("%s", response);

        // Accumulate for header parsing (bounded copy)
        int can_copy = (int)sizeof(full_response) - 1 - total_received;
        if (can_copy > 0) {
            int to_copy = bytes_received < can_copy ? bytes_received : can_copy;
            memcpy(full_response + total_received, response, to_copy);
            total_received += to_copy;
            full_response[total_received] = '\0';
        }

        // Check for end of headers in the accumulated buffer
        if (header_end_pos == -1) {
            char* header_end_ptr = strstr(full_response, "\r\n\r\n");
            if (header_end_ptr) {
                header_end_pos = (int)(header_end_ptr - full_response) + 4;
                break; // stop Phase A as soon as headers are found
            }
        }
    }

    int content_length = -1;

    // Parse Content-Length (case-insensitive) if we found headers
    if (header_end_pos != -1) {
        // Extract just the headers region
        int headers_len = header_end_pos;
        if (headers_len > (int)sizeof(full_response)) headers_len = (int)sizeof(full_response);

        // Walk header lines
        int i = 0;
        while (i < headers_len) {
            // find end of this line
            int line_start = i;
            while (i + 1 < headers_len && !(full_response[i] == '\r' && full_response[i+1] == '\n')) {
                i++;
            }

            int line_len = i - line_start;
            if (line_len > 0) {
                // Compare prefix "Content-Length:"
                const char* line_ptr = full_response + line_start;
                const char* key = "Content-Length:";
                size_t key_len = strlen(key);

                if (line_len >= (int)key_len && strncasecmp(line_ptr, key, key_len) == 0) {
                    // Advance past key and optional whitespace
                    const char* p = line_ptr + key_len;
                    while ((p < full_response + headers_len) && (*p == ' ' || *p == '\t')) p++;
                    content_length = atoi(p);
                    break;
                }
            }

            // skip CRLF
            if (i + 1 < headers_len && full_response[i] == '\r' && full_response[i+1] == '\n') {
                i += 2;
            } else {
                // malformed header line ending; bail
                break;
            }
        }
    }

    if (header_end_pos == -1) {
        // Didn't find headers at all: just drain connection
        while ((bytes_received = sans_recv_pkt(socket_fd, response, MAX_BUFFER_SIZE - 1)) > 0) {
            response[bytes_received] = '\0';
            printf("%s", response);
        }
    } else if (content_length >= 0) {
        // Phase B: We know exactly how many body bytes to read after headers.

        // Count how many body bytes are already in our accumulator (could be >= 0)
        int already_have = total_received - header_end_pos;
        if (already_have < 0) already_have = 0;
        if (already_have > content_length) already_have = content_length;

        int remaining = content_length - already_have;

        // Now read exactly 'remaining' bytes, controlling recv size so the number
        // of sans_recv_pkt calls is driven by remaining data.
        while (remaining > 0) {
            int to_read = remaining;
            if (to_read > (MAX_BUFFER_SIZE - 1)) to_read = MAX_BUFFER_SIZE - 1;

            bytes_received = sans_recv_pkt(socket_fd, response, to_read);
            if (bytes_received <= 0) break;

            response[bytes_received] = '\0';
            printf("%s", response);

            remaining -= bytes_received;
        }
        // (If server sends more after body despite Connection: close, we deliberately
        // ignore it to satisfy the exact-Content-Length requirement.)
    } else {
        // No Content-Length: read until close
        while ((bytes_received = sans_recv_pkt(socket_fd, response, MAX_BUFFER_SIZE - 1)) > 0) {
            response[bytes_received] = '\0';
            printf("%s", response);
        }
    }

    // Step 6: Disconnect
    int disconnect_result = sans_disconnect(socket_fd);
    if (disconnect_result < 0) {
        return -1;
    }

    return 0;
}
