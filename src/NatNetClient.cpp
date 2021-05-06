// client application.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <math.h>
#include <map>
#include <vector>

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

using namespace std;

// Our NatNet client object.
NatNetClient natnetClient;

// Objects for saving off marker and rigid body data streamed
// from NatNet.
MarkerPositionCollection markerPositions;
RigidBodyCollection rigidBodies;

std::map<int, std::string> mapIDToName;

// Ready to render?
bool render = true;

// World Up Axis (default to Y) 
int upAxis = 1;

// Used for converting NatNet data to the proper units
float unitConversion = 1.0f;

// NatNet server IP address.
int IPAddress[4] = { 127, 0, 0, 1 };

int g_analogSamplesPerMocapFrame = 0;

// NatNet 
void NATNET_CALLCONV DataHandler(sFrameOfMocapData* data, void* pUserData);    // receives data from the server
void NATNET_CALLCONV MessageHandler(Verbosity msgType, const char* msg);      // receives NatNet error messages
bool InitNatNet(LPSTR szIPAddress, LPSTR szServerIPAddress, ConnectionType connType); // this is connecting to the server
bool ParseRigidBodyDescription(sDataDescriptions* pDataDefs);

// NatNet data callback function. Stores rigid body and marker data in the file level 
// variables markerPositions, and rigidBodies and sets the file level variable render
// to true. This signals that we have a frame ready to render.
void DataHandler(sFrameOfMocapData* data, void* pUserData)
{
    int mcount = min(MarkerPositionCollection::MAX_MARKER_COUNT, data->MocapData->nMarkers);
    markerPositions.SetMarkerPositions(data->MocapData->Markers, mcount);

    // Marker Data
    markerPositions.SetLabledMarkers(data->LabeledMarkers, data->nLabeledMarkers);

    // rigid bodies
    int rbcount = min(RigidBodyCollection::MAX_RIGIDBODY_COUNT, data->nRigidBodies);
    rigidBodies.SetRigidBodyData(data->RigidBodies, rbcount);

    // skeleton segment (bones) as collection of rigid bodies
    for (int s = 0; s < data->nSkeletons; s++)
    {
        rigidBodies.AppendRigidBodyData(data->Skeletons[s].RigidBodyData, data->Skeletons[s].nRigidBodies);
    }

    render = true;
}

// [Optional] Handler for NatNet messages. 
void NATNET_CALLCONV MessageHandler(Verbosity msgType, const char* msg)
{
    printf("\n[SampleClient] Message received: %s\n", msg);
}


// parsing the data description
bool ParseRigidBodyDescription(sDataDescriptions* pDataDefs)
{
    mapIDToName.clear();

    if (pDataDefs == NULL || pDataDefs->nDataDescriptions <= 0)
        return false;

    // preserve a "RigidBody ID to Rigid Body Name" mapping, which we can lookup during data streaming
    int iSkel = 0;
    for (int i = 0; i < pDataDefs->nDataDescriptions; i++)
    {
        if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
        {
            sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
            mapIDToName[pRB->ID] = std::string(pRB->szName);
            
        }
        // is not entering here --> probably not needed
        else if (pDataDefs->arrDataDescriptions[i].type == Descriptor_Skeleton)
        {
            cout << "Test if in else block." << endl;
            sSkeletonDescription* pSK = pDataDefs->arrDataDescriptions[i].Data.SkeletonDescription;
            for (int i = 0; i < pSK->nRigidBodies; i++)
            {
                // Note: Within FrameOfMocapData, skeleton rigid body ids are of the form:
                //   parent skeleton ID   : high word (upper 16 bits of int)
                //   rigid body id        : low word  (lower 16 bits of int)
                // 
                // However within DataDescriptions they are not, so apply that here for correct lookup during streaming
                int id = pSK->RigidBodies[i].ID | (pSK->skeletonID << 16);
                mapIDToName[id] = std::string(pSK->RigidBodies[i].szName);
                std::cout << mapIDToName[id] << std::endl;
            }
            iSkel++;
        }
        else
            continue;
        continue;
    }

    return true;
} 

