#include <cpprest/http_listener.h>
#include <cpprest/json.h>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

#include <iostream>
#include <map>
#include <set>
#include <string>

std::map<utility::string_t, utility::string_t> dictionary;

void display_json(json::value const & jvalue, utility::string_t const & prefix){
   std::wcout << prefix << jvalue.serialize() << std::endl;
}

void handle_get(http_request request){
   std::cout << "Received a GET request.\n";

   auto answer = json::value::object();
   for (auto const & p : dictionary){
      answer[p.first] = json::value::string(p.second);
   }

   display_json(json::value::null(), L"R: ");
   display_json(answer, L"S: ");

   request.reply(status_codes::OK, answer);
}

int main(){

   // put some data in the dictionary
   dictionary[L"DeepRacer1"] = L"(x,y,z,a,b,g)";

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

      while (true);
   }
   catch (std::exception const & e){
      std::wcout << e.what() << std::endl;
   }

   return 0;
}