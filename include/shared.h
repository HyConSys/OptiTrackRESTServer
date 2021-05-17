#ifndef NNCLIENT_H
#define NNCLIENT_H

#include <atomic>

//NatNet Stuff
#include "NatNetTypes.h"
#include "NatNetClient.h"
//#include "NatNetCAPI.h"

// externs
extern std::atomic_bool exit_request; 
extern int upAxis;
extern float unitConversion;
extern NatNetClient natnetClient;

// NatNet 
void NATNET_CALLCONV DataHandler(sFrameOfMocapData* data, void* pUserData);    // receives data from the server
void data_request();

#endif