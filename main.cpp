#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <curl/easy.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

using json = nlohmann::json;

class Utils {

public:
  void print_json(const json &j, const std::string &prefix = "") {
    for (auto &[key, value] : j.items()) {
      if (value.is_object()) {
        std::cout << prefix << key << " (object):" << std::endl;
        print_json(value, prefix + " ");
      } else if (value.is_array()) {
        std::cout << prefix << key << " (array): " << std::endl;
        for (const auto &item : value) {
          std::cout << prefix << " " << item << std::endl;
        }
      } else {
        std::cout << prefix << key << ": " << value << std::endl;
      }
    }
  }

  bool itIsResetTime(int epochExpiryTime) {
    time_t now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::time_t expiry_time = epochExpiryTime;
    double difference = std::difftime(expiry_time, now);
    if (difference <= 600) {
      std::cout << "10 minutes or less untill expiry. It is reset time."
                << std::endl;
      std::cout << "Time left till expiry: " << difference << std::endl;
      return 1;
    } else {
      std::cout << "About " << difference / 60
                << " minutes left till expiry. Still have some time till token "
                   "refresh."
                << std::endl;
      std::cout << "Time left till refresh: " << (difference - 10 * 60) / 60
                << " minutes." << std::endl;
      return 0;
    }
  }

  json readJsonFile(json fileName) {
    // implementation here
    json jsonData;
    return jsonData;
  }

  bool fileAvailable(const std::string &filename) {
    std::ifstream file(filename);
    return file.good(); // Returns true if file exists
  }

  bool fileIsEmpty(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate); // Open at the end
    return file.tellg() == 0; // Check if position is 0 (empty file)
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

  bool fileReadable(const std::string &filename) {
    return fileAvailable(filename) && !fileIsEmpty(filename);
  }

  json loadStoredData(std::string &fileName) {
    // for accessing stored tokens and expiry time if any
    std::ifstream file(fileName);
    if (!file) {
      std::cerr << "Error opening file for reading!" << std::endl;
      return 1;
    }
    json data;
    file >> data;
    file.close();

    return data;
  }

  std::time_t getExpiryTime(int secondsTillExpiry) {
    std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()); // epoch style time
    std::time_t expiry_time = now + secondsTillExpiry;
    return expiry_time;
  }
};

class ApiClient {
protected:
  json localCredsFileNames;
  json clientData; // Contains client_name, auth_data, CRUD data
  Utils clientUtils;

public:
  ApiClient(const json &clientData, const json &localCredsFileNames)
      : clientData(clientData), localCredsFileNames(localCredsFileNames) {
    authenticateClient();
  }

  virtual void syncPlaylists() = 0; // Pure virtual function

protected:
  void authenticateClient() {
    if (localTokensAvailable()) {
      if (!accessTokenExpired()) {
        return; // Token is valid
      }
      refreshAccessToken();
    } else {
      clientData["accessTokens"] = getAccessTokens(); // Store tokens
      std::cout << std::string(clientData["clientName"])
                << " Authenticated: Client Data Updated " << std::endl;
    }
  }

  virtual json getAccessTokens() = 0;
  virtual bool accessTokenExpired() = 0;
  void refreshAccessToken() {
    std::string requestType = "tokenRefresh";
    // send command to refreshTokens to curlRequestHandler
    // update client Data and update local tokens data
  }
  bool localTokensAvailable() {
    return clientUtils.fileAvailable(
               localCredsFileNames["accessTokensFileName"]) &&
           !clientUtils.fileIsEmpty(
               localCredsFileNames["accessTokensFileName"]);
  }
  std::string curlRequestHandler(const std::string &requestType) {
    CURL *curl = curl_easy_init();
    if (!curl) {
      std::cerr << "Could not initialize cURL!" << std::endl;
      return "";
    }

    std::string request_url = createRequestUrl(requestType);
    struct curl_slist *headers = createHeadersStruct(requestType);
    std::string post_fields = createPostFieldString(requestType);
    std::string response_data;

    setupCurlOpts(curl, request_url, headers, post_fields, response_data);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      std::cerr << "Request Processing Error: " << curl_easy_strerror(res)
                << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response_data;
  }
};

class SyncManager {
private:
  std::vector<std::unique_ptr<ApiClient>> clients;
  std::mutex syncMutex;
  std::thread syncThread;
};

class YouTubeClient : public ApiClient {

  json clientData;
  json localCredsFileNames;

public:
  YouTubeClient(json clientData, json localCredsFileNames)
      : ApiClient(clientData, localCredsFileNames) {}
  void syncPlaylists() override {}
};

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *output) {
  size_t total_size = size * nmemb;
  output->append((char *)contents, total_size);
  return total_size;
}

std::string fetchAccessCode(std::string client_id, std::string base_url,
                            std::string redirect_uri, std::string scope) {
  // for getting access code
  std::string request_url = base_url +
                            "response_type=code&client_id=" + client_id +
                            "&redirect_uri=" + redirect_uri + "&scope=" + scope;

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
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
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

void getPlayLists(std::string access_token, std::string *resource_base_url,
                  std::string *get_data, json *collected_data,
                  int page_no = 1) {
  // code goes here
  std::string request_url = *resource_base_url + "playlists" + *get_data;
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, ("Authorization: Bearer " + access_token).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize cURL" << std::endl;
  }
  // setting up curl_options
  std::string response_data;
  curl_easy_setopt(curl, CURLOPT_URL, request_url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "Curl execution returned errors!" << std::endl;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  // now move on to get the next pages.. jsonize response for next page token
  // and fetch that till there ain't such a token
  // std::cout << response_data << std::endl;
  // pass the response_data and use an if statement to determine if to run the
  // function again on the next page token
  json parsedData = json::parse(response_data);
  std::string current_page = "Page ";
  if (parsedData.contains("nextPageToken") &&
      !parsedData["nextPageToken"].is_null()) {
    std::cout << "Getting more results .." << std::endl;
    // add current page to collected_data

    (*collected_data)[current_page + std::to_string(page_no)] = parsedData;
    *get_data += "&pageToken=" + std::string(parsedData["nextPageToken"]) +
                 "&response_type=video";
    return getPlayLists(access_token, resource_base_url, get_data,
                        collected_data, page_no + 1);
  }
  std::cout << "Just fetched the last page, backtracking.." << std::endl;
  // return parsedData;
}

int main() {}
