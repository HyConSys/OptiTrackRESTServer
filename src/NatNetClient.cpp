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
#include <chrono>


//NatNet SDK
//#include "NatNetCAPI.h" 
#include "NatNetClient.h"
#include "natutils.h"
#include "resource.h"
#include "RigidBodyCollection.h"
#include "MarkerPositionCollection.h"
#include "KalmanFilter.h"

#ifdef _UTF16_STRINGS
#else
#define WSTR2STR(S) (S)
#endif


// boolean variable used in the thread
std::atomic_bool exit_request(false); 

// Our NatNet client object.
NatNetClient natnetClient;

// Objects for saving off marker and rigid body data streamed
// from NatNet.
MarkerPositionCollection markerPositions;
RigidBodyCollection rigidBodies;

std::map<int, std::string> mapIDToName;

extern std::map<utility::string_t, utility::string_t> dict_optitrack;
extern std::mutex dict_m;

// enable/dsiable kalman filter;
extern bool filter_on;

// list of rigid bodies to filter
extern std::vector<std::string> rigid_bodies_to_filter;

// x shift due to vehicle weights being off
float x_shift = 0.0f;

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
    float prev_x = 0.0f, prev_y = 0.0f, prev_t = 0.0f;
    float v =  0.0f;
    int tau_ms = 10;
    float hold_x;
    float hold_y;
    float hold_angle;
    float hold_v;

    std::map<std::string, std::vector<float>> infoDict;

    std::map<std::string, KalmanFilter> x_filter_dict;
    std::map<std::string, KalmanFilter> y_filter_dict;
    std::map<std::string, KalmanFilter> angle_filter_dict;
    std::map<std::string, KalmanFilter> v_filter_dict;

    // initialize dictionaries for kalman filter
    for (int i = 0; i < rigid_bodies_to_filter.size(); i++) {
        x_filter_dict[rigid_bodies_to_filter[i]] = KalmanFilter(false);
        y_filter_dict[rigid_bodies_to_filter[i]] = KalmanFilter(false);
        angle_filter_dict[rigid_bodies_to_filter[i]] = KalmanFilter(false);
        v_filter_dict[rigid_bodies_to_filter[i]] = KalmanFilter(false);
    }

    auto t_start = std::chrono::high_resolution_clock::now();
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

                // measure the time
                auto t_now = std::chrono::high_resolution_clock::now();
                float t = (float)std::chrono::duration<double>(t_now - t_start).count();

                std::tuple<float, float, float, float> quads = rigidBodies.GetQuaternion(i);

                x = std::get<2>(coords) - x_shift;
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

                // save prev values
                if (infoDict[mapIDToName[pRB->ID]].empty()){
                    prev_t = t;
                    prev_x = x;
                    prev_y = y;
                }
                else{
                    prev_t = infoDict[mapIDToName[pRB->ID]][0];
                    prev_x = infoDict[mapIDToName[pRB->ID]][1]; // same as prev_x = x
                    prev_y = infoDict[mapIDToName[pRB->ID]][2]; // same as prev_y = y
                }

                std::vector<float> infoVector;
                infoVector.push_back(t);
                infoVector.push_back(x);
                infoVector.push_back(y);
                infoDict[mapIDToName[pRB->ID]] = infoVector;

                float time_diff = t - prev_t;
                if (time_diff == 0)
                    v = 0.0F;
                else
                    v = ((float)sqrt(pow((x - prev_x), 2) + pow((y - prev_y), 2)))/time_diff;

                // checking if the current rigid body is one that we want to filter 
                bool rb_found = (std::find(rigid_bodies_to_filter.begin(), rigid_bodies_to_filter.end(), mapIDToName[pRB->ID]) != rigid_bodies_to_filter.end());
                if (filter_on && rb_found){ // check if filter is to be used or not
                    hold_x = x_filter_dict[mapIDToName[pRB->ID]].insertElement(x);
                    hold_y = y_filter_dict[mapIDToName[pRB->ID]].insertElement(y);
                    hold_angle = angle_filter_dict[mapIDToName[pRB->ID]].insertElement(angles.z);
                    hold_v = v_filter_dict[mapIDToName[pRB->ID]].insertElement(v);
                }
                else{
                    hold_x = x;
                    hold_y = y;
                    hold_angle = angles.z;
                    hold_v = v;
                }

                dict_m.lock();                
                if(rigidBodies.GetParams(i) == 1)
                    dict_optitrack[StringToWString(mapIDToName[pRB->ID])] = std::to_wstring(t) + L", " + std::to_wstring(hold_x) + L", " +  std::to_wstring(hold_y) + L", " + std::to_wstring(hold_angle) + L", " + std::to_wstring(hold_v);
                else
                    dict_optitrack[StringToWString(mapIDToName[pRB->ID])] = L"untracked";
                dict_m.unlock();
            }
        }
        Sleep(tau_ms); // sleep for some ms
    }
}
