// client application.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "NNClient.h"
#include <cpprest/json.h>
#include <map>

using namespace std;

// boolean variable used in the thread
atomic_bool keyIsPressed(false); 

// Our NatNet client object.
NatNetClient natnetClient;

// Objects for saving off marker and rigid body data streamed
// from NatNet.
MarkerPositionCollection markerPositions;
RigidBodyCollection rigidBodies;

std::map<int, std::string> mapIDToName;
extern std::map<utility::string_t, utility::string_t> dictionary;

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

std::wstring StringToWString(const std::string &s) {
    std::wstring wsTmp(s.begin(), s.end());
    return wsTmp;
}

// request data descriptions from server
void data_request()
{
    //int count = 1; // count to see how many times it has run already
    while (!keyIsPressed)
    {
        sDataDescriptions* pDataDefs = NULL;
        int result = natnetClient.GetDataDescriptionList(&pDataDefs);
        if (result != ErrorCode_OK || pDataDefs == NULL)
        {
            printf("[SampleClient] Unable to retrieve Data Descriptions.");
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

                x = get<2>(coords);
                y = get<0>(coords);
                z = get<1>(coords);
                qx = get<0>(quads);
                qy = get<1>(quads);
                qz = get<2>(quads);
                qw = get<3>(quads);

                Quat qu;
                qu.x = qx;
                qu.y = qy;
                qu.z = qz;
                qu.w = qw;
                EulerAngles angles = Eul_FromQuat(qu, EulOrdZXYs);

                dictionary[StringToWString(mapIDToName[pRB->ID])] = to_wstring(x) + L", " +  to_wstring(y) + L", " + to_wstring(angles.z);

            }
        }
        Sleep(1000); // sleep for 1 second
    }
}
