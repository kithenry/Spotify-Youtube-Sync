#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <curl/easy.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

// curl utility function here
size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *output) {
  size_t total_size = size * nmemb;
  output->append((char *)contents, total_size);
  return total_size;
}

// utility function here
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

// authentication and token refresh
std::time_t getExpiryTime(int secondsTillExpiry) {
  std::time_t now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now()); // epoch style time
  std::time_t expiry_time = now + secondsTillExpiry;
  return expiry_time;
}

// file management (I/O)
bool fileExists(const std::string &filename) {
  std::ifstream file(filename);
  return file.good(); // Returns true if file exists
}

bool fileIsEmpty(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate); // Open at the end
  return file.tellg() == 0; // Check if position is 0 (empty file)
}

// file management (I/O)
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

// authentication and token refresh
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
    std::cout
        << "About " << difference / 60
        << " minutes left till expiry. Still have some time till token refresh."
        << std::endl;
    std::cout << "Time left till refresh: " << (difference - 10 * 60) / 60
              << " minutes." << std::endl;
    return 0;
  }
}

// authentication and token refresh
void refreshAccessToken(std::string client_id, std::string client_secret,
                        std::string post_url, json *accessTokens) {
  // for getting new access token
  // after gettting new access token, update storedData file with new data
  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Could not initialize cURL!" << std::endl;
  }
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");
  std::string post_fields =
      "client_id=" + client_id + "&client_secret=" + client_secret +
      "&refresh_token=" + std::string(accessTokens->at("refresh_token")) +
      "&grant_type=refresh_token";
  std::string response_data;
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_URL, post_url.c_str());
  curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_fields.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "Request Processing Error!" << std::endl;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  std::cout << response_data << std::endl;
  // update AccesTokens with response_data
  json updatedData = json::parse(response_data);
  (*accessTokens)["access_token"] = updatedData["access_token"];
  (*accessTokens)["expiry_time"] = getExpiryTime(updatedData["expires_in"]);
  storeFetchedData(*accessTokens);
}

