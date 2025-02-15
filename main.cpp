#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <curl/easy.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *output) {
  size_t total_size = size * nmemb;
  output->append((char *)contents, total_size);
  return total_size;
}

bool isFileEmpty(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate);
  return file.tellg() == 0;
}

int storeFetchedData(json dataToStore) {
  std::ofstream file("storedData.json");

  if (!file) {
    std::cerr << "Error opening file for writing!" << std::endl;
    return 1;
  }

  file << dataToStore.dump(4);
  file.close();
  return 0;
}

std::string refreshToken() {
  // for getting new access token
  // after gettting new access token, update storedData file with new data
}

json fetchStoredData() {
  // for accessing stored tokens and expiry time if any
}

std::string fetchAccessCode(std::string client_id, std::string base_url,
                            std::string redirect_uri, std::string scope) {
  // for getting access code, then later on access token, refresh token and
  // expiry time Build the request URL by concatenating the necessary components
  std::string request_url = base_url +
                            "response_type=code&client_id=" + client_id +
                            "&redirect_uri=" + redirect_uri + "&scope=" + scope;
  std::string command = "xdg-open " + request_url;
  std::cout << request_url << std::endl;

  // tell the user to open the url in their browser, select the gmail account
  // they wish to associate with the app and paste in terminal the link they
  // will be redirected to.. or for now, the access token
  std::cout
      << "Visit the above link in your browser and; \nAfter authenticating "
         "your gmail account or choice, paste the link you will be\nredirected "
         "to "
         "right here under;"
      << std::endl;
  std::string access_code;
  std::cin >> access_code;
  std::cout << access_code << " is the code you received!" << std::endl;
  // getting the access token and refresh token
  return access_code;
}

std::string fetchAccessTokens(std::string post_url, std::string content_type,
                              std::string client_id, std::string client_secret,
                              std::string access_code,
                              std::string redirect_uri) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize cURL!" << std::endl;
    return 0;
  }

  std::string post_fields =
      "client_id=" + client_id + "&client_secret=" + client_secret +
      "&code=" + access_code + "&grant_type=authorization_code" +
      "&redirect_uri=" + redirect_uri;
  std::string response_data;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_URL, post_url.c_str());
  curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_fields.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
  } else {
    std::cout << "Response: " << response_data << std::endl;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return response_data;
}

int processJsonData(json jsonResponse) {
  std::string access_token = jsonResponse["access_token"];
  std::string refresh_token = jsonResponse["refresh_token"];
  int expires_in = jsonResponse["expires_in"];

  auto now = std::chrono::system_clock::now();
  auto future_time = now + std::chrono::seconds(expires_in);

  json jsonData;
  jsonData["expiry_time"] = std::chrono::system_clock::to_time_t(future_time);
  jsonData["refresh_token"] = refresh_token;
  jsonData["access_token"] = access_token;

  return storeFetchedData(jsonData);
}

std::string getPlayLists(std::string client_id, std::string client_secret,
                         std::string access_token) {
  // code goes here
}

int main() {
  // Define the necessary strings using std::string for better handling
  std::string client_id = "164350196968-vjf45qp8u8htdmfud7dsj4gch0tv4t9m.apps."
                          "googleusercontent.com";
  std::string client_secret = "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";
  std::string base_url = "https://accounts.google.com/o/oauth2/v2/auth?";
  std::string redirect_uri = "http://localhost";
  std::string scope = "https://www.googleapis.com/auth/youtube.readonly";
  std::string storedDataFile = "storedData.txt";
  if (!isFileEmpty(storedDataFile)) {
    // fetch access_token and refresh_token and expiry time
    // check if access_token is expired
    // if it is, refresh it then move on to use it, if not, move on to use it.
  } else {
    // get access code, then access token and refresh token and exp time
    // store tokens and expiry time
    std::string access_code =
        fetchAccessCode(client_id, base_url, redirect_uri, scope);
    std::string post_url = "https://oauth2.googleapis.com/token";
    std::string content_type = "application/x-www-form-urlencoded";
    std::string acess_tokens_data =
        fetchAccessTokens(post_url, content_type, client_id, client_secret,
                          access_code, redirect_uri);

    // parse and store the json
    json jsonResponse = json::parse(acess_tokens_data);

    std::cout << "Access Token: " << jsonResponse["access_token"]
              << "\nRefresh Token: " << jsonResponse["refresh_token"]
              << "\nScope: " << jsonResponse["scope"]
              << "\nExpires in: " << jsonResponse["expires_in"] << "seconds"
              << std::endl;
    processJsonData(jsonResponse);
  }

  // setting up mechanism to refresh access token 5 minutes before it expires
  // (this will be in the code to access the api, check if time is still
  // sufficient, if so, carry on, if not, refresh token) refresh token code get
  // the time when the token will expire and keep checking how far now is from
  // then if now is less than or equal to 10 minutes away from time of expiry,
  // refresh the token
}
