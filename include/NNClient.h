#ifndef NNCLIENT_H
#define NNCLIENT_H

#include <iostream>
#include <string>
#include <math.h>
#include <map>
#include <vector>
#include <thread>
#include <iomanip> 

//NatNet SDK
#include "NatNetTypes.h"
#include "NatNetCAPI.h"
#include "NatNetClient.h"
#include "natutils.h"

#include "RigidBodyCollection.h"
#include "MarkerPositionCollection.h"

#include <windows.h>
#include <winsock.h>
#include "resource.h"
#include "NNClient.h"

// NatNet 
void NATNET_CALLCONV DataHandler(sFrameOfMocapData* data, void* pUserData);    // receives data from the server
bool InitNatNet(LPSTR szIPAddress, LPSTR szServerIPAddress, ConnectionType connType); // this is connecting to the server
bool ParseRigidBodyDescription(sDataDescriptions* pDataDefs);
void data_request();

#endif