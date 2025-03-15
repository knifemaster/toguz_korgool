#include <curl/curl.h>
#include <iostream>
#include <cstring>
int main() {
    CURL* curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "e35213706@gmail.com");

        struct curl_slist* recipients = NULL;
        recipients = curl_slist_append(recipients, "e35213706@gmail.com");
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_USERNAME, "fe35213706@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "passwor");

        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

