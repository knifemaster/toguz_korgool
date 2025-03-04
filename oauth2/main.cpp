#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Функция для обработки HTTP-ответа
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch (std::bad_alloc& e) {
        return 0;
    }
    return newLength;
}

// Функция для выполнения HTTP-запроса
std::string performHttpRequest(const std::string& url, const std::string& postData = "") {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        if (!postData.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

int main() {
    // Замените на свои значения
    std::string client_id = "client_id";
    std::string client_secret = "client_secret";
    //std::string redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
    std::string redirect_uri = "http://localhost:5173";

    
    std::string auth_code;

    // Шаг 1: Получение кода авторизации
    std::string auth_url = "https://accounts.google.com/o/oauth2/auth?";
    auth_url += "client_id=" + client_id;
    auth_url += "&redirect_uri=" + redirect_uri;
    auth_url += "&response_type=code";
    auth_url += "&scope=email profile";
    auth_url += "&access_type=offline";

    std::cout << "Перейдите по ссылке для авторизации: " << auth_url << std::endl;
    std::cout << "Введите код авторизации: ";
    std::cin >> auth_code;

    // Шаг 2: Получение токена доступа
    std::string token_url = "https://oauth2.googleapis.com/token";
    std::string post_data = "code=" + auth_code;
    post_data += "&client_id=" + client_id;
    post_data += "&client_secret=" + client_secret;
    post_data += "&redirect_uri=" + redirect_uri;
    post_data += "&grant_type=authorization_code";

    std::string token_response = performHttpRequest(token_url, post_data);

    // Парсинг JSON-ответа
    json token_json = json::parse(token_response);
    std::string access_token = token_json["access_token"];

    // Шаг 3: Получение информации о пользователе
    std::string userinfo_url = "https://www.googleapis.com/oauth2/v1/userinfo?access_token=" + access_token;
    std::string userinfo_response = performHttpRequest(userinfo_url);

    // Парсинг JSON-ответа
    json userinfo_json = json::parse(userinfo_response);
    std::cout << "User Info: " << userinfo_json.dump(4) << std::endl;

    return 0;
}
