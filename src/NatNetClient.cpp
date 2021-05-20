#include <iostream>
#include <string>
#include <math.h>
#include <map>
#include <vector>
#include <thread>
#include <map>
#include <windows.h>
#include <winsock.h>
#include <cpprest/json.h>


//NatNet SDK
//#include "NatNetCAPI.h" 
#include "NatNetClient.h"
#include "natutils.h"
#include "resource.h"
#include "RigidBodyCollection.h"
#include "MarkerPositionCollection.h"


// boolean variable used in the thread
std::atomic_bool exit_request(false); 

// Our NatNet client object.
NatNetClient natnetClient;

// Objects for saving off marker and rigid body data streamed
// from NatNet.
MarkerPositionCollection markerPositions;
RigidBodyCollection rigidBodies;

std::map<int, std::string> mapIDToName;

extern std::map<utility::string_t, utility::string_t> dictionary;
extern std::mutex dict_m;

// Ready to render?
bool render = true;

// World Up Axis (default to Y) 
int upAxis = 1;

// Used for converting NatNet data to the proper units
float unitConversion = 1.0f;

// NatNet server IP address.
int IPAddress[4] = { 127, 0, 0, 1 };

int g_analogSamplesPerMocapFrame = 0;

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

std::wstring StringToWString(const std::string &s) {
    std::wstring wsTmp(s.begin(), s.end());
    return wsTmp;
}

// request data descriptions from server
void data_request()
{
    int prev_x, prev_y = 0;
    float v = 0;
    bool initial = True;
    int tau_ms = 1000;

    while (!exit_request)
    {
        sDataDescriptions* pDataDefs = NULL;
        int result = natnetClient.GetDataDescriptionList(&pDataDefs);
        if (result != ErrorCode_OK || pDataDefs == NULL)
        {
            std::cout << "NatNetClient: Unable to retrieve Data Descriptions." << std::endl;
        }
        
        // getting name and description
        for (int i = 0; i < pDataDefs->nDataDescriptions; i++)
        {
            if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
            {
                sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
                mapIDToName[pRB->ID] = std::string(pRB->szName);

                float x, y, z, qx, qy, qz, qw;

                std::tuple<float,float,float> coords = rigidBodies.GetCoordinates(i);
                std::tuple<float, float, float, float> quads = rigidBodies.GetQuaternion(i);

                x = std::get<2>(coords);
                y = std::get<0>(coords);
                z = std::get<1>(coords);
                qx = std::get<0>(quads);
                qy = std::get<1>(quads);
                qz = std::get<2>(quads);
                qw = std::get<3>(quads);

                Quat qu;
                qu.x = qx;
                qu.y = qy;
                qu.z = qz;
                qu.w = qw;
                EulerAngles angles = Eul_FromQuat(qu, EulOrdZXYs);

                // calculate v
                if(initial == True)
                {
                    v = 0
                    prev_x = x;
                    prev_y = y;
                    initial = False;
                }
                else
                {
                    v = sqrt(pow((x - prev_x), 2) + pow((y - prev_y), 2));
                    v /= (tau_ms/1000.0); // divide difference by the sleep time
                    
                    // set variables for next calculation
                    prev_x = x;
                    prev_y = y;

                }

                dict_m.lock();                
                if(rigidBodies.GetParams(i) == 1)
                    dictionary[StringToWString(mapIDToName[pRB->ID])] = std::to_wstring(x) + L", " +  std::to_wstring(y) + L", " + std::to_wstring(angles.z) + L", " + std::to_wstring(v);
                else
                    dictionary[StringToWString(mapIDToName[pRB->ID])] = L"untracked";
                dict_m.unlock();

            }
        }
        Sleep(tau_ms); // sleep for 100 ms
    }
}
