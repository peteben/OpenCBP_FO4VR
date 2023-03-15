#include "CollisionConfig.h"

#include <fstream>
#include <algorithm>

#include "log.h"
#include "Utility.hpp"

#pragma warning(disable : 4996)

std::vector<SpecificNPCConfig> specificNPCConfigList;


std::vector<std::string> AffectedNodeLines;
std::vector<std::string> ColliderNodeLines;
concurrency::concurrent_vector<ConfigLine> AffectedNodesList; //Nodes that can be collided with
concurrency::concurrent_vector<ConfigLine> ColliderNodesList; //Nodes that can collide nodes

int collisionSkipFrames = 0; //0
int gridsize = 25;
int adjacencyValue = 0;
int tuningModeCollision = 0;
float actorDistance = 4194304.0f;
int logging = 0;

float collisionX = 1.0;
float collisionY = 1.0;
float collisionZ = 1.0;

std::string GroundReferenceBone("Root");
std::string HighheelReferenceBone("NPC");

//concurrency::concurrent_vector<std::string> PlayerCollisionEventNodes;
//
//float MinimumCollisionDurationForEvent = 0.3f;
//
//concurrency::concurrent_unordered_map<std::string, PlayerCollisionEvent> ActorNodePlayerCollisionEventMap;

void LoadCollisionConfig()
{
    AffectedNodeLines.clear();
    ColliderNodeLines.clear();
    AffectedNodesList.clear();
    ColliderNodesList.clear();

    std::string filepath = "Data\\F4SE\\Plugins\\OCBPCollisionConfig.txt";

    std::ifstream file(filepath);

    if (!file.is_open())
    {
        transform(filepath.begin(), filepath.end(), filepath.begin(), ::tolower);
        file.open(filepath);
    }

    if (file.is_open())
    {
        std::string line;
        std::string currentSetting;
        while (std::getline(file, line))
        {
            //trim(line);
            skipComments(line);
            trim(line);
            if (line.length() > 0)
            {
                if (line.substr(0, 1) == "[")
                {
                    //newsetting
                    currentSetting = line;
                }
                else
                {
                    if (currentSetting == "[ExtraOptions]")
                    {
                        std::string variableName;
                        float variableValue = GetConfigSettingsFloatValue(line, variableName);
                        if (variableName == "collisionX")
                        {
                            collisionX = variableValue;
                        }
                        else if (variableName == "collisionY")
                        {
                            collisionY = variableValue;
                        }
                        else if (variableName == "collisionZ")
                        {
                            collisionZ = variableValue;
                        }
                        else if (variableName == "adjacencyValue")
                        {
                            adjacencyValue = variableValue;
                        }
                    }
                    else if (currentSetting == "[AffectedNodes]")
                    {
                        AffectedNodeLines.emplace_back(line);
                        ConfigLine newConfigLine;
                        newConfigLine.NodeName = line;
                        AffectedNodesList.push_back(newConfigLine);
                    }
                    else if (currentSetting == "[ColliderNodes]")
                    {
                        ColliderNodeLines.emplace_back(line);
                        ConfigLine newConfigLine;
                        newConfigLine.NodeName = line;
                        ColliderNodesList.push_back(newConfigLine);
                    }
                    else
                    {
                        Sphere newSphere;
                        ConfigLineSplitter(line, newSphere);

                        std::string trimmedSetting = gettrimmed(currentSetting);
                        if (std::find(AffectedNodeLines.begin(), AffectedNodeLines.end(), trimmedSetting) != AffectedNodeLines.end())
                        {
                            for (int i = 0; i < AffectedNodesList.size(); i++)
                            {
                                if (AffectedNodesList[i].NodeName == trimmedSetting)
                                {
                                    newSphere.NodeName = AffectedNodesList[i].NodeName;
                                    AffectedNodesList[i].CollisionSpheres.emplace_back(newSphere);
                                    break;
                                }
                            }
                        }
                        if (std::find(ColliderNodeLines.begin(), ColliderNodeLines.end(), trimmedSetting) != ColliderNodeLines.end())
                        {
                            for (int i = 0; i < ColliderNodesList.size(); i++)
                            {
                                if (ColliderNodesList[i].NodeName == trimmedSetting)
                                {
                                    newSphere.NodeName = ColliderNodesList[i].NodeName;
                                    ColliderNodesList[i].CollisionSpheres.emplace_back(newSphere);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        DumpCollisionConfigsToLog();
        logger.Info("Collision Config file is loaded successfully.\n");
        return;

    }

    logger.Info("Collision Config file is not loaded.\n");
    return;
}

void ConfigLineSplitter(std::string& line, Sphere& newSphere)
{
    std::vector<std::string> splittedFloats = split(line, ',');
    if (splittedFloats.size() > 0)
    {
        newSphere.offset.x = strtof(splittedFloats[0].c_str(), 0);
    }
    if (splittedFloats.size() > 1)
    {
        newSphere.offset.y = strtof(splittedFloats[1].c_str(), 0);
    }
    if (splittedFloats.size() > 2)
    {
        newSphere.offset.z = strtof(splittedFloats[2].c_str(), 0);
    }
    if (splittedFloats.size() > 3)
    {
        newSphere.radius = strtof(splittedFloats[3].c_str(), 0);
    }
}

int GetConfigSettingsValue(std::string line, std::string& variable)
{
    int value = 0;
    std::vector<std::string> splittedLine = split(line, '=');
    variable = "";
    if (splittedLine.size() > 1)
    {
        variable = splittedLine[0];
        trim(variable);

        std::string valuestr = splittedLine[1];
        trim(valuestr);
        value = std::stoi(valuestr);
    }

    return value;
}

float GetConfigSettingsFloatValue(std::string line, std::string& variable)
{
    float value = 0;
    std::vector<std::string> splittedLine = split(line, '=');
    variable = "";
    if (splittedLine.size() > 1)
    {
        variable = splittedLine[0];
        trim(variable);

        std::string valuestr = splittedLine[1];
        trim(valuestr);
        value = strtof(valuestr.c_str(), 0);
    }

    return value;
}

std::string GetConfigSettingsStringValue(std::string line, std::string& variable)
{
    std::string valuestr = "";
    std::vector<std::string> splittedLine = split(line, '=');
    variable = "";
    if (splittedLine.size() > 0)
    {
        variable = splittedLine[0];
        trim(variable);
    }

    if (splittedLine.size() > 1)
    {
        valuestr = splittedLine[1];
        trim(valuestr);
    }

    return valuestr;
}

void printSpheresMessage(std::string message, std::vector<Sphere> spheres)
{
    for (int i = 0; i < spheres.size(); i++)
    {
        message += " Spheres: ";
        message += std::to_string(spheres[i].offset.x);
        message += ",";
        message += std::to_string(spheres[i].offset.y);
        message += ",";
        message += std::to_string(spheres[i].offset.z);
        message += ",";
        message += std::to_string(spheres[i].radius);
    }
    logger.Info("%s\n", message.c_str());
}

std::vector<std::string> ConfigLineVectorToStringVector(std::vector<ConfigLine> linesList)
{
    std::vector<std::string> outVector;

    for (int i = 0; i < linesList.size(); i++)
    {
        std::string str = linesList[i].NodeName;
        trim(str);
        outVector.emplace_back(str);
    }
    return outVector;
}

bool GetSpecificNPCConfigForActor(Actor* actor, SpecificNPCConfig& snc)
{
    if (actor != nullptr)
    {
        for (int i = 0; i < specificNPCConfigList.size(); i++)
        {
            bool correct = true;

            for (int j = 0; j < specificNPCConfigList.at(i).conditions.AndItems.size(); j++)
            {
                if (specificNPCConfigList.at(i).conditions.AndItems.at(j).single)
                {
                    if (!ConditionCheck(actor, specificNPCConfigList.at(i).conditions.AndItems.at(j)))
                    {
                        correct = false;
                        break;
                    }
                }
                else
                {
                    bool innerCorrect = false;
                    for (int o = 0; o < specificNPCConfigList.at(i).conditions.AndItems.at(j).OrItems.size(); o++)
                    {
                        if (specificNPCConfigList.at(i).conditions.AndItems.at(j).OrItems.at(o).single)
                        {
                            if (ConditionCheck(actor, specificNPCConfigList.at(i).conditions.AndItems.at(j).OrItems.at(o)))
                            {
                                innerCorrect = true;
                                break;
                            }
                        }
                    }
                    if (innerCorrect == false)
                    {
                        correct = false;
                        break;
                    }
                }
            }

            if (correct)
            {
                snc = specificNPCConfigList.at(i);
                return true;
            }
        }
    }

    return false;
}

bool GetSpecificNPCBounceConfigForActor(Actor* actor)
{
    if (actor != nullptr)
    {
        //for (int i = 0; i < specificNPCBounceConfigList.size(); i++)
        //{
        //    bool correct = true;

        //    for (int j = 0; j < specificNPCBounceConfigList.at(i).conditions.AndItems.size(); j++)
        //    {
        //        if (specificNPCBounceConfigList.at(i).conditions.AndItems.at(j).single)
        //        {
        //            if (!ConditionCheck(actor, specificNPCBounceConfigList.at(i).conditions.AndItems.at(j)))
        //            {
        //                correct = false;
        //                break;
        //            }
        //        }
        //        else
        //        {
        //            bool innerCorrect = false;
        //            for (int o = 0; o < specificNPCBounceConfigList.at(i).conditions.AndItems.at(j).OrItems.size(); o++)
        //            {
        //                if (specificNPCBounceConfigList.at(i).conditions.AndItems.at(j).OrItems.at(o).single)
        //                {
        //                    if (ConditionCheck(actor, specificNPCBounceConfigList.at(i).conditions.AndItems.at(j).OrItems.at(o)))
        //                    {
        //                        innerCorrect = true;
        //                        break;
        //                    }
        //                }
        //            }
        //            if (innerCorrect == false)
        //            {
        //                correct = false;
        //                break;
        //            }
        //        }
        //    }

        //    if (correct)
        //    {
        //        snbc = specificNPCBounceConfigList.at(i);
        //        return true;
        //    }
        //}
    }

    return false;
}

void DumpCollisionConfigsToLog()
{
    logger.Info("***** Affected Node Lines Dump *****\n");
    for (auto & line : AffectedNodeLines)
    {
        logger.Info("%s\n", line.c_str());
    }

    logger.Info("***** Collider Node Lines Dump *****\n");
    for (auto & line : ColliderNodeLines)
    {
        logger.Info("%s\n", line.c_str());
    }

    logger.Info("***** Affected Nodes Dump *****\n");
    for (auto & node : AffectedNodesList)
    {
        logger.Info("%s\n", node.NodeName.c_str());
        printSpheresMessage("", node.CollisionSpheres);
    }

    logger.Info("***** Collider Nodes Dump *****\n");
    for (auto & node : ColliderNodesList)
    {
        logger.Info("%s\n", node.NodeName.c_str());
        printSpheresMessage("", node.CollisionSpheres);
    }
}

//void LoadPlayerCollisionEventConfig()
//{
//    PlayerCollisionEventNodes.clear();
//    std::string	runtimeDirectory = GetRuntimeDirectory();
//
//    if (!runtimeDirectory.empty())
//    {
//        std::string configPath = runtimeDirectory + "Data\\F4SE\\Plugins\\CBPCPlayerCollisionEventConfig.txt";
//
//        std::ifstream file(configPath);
//        std::string line;
//        std::string currentSetting;
//        while (std::getline(file, line))
//        {
//            trim(line);
//            skipComments(line);
//            trim(line);
//            if (line.length() > 0)
//            {
//                if (line.substr(0, 1) == "[")
//                {
//                    //newsetting
//                    currentSetting = line;
//                }
//                else
//                {
//                    if (currentSetting == "[PlayerCollisionEventNodes]")
//                    {
//                        PlayerCollisionEventNodes.push_back(line);
//                    }
//                    else if (currentSetting == "[Settings]")
//                    {
//                        std::string variableName;
//                        float variableValue = GetConfigSettingsFloatValue(line, variableName);
//                        if (variableName == "MinimumCollisionDuration")
//                        {
//                            MinimumCollisionDurationForEvent = variableValue;
//                        }
//                    }
//                }
//            }
//        }
//
//        logger.Error("Player Collision Event Config file is loaded successfully.");
//        return;
//    }
//
//    logger.Error("Player Collision Event Config file is not loaded.");
//    return;
//}
