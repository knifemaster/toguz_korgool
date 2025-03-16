class EmailSender {
public:
    EmailSender(const std::string& from, const std::string& to, const std::string& username, const std::string& password)
        : from_(from), to_(to), username_(username), password_(password) {}

    bool send(const std::string& subject, const std::string& body) {
        CURL* curl;
        CURLcode res;

        curl = curl_easy_init();
        if (curl) {
            std::string payload_text = "From: " + from_ + "\r\n"
                                    "To: " + to_ + "\r\n"
                                    "Subject: " + subject + "\r\n"
                                    "\r\n"
                                    + body + "\r\n"
                                    ".\r\n"; // Конец письма

            curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");
            curl_easy_setopt(curl, CURLOPT_USERNAME, username_.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
            curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from_.c_str());

            struct curl_slist* recipients = nullptr;
            recipients = curl_slist_append(recipients, to_.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

            curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
            curl_easy_setopt(curl, CURLOPT_READDATA, &payload_text);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);


            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                curl_slist_free_all(recipients);
                curl_easy_cleanup(curl);
                return false;
            }

            std::cout << "Email sent successfully!" << std::endl;
            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);
            return true;
        } else {
            std::cerr << "Failed to initialize CURL." << std::endl;
            return false;
        }
    }

private:
    std::string from_;     
    std::string to_;       
    std::string username_; 
    std::string password_; 


    static size_t payload_source(void* ptr, size_t size, size_t nmemb, void* userp) {
        std::string* payload = static_cast<std::string*>(userp);
        if (payload->empty()) return 0;

        size_t buffer_size = size * nmemb;
        size_t bytes_to_copy = (buffer_size < payload->size()) ? buffer_size : payload->size();

        memcpy(ptr, payload->c_str(), bytes_to_copy);
        payload->erase(0, bytes_to_copy);

        return bytes_to_copy;
    }
};

