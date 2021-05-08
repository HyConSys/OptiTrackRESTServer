#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <mutex>
#include <stdio.h>

#include "NatNetClient.h"
#include "NNClient.h"
#include "NatNetCAPI.h"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace std;

extern atomic_bool keyIsPressed; 
extern int upAxis;
extern float unitConversion;
extern NatNetClient natnetClient;

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>

std::mutex dict_m;
std::map<utility::string_t, utility::string_t> dictionary;

void display_json(json::value const & jvalue, utility::string_t const & prefix){
   std::wcout << prefix << jvalue.serialize() << std::endl;
}

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
   // start the listener
   http_listener listener(L"http://*:12345/OptiTrackRestServer");
   listener.support(methods::GET,  handle_get);
   try{
      listener
         .open()
         .then([&listener]() {
             std::cout << "Starting to listen ... \n"; 
             })
         .wait();
   }
   catch (std::exception const & e){
      std::wcout << e.what() << std::endl;
   }

   // print version info
   unsigned char ver[4];
   NatNet_GetVersion(ver);
   printf("NatNet Sample Client (NatNet ver. %d.%d.%d.%d)\n", ver[0], ver[1], ver[2], ver[3]);

   // set the frame callback handler
   natnetClient.SetFrameReceivedCallback(DataHandler);

   // connect to the server
   sNatNetClientConnectParams connectParams;
   connectParams.connectionType = ConnectionType::ConnectionType_Multicast;
   connectParams.localAddress = "192.168.1.194";
   connectParams.serverAddress = "192.168.1.194";
   int retCode = natnetClient.Connect(connectParams);
   if (retCode != ErrorCode_OK)
   {
      //Unable to connect to server.
      cout << "Unable to connect to server." << endl;
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
         cout << "Unable to connect to server. Host is not present." << endl;
         return false;
      }
   }
    
    // using a thread to keep it running until a button is pressed
    thread loopThread = thread(data_request);
    cout << "Press any key to exit." << endl;
    char c = getchar();
    keyIsPressed = true;
    loopThread.join();

    // Done - clean up.
    natnetClient.Disconnect();

   return 0;
}