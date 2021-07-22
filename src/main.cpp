#include <iostream>
#include <map>
#include <set>
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include "shared.h"

// WSTRING to STRING conversion
#ifdef _UTF16_STRINGS
#include <locale>
#include <codecvt>
using convert_type = std::codecvt_utf8<wchar_t>;
std::wstring_convert<convert_type, wchar_t> wstr_converter;
#define WSTR2STR(S) (wstr_converter.to_bytes(S))
#else
#define WSTR2STR(S) (S)
#endif

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace std;

// configs come from a JSON File
json::value json_cfg;

// dictionaries of rigid bodys and a lock to handle sync
std::mutex dict_m;
std::map<utility::string_t, utility::string_t> dict_optitrack;    // Controllerd by OptiTrack Objects
std::map<utility::string_t, utility::string_t> dict_extras;       // Adds extras to dict_optitrack
std::map<utility::string_t, utility::string_t> dict_user;         // Treated as separate items 
                                                                  // and managed remotely by the 
                                                                  // user via PUT/DEL. It overrides 
                                                                  // the values above when keys are
                                                                  // the same

// flag to enable/disable kalman filter on data
bool filter_on;

// list of rigid bodies to filter
std::vector<std::string> rigid_bodies_to_filter;


void handle_get(http_request request){
   auto answer = json::value::object();
   utility::string_t query = request.request_uri().query();
   utility::string_t filter_rb;
   if(!query.empty()){
      // remove "RigidBody=" from the text
      filter_rb = query.replace(0,10,L"");
   }
   

   dict_m.lock();
   if (!query.empty() && !filter_rb.empty()){
      auto found_at = dict_optitrack.find(filter_rb);
      if(found_at != dict_optitrack.end()){
         auto extra_cfg = dict_extras[filter_rb];
         if (!extra_cfg.empty() && found_at->second != utility::string_t(L"untracked"))
            answer[filter_rb] = json::value::string(found_at->second + L", " + extra_cfg);
         else
            answer[filter_rb] = json::value::string(found_at->second);
      }
      else{
         answer[filter_rb] = json::value::string(L"");
      }
   }
   else
   {
      for (auto const & p : dict_optitrack){
         auto extra_cfg = dict_extras[p.first];
         if (!extra_cfg.empty()  && p.second != utility::string_t(L"untracked"))
            answer[p.first] = json::value::string(p.second + L", " + extra_cfg);
         else
            answer[p.first] = json::value::string(p.second);
      }
   }
   for (auto const & p : dict_user){
      answer[p.first] = json::value::string(p.second);
   }
   dict_m.unlock();

   request.reply(status_codes::OK, answer);
}
void handle_put(http_request request){
   auto answer = json::value::object();
   auto jvalue = request.extract_json().get();
   if (!jvalue.is_null()){
      dict_m.lock();
      for (auto const & e : jvalue.as_object()){
         if (e.second.is_string()){
            auto key = e.first;
            auto value = e.second.as_string();
         
            if (dict_user.find(key) == dict_user.end())
               answer[key] = json::value::string(L"<put>");
            else
               answer[key] = json::value::string(L"<updated>");
               
            dict_user[key] = value;
         }
      }
      dict_m.unlock();
   }
   request.reply(status_codes::OK, answer);
}
void handle_del(http_request request){
   auto answer = json::value::object();
   auto jvalue = request.extract_json().get();
   if (!jvalue.is_null()){
      set<utility::string_t> toDel_keys;
      dict_m.lock();
      for (auto const & e : jvalue.as_array()){
         if (e.is_string()){
            auto key = e.as_string();
            auto pos = dict_user.find(key);
            if (pos == dict_user.end())
               answer[key] = json::value::string(L"<not_found>");
            else{
               answer[key] = json::value::string(L"<deleted>");
               toDel_keys.insert(key);
            }
         }
      }
      for (auto const & key : toDel_keys)
         dict_user.erase(key);
      dict_m.unlock();
   }
   request.reply(status_codes::OK, answer);
}

int main(int argc, char* argv[]){

   // check args
   if(argc != 2){
      std::cout << "Error: you must supply the JSON configuration file as the only argument." << std::endl;
      return -1;
   }

   // check config file
   std::string json_cfg_file = std::string(argv[1]);
   if(!std::filesystem::exists(json_cfg_file)){
      std::cout << "Error: the provided JSON configuration file does not exist." << std::endl;
      return -1;
   }

   // read config file
   utility::ifstream_t file_reader = utility::ifstream_t(json_cfg_file);
   json_cfg = json::value::parse(file_reader);
   file_reader.close();

   // read the extra configs from the file
   if(json_cfg.has_field(L"rigidbody_extra_config")){
      auto rigidbody_extra_config = json_cfg.at(L"rigidbody_extra_config").as_object();
      for (auto const & sub_json : rigidbody_extra_config){
         utility::string_t key = sub_json.first;
         utility::string_t val = sub_json.second.as_string();
         dict_extras[key] = val;
      }
   }

   // enable/disable the kalman filter
    if (WSTR2STR(json_cfg.at(L"enable_kalman_filter").as_string()) == "true")
        filter_on = true;
    else
        filter_on = false;

   // find which rigid bodies we want to use the kalman filter on
   string rigid_bodies_list = WSTR2STR(json_cfg.at(L"rigid_bodies_for_kalman_filter").as_string());
   if (!rigid_bodies_list.empty()){
      stringstream ss_rigid_bodies_list;
      ss_rigid_bodies_list << rigid_bodies_list;
      while(ss_rigid_bodies_list.good()){
         string split_string;
         getline(ss_rigid_bodies_list, split_string, ',');
         rigid_bodies_to_filter.push_back(split_string);
      }
   }   
      
   // set the frame callback handler
   natnetClient.SetFrameReceivedCallback(DataHandler);

   // connect to the server
   std::string optitrack_server_address = WSTR2STR(json_cfg.at(L"optitrack_server_address").as_string());
   sNatNetClientConnectParams connectParams;
   connectParams.connectionType = ConnectionType::ConnectionType_Multicast;
   connectParams.localAddress = "127.0.0.1";
   connectParams.serverAddress = optitrack_server_address.c_str();
   int retCode = natnetClient.Connect(connectParams);
   if (retCode != ErrorCode_OK)
   {
      //Unable to connect to server.
      cout << "Error: Unable to connect to OptiTrack Motive stream-server." << endl;
      return false;
   }
   else
   {
      // Print server info
      sServerDescription ServerDescription;
      memset(&ServerDescription, 0, sizeof(ServerDescription));
      natnetClient.GetServerDescription(&ServerDescription);
      if (!ServerDescription.HostPresent)
      {
         //Unable to connect to server. Host not present
         cout << "Error: Unable to connect to server. Host is not present." << endl;
         return false;
      }
   }

   // start the REST listener
   auto rest_server_url = json_cfg.at(L"rest_server_url").as_string();
   http_listener listener(rest_server_url);
   listener.support(methods::GET,  handle_get);
   listener.support(methods::PUT,  handle_put);
   listener.support(methods::DEL,  handle_del);
   try{
      listener
         .open()
         .then([&listener]() {
             std::cout << "The REST server is up and waiting for GET requests ... \n"; 
             })
         .wait();
   }
   catch (std::exception const & e){
      std::wcout << e.what() << std::endl;
   }
    
    // using a thread to keep it running until a button is pressed
    thread loopThread = thread(data_request);
    cout << "Press any key to exit." << endl;
    getchar();
    exit_request = true;
    cout << "Exiting ..." << endl;
    loopThread.join();

    // Done - clean up.
    natnetClient.Disconnect();

   return 0;
}