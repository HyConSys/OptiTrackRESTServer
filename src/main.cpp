#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include "shared.h"

// configs

#define REST_SERVER_URL L"http://*:12345/OptiTrackRestServer"
#define LOCAL_IP_ADDRESS "127.0.0.1"
#define OPTITRACK_SERVER_IP_ADDRESS LOCAL_IP_ADDRESS

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace std;

std::mutex dict_m;
std::map<utility::string_t, utility::string_t> dictionary;

void handle_get(http_request request){
   auto answer = json::value::object();

   dict_m.lock();
   for (auto const & p : dictionary){
      answer[p.first] = json::value::string(p.second);
   }
   dict_m.unlock();

   request.reply(status_codes::OK, answer);
}

int main(){
   
   // set the frame callback handler
   natnetClient.SetFrameReceivedCallback(DataHandler);

   // connect to the server
   sNatNetClientConnectParams connectParams;
   connectParams.connectionType = ConnectionType::ConnectionType_Multicast;
   connectParams.localAddress = LOCAL_IP_ADDRESS;
   connectParams.serverAddress = OPTITRACK_SERVER_IP_ADDRESS;
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
   http_listener listener(REST_SERVER_URL);
   listener.support(methods::GET,  handle_get);
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