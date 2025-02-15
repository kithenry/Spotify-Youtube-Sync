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

size_t refreshToken() {
  // for getting new access token
}

int main() {
  // Define the necessary strings using std::string for better handling
  std::string client_id = "164350196968-vjf45qp8u8htdmfud7dsj4gch0tv4t9m.apps."
                          "googleusercontent.com";
  std::string client_secret = "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";
  std::string base_url = "https://accounts.google.com/o/oauth2/v2/auth?";
  std::string redirect_uri = "http://localhost";
  std::string scope = "https://www.googleapis.com/auth/youtube.readonly";

  // Build the request URL by concatenating the necessary components
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

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize cURL!" << std::endl;
    return 0;
  }

  std::string post_url = "https://oauth2.googleapis.com/token";
  std::string content_type = "application/x-www-form-urlencoded";
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
  json jsonResponse = json::parse(response_data);
  std::cout << "This is how the parsed json string looks: \n " << jsonResponse
            << std::endl;
  std::cout << "Access Token: " << jsonResponse["access_token"]
            << "\nRefresh Token: " << jsonResponse["refresh_token"]
            << "\nScope: " << jsonResponse["scope"]
            << "\nExpires in: " << jsonResponse["expires_in"] << "seconds"
            << std::endl;

  std::string access_token = jsonResponse["access_token"];
  std::string refresh_token = jsonResponse["refresh_token"];
  int expires_in = jsonResponse["expires_in"];

  auto now = std::chrono::system_clock::now();
  auto future_time = now + std::chrono::seconds(expires_in);

  json storedData;
  storedData["expiry_time"] = std::chrono::system_clock::to_time_t(future_time);
  storedData["refresh_token"] = refresh_token;
  storedData["access_token"] = access_token;

  std::ofstream file("storedData.json");

  if (!file) {
    std::cerr << "Error opening file for writing!" << std::endl;
    return 1;
  }

  file << storedData.dump(4);
  file.close();

  // setting up mechanism to refresh access token 5 minutes before it expires
  // (this will be in the code to access the api, check if time is still
  // sufficient, if so, carry on, if not, refresh token) refresh token code get
  // the time when the token will expire and keep checking how far now is from
  // then if now is less than or equal to 10 minutes away from time of expiry,
  // refresh the token
}

// using json = nlohmann::json; //
//
// size_t WriteCallback(void *contents, size_t size, size_t nmemb,
//                      std::string *output) {
//   output->append((char *)contents, size * nmemb);
//   return size * nmemb;
// }
//
// void getYouTubePlaylists(const std::string &accessToken) {
//   CURL *curl;
//   CURLcode res;
//   std::string responseString;
//
//   curl = curl_easy_init();
//   if (curl) {
//     std::string url = "https://www.googleapis.com/youtube/v3/"
//                       "playlists?part=snippet&mine=true&maxResults=10";
//
//     struct curl_slist *headers =
//         NULL; // a pointer (headers) to an address holding a struct of
//         headers
//               // initialized to null.
//     headers = curl_slist_append(
//         headers,
//         ("Authorization Bearer " + accessToken)
//             .c_str()); // the .c_tr() is for compatibility with C-type string
//                       // user functions like printf. It's a C++ string
//                       method.
//     headers = curl_slist_append(
//         headers, "Accept: application/json"); // headers is an existing
//                                               // curl_slist or nullptr for a
//                                               new
//                                               // list. The second arg is what
//                                               is
//                                               // to be appended. This
//                                               curl_slist
//                                               // is a linked list
//                                               datastructure.
//
//     curl_easy_setopt(curl, CURLOPT_URL,
//                      url.c_str()); // pass url as C-format string
//     curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // pass headers
//     object curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
//                      WriteCallback); // pass address to callback function
//     curl_easy_setopt(curl, CURLOPT_WRITEDATA,
//                      &responseString); // pass address of responseString
//
//     res = curl_easy_perform(curl);
//     if (res != CURLE_OK) {
//       std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
//     } else {
//       json jsonResponse = json::parse(responseString);
//       std::cout << "Playlists: " << responseString << std::endl;
//       for (const auto &item : jsonResponse["items"]) {
//         std::cout << "- " << item["snippet"]["title"] << std::endl;
//       }
//     }
//
//     curl_easy_cleanup(curl);
//     curl_slist_free_all(headers);
//   }
// }
//
// int main() {
//   std::string accessToken =
//       "ya29."
//       "a0AXeO80RlYNnwenuw843PDV5fHlWjTxnISO1qD8CUg6NzKj9a9i64Zlw6mEBMBRl5ZPdq9C"
//       "W6nGi6qJLjrENL38WEzOY18EpGkjpjRPbonNqOdA9NvBdzHfE9XZ_UYD2_"
//       "SYcya9chk89LApyl1y4K48bRlnPZ4e664eFqbCEVaCgYKAasSARMSFQHGX2MiXyaPgLX2ypD"
//       "9TRofnI4XNw0175";
//   getYouTubePlaylists(accessToken);
//   return 0;
// }
// get browser opened up with a curl request
// get user to select gmail account and copy code from redirect to stdin
// use code to get the refresh token and access token