int main()
{
    // print version info
    unsigned char ver[4];
    NatNet_GetVersion(ver);
    printf("NatNet Sample Client (NatNet ver. %d.%d.%d.%d)\n", ver[0], ver[1], ver[2], ver[3]);

    // Set callback handlers
    // Callback for NatNet messages
    NatNet_SetLogCallback(MessageHandler);

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

    // retrieve rigid body descriptions from server
    sDataDescriptions* pDataDefs = NULL;
    retCode = natnetClient.GetDataDescriptionList(&pDataDefs);
    if (retCode != ErrorCode_OK || ParseRigidBodyDescription(pDataDefs) == false)
    {
        cout << "Unable to retrieve RigidBody description." << endl;
        return false;
    }
    // freeing the data descriptions
    NatNet_FreeDescriptions(pDataDefs);
    pDataDefs = NULL;

    // example of NatNet general message passing. Set units to millimeters
    // and get the multiplicative conversion factor in the response.
    // sending and receiving NetNat test command
    void* response;
    int nBytes;
    retCode = natnetClient.SendMessageAndWait("UnitsToMillimeters", &response, &nBytes);
    if (retCode == ErrorCode_OK)
    {
        unitConversion = *(float*)response;
    }
    retCode = natnetClient.SendMessageAndWait("UpAxis", &response, &nBytes);
    if (retCode == ErrorCode_OK)
    {
        upAxis = *(long*)response;
    }

    // to prevent program from closing on its own
    char input = ' ';
    bool exit = false;

    while (input != 'q') 
    {
        cout << "Select 'q' to quit, 'd' to request data desctiptions." << endl;
        cin >> input;

        switch (input)
        {
        case 'q':
        {
            cout << "Quitting..." << endl;
            exit = true;
            break;
        }
        case 'd':
        {
            printf("\n\n[SampleClient] Requesting Data Descriptions...");
            sDataDescriptions* pDataDefs = NULL;
            int result = natnetClient.GetDataDescriptionList(&pDataDefs);
            if (result != ErrorCode_OK || pDataDefs == NULL)
            {
                printf("[SampleClient] Unable to retrieve Data Descriptions.");
            }
            else
            {
                printf("[SampleClient] Received %d Data Descriptions:\n", pDataDefs->nDataDescriptions);
            }

            // getting name and description
            for (int i = 0; i < pDataDefs->nDataDescriptions; i++)
            {
                if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
                {
                    sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
                    cout << "What is pRB?: " << pRB << endl;
                    mapIDToName[pRB->ID] = std::string(pRB->szName);
                    cout << "Name is: " << mapIDToName[pRB->ID] << endl << endl;
                }
                else if (pDataDefs->arrDataDescriptions[i].type == Descriptor_Skeleton)
                {
                    cout << "Test if it ever enters here..." << endl;
                    sSkeletonDescription* pSK = pDataDefs->arrDataDescriptions[i].Data.SkeletonDescription;
                    for (int i = 0; i < pSK->nRigidBodies; i++)
                    {
                        // Note: Within FrameOfMocapData, skeleton rigid body ids are of the form:
                        //   parent skeleton ID   : high word (upper 16 bits of int)
                        //   rigid body id        : low word  (lower 16 bits of int)
                        // 
                        // However within DataDescriptions they are not, so apply that here for correct lookup during streaming
                        int id = pSK->RigidBodies[i].ID | (pSK->skeletonID << 16);
                        mapIDToName[id] = std::string(pSK->RigidBodies[i].szName);
                    }
                }
            }
            break;
        }
        default: { break; }
        if (exit) { break; }

        }

    }
 
    // Done - clean up.
    natnetClient.Disconnect();

    return 0;
}

// Initialize the NatNet client with client and server IP addresses.
bool InitNatNet(LPSTR szIPAddress, LPSTR szServerIPAddress, ConnectionType connType)
{
    unsigned char ver[4];
    NatNet_GetVersion(ver);

    // Set callback handlers
    // Callback for NatNet messages.
    NatNet_SetLogCallback(MessageHandler);
    // this function will receive data from the server
    natnetClient.SetFrameReceivedCallback(DataHandler);

    // step 2b - connect to server
    sNatNetClientConnectParams connectParams;
    connectParams.connectionType = connType;
    connectParams.localAddress = szIPAddress;
    connectParams.serverAddress = szServerIPAddress;
    int retCode = natnetClient.Connect(connectParams);
    if (retCode != ErrorCode_OK)
    {
        //Unable to connect to server.
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
            return false;
        }
    }

    // Retrieve RigidBody description from server
    sDataDescriptions* pDataDefs = NULL;
    // step 3a - getting data descriptions
    retCode = natnetClient.GetDataDescriptionList(&pDataDefs);
    if (retCode != ErrorCode_OK || ParseRigidBodyDescription(pDataDefs) == false)
    {
        //Unable to retrieve RigidBody description
        //return false;
    }
    // step 3c - freeing the data descriptions
    NatNet_FreeDescriptions(pDataDefs);
    pDataDefs = NULL;

    // example of NatNet general message passing. Set units to millimeters
    // and get the multiplicative conversion factor in the response.
    void* response;
    int nBytes;
    // step 2d - sending NetNat commands
    retCode = natnetClient.SendMessageAndWait("UnitsToMillimeters", &response, &nBytes);
    if (retCode == ErrorCode_OK)
    {
        unitConversion = *(float*)response;
    }

    // step 2d - sending NetNat commands
    retCode = natnetClient.SendMessageAndWait("UpAxis", &response, &nBytes);
    if (retCode == ErrorCode_OK)
    {
        upAxis = *(long*)response;
    }

    return true;
}

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file