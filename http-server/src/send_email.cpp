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

	        const char* email_text = "To: to@example.com\r\n"
                                 "From: from@gmail.com\r\n"
                                 "Subject: Test Email\r\n\r\n"
                                 "This is a test email.";
        curl_easy_setopt(curl, CURLOPT_READDATA, fmemopen((void*)email_text, strlen(email_text), "rb"));
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        //sending email
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
    return 0;
}
