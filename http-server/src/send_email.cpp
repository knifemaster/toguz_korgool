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

