#include <iostream>
#include <curl/curl.h>
#include <cstring>

#define FROM "e35213706@gmail.com"
#define TO "wiseofraven@gmail.com"
#define USERNAME "e35213706@gmail.com"
#define PASSWORD "" // Replace with your app password
#define SMTP_URL "smtp://smtp.gmail.com:587"

static const char *payload_text = 
    "From: " FROM "\r\n"
    "To: " TO "\r\n"
    "Subject: Test email from C++\r\n"
    "\r\n" // Empty line to separate headers from the body
    "Hello, this is a test email sent from a C++ application using libcurl.\r\n"
    ".\r\n"; // End of email marker (single dot on a line)

size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp) {
    static size_t bytes_remaining = strlen(payload_text);
    if (bytes_remaining == 0) return 0; // No more data to send

    size_t buffer_size = size * nmemb;
    size_t bytes_to_copy = (buffer_size < bytes_remaining) ? buffer_size : bytes_remaining;

    memcpy(ptr, payload_text + (strlen(payload_text) - bytes_remaining), bytes_to_copy);
    bytes_remaining -= bytes_to_copy;

    return bytes_to_copy;
}

int main() {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, SMTP_URL);
        curl_easy_setopt(curl, CURLOPT_USERNAME, USERNAME);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, PASSWORD);
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Enable SSL peer verification
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L); // Enable SSL host verification
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM);

        struct curl_slist *recipients = nullptr;
        recipients = curl_slist_append(recipients, TO);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, nullptr);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        // Enable verbose output for debugging
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "Email sent successfully!" << std::endl;
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize CURL." << std::endl;
    }

    return 0;
}