// file management and (I/O)
json loadStoredData(std::string fileName) {
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

// authentication
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

// authentication
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

// response processing
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

// curl requests
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

// curl requests
json getPlaylistItems(std::string access_token, std::string *resource_base_url,
                      std::string *get_data, std::string playlistId) {
  // Make a local copy of get_data to ensure a clean query string in each call
  std::string local_get_data = *get_data + "&playlistId=" + playlistId;

  std::string request_url =
      *resource_base_url + "playlistItems" + local_get_data;
  json playlistItems;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, ("Authorization: Bearer " + access_token).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize cURL" << std::endl;
    return json();
  }

  std::string response_data;
  curl_easy_setopt(curl, CURLOPT_URL, request_url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    std::cerr << "Curl execution returned errors!" << std::endl;
  } else {
    std::cout << "Curl Exec Successful " << std::endl;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  std::cout << "Reached this point !!! " << std::endl;
  std::cout << response_data << std::endl;

  try {
    playlistItems = json::parse(response_data);
  } catch (const std::exception &e) {
    std::cerr << "JSON parsing failed: " << e.what() << std::endl;
    return json();
  }

  // If there's a nextPageToken, fetch the next page
  if (playlistItems.contains("nextPageToken") &&
      !playlistItems["nextPageToken"].is_null()) {
    std::cout << "Getting more results .." << std::endl;
    std::cout << "Next Page Token: " << playlistItems["nextPageToken"]
              << std::endl;

    // Create a new clean query string for the next page request
    std::string next_get_data =
        *get_data + "&playlistId=" + playlistId +
        "&pageToken=" + playlistItems["nextPageToken"].get<std::string>();

    return getPlaylistItems(access_token, resource_base_url, &next_get_data,
                            playlistId);
  }

  std::cout << "Just fetched the last page, backtracking.." << std::endl;
  print_json(playlistItems);
  return playlistItems;
}

int main() {

  // load credentials from json file
  std::string credentials_file = "creds.json";
  json creds = loadStoredData(credentials_file);
  std::string client_id = creds["client_id"];
  std::string client_secret = creds["client_secret"];
  std::string base_url = "https://accounts.google.com/o/oauth2/v2/auth?";
  std::string redirect_uri = "http://localhost";
  std::string scope = "https://www.googleapis.com/auth/youtube";
  std::string storedDataFile = "storedData.json";
  json accessTokens;
  time_t expiry_time;
  if (fileExists(storedDataFile) && !fileIsEmpty(storedDataFile)) {
    // load from memory the access_token and refresh_token and expiry time
    std::cout << "Trying to retrive stored data" << std::endl;
    accessTokens = loadStoredData(storedDataFile);
    expiry_time = accessTokens["expiry_time"];
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
    accessTokens = json::parse(acess_tokens_data);
    processJsonData(accessTokens);
    expiry_time = getExpiryTime(accessTokens["expires_in"]);
  }

  // calculate expiry_time

  if (itIsResetTime(expiry_time)) {
    std::cout << "Time to refresh the tokens" << std::endl;
    std::string token_url = "https://oauth2.googleapis.com/token";
    std::cout << "Old Access Tokens:\n" << accessTokens << std::endl;
    refreshAccessToken(client_id, client_secret, token_url, &accessTokens);
    std::cout << "New Access Tokens:\n" << accessTokens << std::endl;
  } else {
    std::cout << "Carry on, not refresh time yet" << std::endl;
  }
  // getting playlists
  std::string resource_base_url = "https://www.googleapis.com/youtube/v3/";
  std::string get_data = "?part=snippet&mine=true&maxResult=10";
  json playlist_data;
  getPlayLists(accessTokens["access_token"], &resource_base_url, &get_data,
               &playlist_data);
  // print_json(playlist_data);
  json processedPlayListData;
  // getting playlist times
  std::string current_title;
  json currentSnippet;
  for (auto &[key, value] : playlist_data.items()) { // get each page object
    for (auto &playlistObject : value["items"]) { // get each object in the item
                                                  // key of each page object
      try {
        // create object for each, fill with required keys and values
        currentSnippet = playlistObject["snippet"];
        current_title = currentSnippet["title"];
        std::cout << current_title << std::endl;
        processedPlayListData[current_title] = {
            {"playlistId", playlistObject["id"]},
            {"description", currentSnippet["description"]},
            {"creationDate", currentSnippet["publishedAt"]}};

        std::cout << std::endl;
        std::cout << std::endl;
      } catch (const std::exception &e) {
        std::cout << "Handling exception:  " << e.what() << std::endl;
        throw;
      }
    }
  }

  print_json(processedPlayListData);

  // getting the items in each playlist.
  get_data = "?part=snippet&maxResults=10&key=";
  for (auto &[key, value] : processedPlayListData.items()) {
    // std::cout << value << std::endl;
    getPlaylistItems(accessTokens["access_token"], &resource_base_url,
                     &get_data, value["playlistId"]);
  }
  // setting up mechanism to refresh access token 5 to 10 minutes before expiry
  // (this will be in the code to access the api, check if time is still
  // sufficient, if so, carry on, if not, refresh token) refresh token code get
  // the time when the token will expire and keep checking how far now is from
  // then if now is less than or equal to 10 minutes away from time of expiry,
  // refresh the token
  // after getting playlist data ;
  // make sure you have received or playlists. have a means to visit all
  // available pages to ensure this. parse the data to fit some useable format;
  // (see Pagination at youtube api docs) get the name of each list, the
  // description and the contents of the list. for each of the contents, a json
  // object with the title and the artist for a song... so we need a means to
  // tell if some media is a song or not.
  // access user subscriptions
  // add music search function with mod to get best result.
  // parse music info from youtube description (supplied to youtube by bla bla
  // bla) modularise the code; functions for handling curl, authentication,
  // handling json parsing, handling json printing. (follow GPT recommendation)
  // - api client base class
  // - youtubeclient & spotify client derived class
  // - playlist manager (searching , creating, syncing playlists across
  // platforms)
  // - utility functions (json parsing, string handling, file access)
  // after cleaning the json.. move on to
  // do all you have done here for the spotify api after that implement search
  // functionality for both spotify and youtube api get some means to get a
  // match for every song in the spotify lists on youtube and vice versa move on
  // to based on the data from the previous step create playlists on the other
  // platform with the same names as was on source names, descriptions, songs...
  // add subscription transfer to your system..  *find ways to get only artist
  // subscriptions etc etc.
  // giving the code structure;
  // apply classes
  // seperate code into multiple files
  // define functions that will be used by any platform that will be added to
  // the code base
  /*
   Y our to-do list is already well-structured, but breaking it down further
   will he*lp clarify responsibilities, ensure modularity, and make
   implementation smoother. Here’s a refined version with more details:

   ---

   ### **1. API Client Base Class (`ApiClient`)**
   **Purpose:**
   A reusable base class that provides core functionality for making HTTP
   requests.

   **Breakdown:**
   ✅ **Member Variables**
   - `std::string access_token` (stores the auth token)
   - `std::string base_url` (stores the API base URL)

   ✅ **Core Methods**
   - `std::string makeRequest(std::string endpoint, std::string method = "GET",
   std::string postData = "");`
   - Handles cURL setup, request execution, and response handling.
   - `void setAccessToken(std::string token);`
   - Updates the token when refreshing authentication.

   ✅ **Considerations:**
   - Error handling for failed API calls.
   - Rate limiting strategies (if needed).

   ---

   ### **2. YouTube & Spotify API Clients (`YouTubeClient` & `SpotifyClient`)**
   **Purpose:**
   Extend `ApiClient` to interact with YouTube and Spotify.

   **Breakdown:**
   ✅ **`YouTubeClient`**
   - `json getPlaylists();` → Fetch all user playlists.
   - `json getPlaylistItems(std::string playlistId);` → Fetch items in a
   playlist.
   - `json searchSong(std::string query);` → Search for songs.
   - `json createPlaylist(std::string name, std::string description);` → Create
   a playlist.
   - `bool addSongToPlaylist(std::string playlistId, std::string songId);` → Add
   a song to a playlist.

   ✅ **`SpotifyClient`**
   - `json getPlaylists();`
   - `json getPlaylistItems(std::string playlistId);`
   - `json searchSong(std::string query);`
   - `json createPlaylist(std::string name, std::string description);`
   - `bool addSongToPlaylist(std::string playlistId, std::string songId);`

   ✅ **Considerations:**
   - YouTube uses video IDs, while Spotify uses track IDs.
   - OAuth token handling may differ slightly.

   ---

   ### **3. Playlist Manager (`PlaylistManager`)**
   **Purpose:**
   Handles cross-platform playlist management.

   **Breakdown:**
   ✅ **Fetching & Storing Playlists**
   - `void loadPlaylists();`
   - Fetches and stores playlists from YouTube & Spotify.

   ✅ **Searching for Songs**
   - `json search(std::string query, std::string platform);`
   - Calls either `YouTubeClient::searchSong` or `SpotifyClient::searchSong`.

   ✅ **Syncing Playlists**
   - `bool syncPlaylist(std::string sourcePlatform, std::string destPlatform,
   std::string playlistId);`
   - Fetches songs from one platform, searches them on the other, and adds them
   to a new/existing playlist.

   ✅ **Exporting Playlists**
   - `void saveToFile(std::string filename, json data);`
   - Saves playlist info in JSON or CSV.

   ✅ **Considerations:**
   - **Matching songs across platforms** (some may not be available).
   - **Handling missing metadata** (some info may be incomplete).

   ---

   ### **4. Utility Functions (`Utils`)**
   **Purpose:**
   Provide helper functions for various tasks.

   **Breakdown:**
   ✅ **JSON Handling**
   - `json parseJson(std::string jsonString);`
   - `std::string jsonToString(json data);`

   ✅ **String Handling**
   - `std::string toLowerCase(std::string str);`
   - `std::string trim(std::string str);`

   ✅ **File Access**
   - `bool writeToFile(std::string filename, std::string content);`
   - `std::string readFromFile(std::string filename);`

   ✅ **Considerations:**
   - Logging system for debugging.
   - Error handling for missing files.

   ---

   ### **5. After Cleaning JSON, Next Steps**
   After ensuring JSON is clean and structured properly:
   ✅ **Test API client classes independently**
   ✅ **Implement search & playlist functions**
   ✅ **Write tests for cross-platform syncing**
   ✅ **Document API endpoints & how functions work**

   ---

   This structure will keep your code modular and scalable for future
   integrations. 🚀
   */
}

/**

 [clang] (typecheck_bool_condition) Value of type 'iterator' (aka
 'iter_impl<nlohmann::basic_json<std::map, std::vector, std::basic_string<char>,
 bool, long, unsigned long, double, std::allocator, nlohmann::adl_serializer,
 std::vector<unsigned char, std::allocator<unsigned char>>, void>>') is not
 contextually convertible to 'bool'

 **/
