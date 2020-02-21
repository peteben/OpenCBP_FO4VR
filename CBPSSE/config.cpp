#include "config.h"
#include "INIReader.h"
#include "log.h"
#include "SimObj.h"
#include "Thing.h"
#include "Utility.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <unordered_map>

#include "f4se/GameObjects.h"
#include "f4se/GameRTTI.h"
#include "f4se_common/Utilities.h"

#define DEBUG 0
#pragma warning(disable : 4996)

int configReloadCount = 60;
bool playerOnly = false;
bool femaleOnly = false;
bool maleOnly = false;
bool npcOnly = false;
bool detectArmor = false;
bool useWhitelist = false;

config_t config;
config_t configArmor;
configOverrides_t configOverrides;
configOverrides_t configArmorOverrides;

// TODO data structure these
whitelist_t whitelist;
std::vector<std::string> raceWhitelist;

std::vector<std::string> AffectedNodeLines;
std::vector<std::string> ColliderNodeLines;



std::vector<ConfigLine> AffectedNodesList; //Nodes that can be collided with

std::vector<ConfigLine> ColliderNodesList; //Nodes that can collide nodes

std::vector<std::string> affectedBones;


int collisionSkipFrames = 0; //0
int collisionSkipFramesPelvis = 5; //5
int gridsize = 25;
int adjacencyValue = 5;
int tuningModeCollision = 0;
float actorDistance = 4194304.0f;
int malePhysics = 0;
int malePhysicsOnlyForExtraRaces = 0;
int logging = 0;

float bellybulge = 2;
float bellybulgePosition = -3;
float bellybulgemax = 10;
float bellybulgeposlowest = -12;

float bellyBulgeReturnTime = 1.5f;
std::vector<std::string> bellybulgenodesList;

float vaginaOpeningLimit = 5.0f;
float vaginaOpeningMultiplier = 4.0f;

bool LoadConfig() {
    logger.Info("loadConfig\n");

    std::set<std::string> bonesSet;

    bool reloadActors = false;
    auto playerOnlyOld = playerOnly;
    auto femaleOnlyOld = femaleOnly;
    auto maleOnlyOld = maleOnly;
    auto npcOnlyOld = npcOnly;
    auto useWhitelistOld = useWhitelist;

    boneNames.clear();
    config.clear();
    configArmor.clear();
    configOverrides.clear();
    configArmorOverrides.clear();

    // Note: Using INIReader results in a slight double read
    INIReader configReader("Data\\F4SE\\Plugins\\ocbp.ini");
    if (configReader.ParseError() < 0) {
        logger.Error("Can't load 'ocbp.ini'\n");
    }
    logger.Error("Reading CBP Config\n");

    // Read general settings
    playerOnly = configReader.GetBoolean("General", "playerOnly", false);
    npcOnly    = configReader.GetBoolean("General", "npcOnly", false);
    useWhitelist = configReader.GetBoolean("General", "useWhitelist", false);

    if (useWhitelist) {
        maleOnly = false;
        femaleOnly = false;
    }
    else {
        femaleOnly = configReader.GetBoolean("General", "femaleOnly", false);
        maleOnly = configReader.GetBoolean("General", "maleOnly", false);

    }

    reloadActors = (playerOnly ^ playerOnlyOld) ||
                    (femaleOnly ^ femaleOnlyOld) ||
                    (maleOnly ^ maleOnlyOld) ||
                    (npcOnly ^ npcOnlyOld) ||
                    (useWhitelist ^ useWhitelistOld);

    detectArmor = configReader.GetBoolean("General", "detectArmor", false);
    configReloadCount = configReader.GetInteger("Tuning", "rate", 0);

    // Read sections
    auto sections = configReader.Sections();
    for (auto sectionsIter = sections.begin(); sectionsIter != sections.end(); ++sectionsIter) {

        // Split for override section check
        auto overrideStr = std::string("Override:");
        auto splitStr = std::mismatch(overrideStr.begin(), overrideStr.end(), sectionsIter->begin());

        auto overrideAStr = std::string("Override.A:");
        auto splitAStr = std::mismatch(overrideAStr.begin(), overrideAStr.end(), sectionsIter->begin());

        if (*sectionsIter == std::string("Attach")) {
            // Get section contents
            auto sectionMap = configReader.Section(*sectionsIter);
            for (auto& valuesIter : sectionMap) {
                auto& boneName = valuesIter.first;
                auto& attachName = valuesIter.second;
                boneNames.push_back(boneName);
                // Find specified bone section and insert map values into config
                if (sections.find(attachName) != sections.end()) {
                    auto attachMapSection = configReader.Section(attachName);
                    for (auto& attachIter : attachMapSection) {
                        auto& keyName = attachIter.first;
                        config[boneName][keyName] = configReader.GetFloat(attachName, keyName, 0.0);
                    }
                }
            }
        }
        else if (*sectionsIter == std::string("Attach.A") && detectArmor) {
            // Get section contents
            auto sectionMap = configReader.Section(*sectionsIter);
            for (auto &valuesIter : sectionMap) {
                auto &boneName = valuesIter.first;
                auto &attachName = valuesIter.second;
                boneNames.push_back(boneName);
                // Find specified bone section and insert map values into configArmor
                if (sections.find(attachName) != sections.end()) {
                    auto attachMapSection = configReader.Section(attachName);
                    for (auto &attachIter : attachMapSection) {
                        auto& keyName = attachIter.first;
                        configArmor[boneName][keyName] = configReader.GetFloat(attachName, keyName, 0.0);
                    }
                }
            }
        }
        else if (*sectionsIter == std::string("Whitelist") && useWhitelist) {
            whitelist.clear();
            raceWhitelist.clear();

            // Get section contents
            auto sectionMap = configReader.Section(*sectionsIter);
            for (auto& valuesIter : sectionMap) {
                auto& boneName      = valuesIter.first;
                auto& whitelistName = valuesIter.second;

                size_t commaPos;
                do {
                    commaPos = whitelistName.find_first_of(",");
                    auto token = whitelistName.substr(0, commaPos);
                    size_t colonPos = token.find_last_of(":");
                    auto raceName = token.substr(0, colonPos);
                    auto genderStr = token.substr(colonPos + 1);

                    if (colonPos == -1) {
                        whitelist[boneName][token].male = true;
                        whitelist[boneName][token].female = true;
                        raceWhitelist.push_back(token);
                    }
                    else if (genderStr == "male") {
                        whitelist[boneName][raceName].male = true;
                        raceWhitelist.push_back(raceName);
                    }
                    else if (genderStr == "female") {
                        whitelist[boneName][raceName].female = true;
                        raceWhitelist.push_back(raceName);
                    }
                    whitelistName = whitelistName.substr(commaPos + 1);

                    //logger.Info("<token:> %s, <rest:> %s, <commaPos:> %d, <colonPos:> %d\n", token.c_str(), whitelistName.c_str(), commaPos >= 0, colonPos < 0);
                } while (commaPos != -1);
            }
        }
        else if (splitStr.first == overrideStr.end()) {
            // If section name is prefixed with "Override:", grab other half of name for bone
            auto boneName = std::string(splitStr.second, sectionsIter->end());

            // Get section contents
            auto sectionMap = configReader.Section(*sectionsIter); 
            for (auto &valuesIt : sectionMap) {
                configOverrides[boneName][valuesIt.first] = configReader.GetFloat(*sectionsIter, valuesIt.first, 0.0);
            }
        }
        else if (splitAStr.first == overrideAStr.end()) {
            // If section name is prefixed with "Override:", grab other half of name for bone
            auto boneName = std::string(splitAStr.second, sectionsIter->end());

            // Get section contents
            auto sectionMap = configReader.Section(*sectionsIter);
            for (auto& valuesIt : sectionMap) {
                configArmorOverrides[boneName][valuesIt.first] = configReader.GetFloat(*sectionsIter, valuesIt.first, 0.0);
            }
        }
    }

    // replace configs with override settings (if any)
    for (auto &boneIter : configOverrides) {
        if (config.count(boneIter.first) > 0) {
            for (auto settingIter : boneIter.second) {
                config[boneIter.first][settingIter.first] = settingIter.second;
            }
        }
    }

    // replace armor configs with override settings (if any)
    for (auto& boneIter : configArmorOverrides) {
        if (configArmor.count(boneIter.first) > 0) {
            for (auto settingIter : boneIter.second) {
                configArmor[boneIter.first][settingIter.first] = settingIter.second;
            }
        }
    }

    // Remove duplicate entries
    bonesSet = std::set<std::string>(boneNames.begin(), boneNames.end());
    boneNames.assign(bonesSet.begin(), bonesSet.end());

    logger.Error("Finished CBP Config\n");
    return reloadActors;
}

void loadMasterConfig()
{
	//MenuManager* menuManager = MenuManager::GetSingleton();
	//if (menuManager)
	//	menuManager->MenuOpenCloseEventDispatcher()->AddEventSink(&menuEvent);

	std::string	runtimeDirectory = GetRuntimeDirectory();

	if (!runtimeDirectory.empty())
	{
		config.clear();
		affectedBones.clear();
		std::string filepath = runtimeDirectory + "Data\\F4SE\\Plugins\\CBPCMasterConfig.txt";

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
						if (currentSetting == "[Settings]")
						{
							std::string variableName;
							std::string variableStrValue = GetConfigSettingsStringValue(line, variableName);
							//if (variableName == "ExtraRaces")
							//{
							//	extraRacesList = split(variableStrValue, ',');
							//}
							//else
							{
								int variableValue = GetConfigSettingsValue(line, variableName);
								if (variableName == "SkipFrames")
									collisionSkipFrames = variableValue;
								else if (variableName == "SkipFramesPelvis")
									collisionSkipFramesPelvis = variableValue;
								else if (variableName == "GridSize")
									gridsize = variableValue;
								else if (variableName == "AdjacencyValue")
									adjacencyValue = variableValue;
								else if (variableName == "TuningMode")
									tuningModeCollision = variableValue;
								else if (variableName == "ActorDistance")
								{
									float variableFloatValue = GetConfigSettingsFloatValue(line, variableName);
									actorDistance = variableFloatValue * variableFloatValue;
								}
#ifdef RUNTIME_VR_VERSION_1_4_15	
								else if (variableName == "HapticFrequency")
									hapticFrequency = variableValue;
								else if (variableName == "HapticStrength")
									hapticStrength = variableValue;
#endif
								else if (variableName == "MalePhysics")
									malePhysics = variableValue;
								else if (variableName == "MalePhysicsOnlyForExtraRaces")
									malePhysicsOnlyForExtraRaces = variableValue;
								else if (variableName == "Logging")
								{
									logging = variableValue;
								}
							}
						}
						else if (currentSetting == "[ConfigMap]")
						{
							std::string variableName;
							std::string variableValue = GetConfigSettingsStringValue(line, variableName);
							affectedBones.emplace_back(variableName);

							//if (variableValue != "")
							//{
							//	config[variableName] = variableValue;
							//	LOG("ConfigMap[%s] = %s", variableName, variableValue);
							//}
						}
					}
				}
			}

			LOG("Master Config file is loaded successfully.");
			return;
		}
	}

	LOG("Master Config file is not loaded.");
}

void loadCollisionConfig()
{
	std::string	runtimeDirectory = GetRuntimeDirectory();

	if (!runtimeDirectory.empty())
	{
		AffectedNodeLines.clear();
		ColliderNodeLines.clear();
		AffectedNodesList.clear();
		ColliderNodesList.clear();

		std::string filepath = runtimeDirectory + "Data\\SKSE\\Plugins\\CBPCollisionConfig.txt";

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
							if (variableName == "BellyBulge")
							{
								bellybulge = variableValue;
							}
							else if (variableName == "BellyBulgePosition")
							{
								bellybulgePosition = variableValue;
							}
							else if (variableName == "BellyBulgeMax")
							{
								bellybulgemax = variableValue;
							}
							else if (variableName == "BellyBulgePosLowest")
							{
								bellybulgeposlowest = variableValue;
							}
							else if (variableName == "BellyBulgeNodes")
							{
								std::string variableStrValue = GetConfigSettingsStringValue(line, variableName);
								bellybulgenodesList = split(variableStrValue, ',');
							}
							else if (variableName == "VaginaOpeningLimit")
							{
								vaginaOpeningLimit = variableValue;
							}
							else if (variableName == "VaginaOpeningMultiplier")
							{
								vaginaOpeningMultiplier = variableValue;
							}
							else if (variableName == "BellyBulgeReturnTime")
							{
								bellyBulgeReturnTime = variableValue;
							}
						}
						else if (currentSetting == "[AffectedNodes]")
						{
							AffectedNodeLines.emplace_back(line);
							ConfigLine newConfigLine;
							newConfigLine.NodeName = line;
							AffectedNodesList.emplace_back(newConfigLine);
						}
						else if (currentSetting == "[ColliderNodes]")
						{
							ColliderNodeLines.emplace_back(line);
							ConfigLine newConfigLine;
							newConfigLine.NodeName = line;
							ColliderNodesList.emplace_back(newConfigLine);
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
		}
		LOG("Collision Config file is loaded successfully.");
		return;
	}

	LOG("Collision Config file is not loaded.");
	return;
}

void loadExtraCollisionConfig()
{
	std::string	runtimeDirectory = GetRuntimeDirectory();

	if (!runtimeDirectory.empty())
	{
		//specificNPCConfigList.clear();

		std::string configPath = runtimeDirectory + "Data\\SKSE\\Plugins\\";

		auto configList = get_all_files_names_within_folder(configPath.c_str());

		for (int i = 0; i < configList.size(); i++)
		{
			std::string filename = configList.at(i);

			if (filename == "." || filename == "..")
				continue;

			if (stringStartsWith(filename, "cbpcollisionconfig") && filename != "CBPCollisionConfig.txt")
			{
				std::string msg = "File found: " + filename;
				LOG(msg.c_str());

				std::string filepath = configPath;
				filepath.append(filename);

				SpecificNPCConfig newNPCConfig;

				std::ifstream file(filepath);

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
								if (currentSetting == "[Options]")
								{
									std::string variableName;
									std::string variableValue = GetConfigSettingsStringValue(line, variableName);
									if (variableName == "Characters")
									{
										newNPCConfig.charactersList = split(variableValue, ',');
									}
									else if (variableName == "Races")
									{
										newNPCConfig.raceList = split(variableValue, ',');
									}
								}
								else if (currentSetting == "[ExtraOptions]")
								{
									std::string variableName;
									float variableValue = GetConfigSettingsFloatValue(line, variableName);
									if (variableName == "BellyBulge")
									{
										newNPCConfig.bellybulge = variableValue;
									}
									else if (variableName == "BellyBulgePosition")
									{
										newNPCConfig.bellybulgePosition = variableValue;
									}
									else if (variableName == "BellyBulgeMax")
									{
										newNPCConfig.bellybulgemax = variableValue;
									}
									else if (variableName == "BellyBulgePosLowest")
									{
										newNPCConfig.bellybulgeposlowest = variableValue;
									}
									else if (variableName == "BellyBulgeNodes")
									{
										std::string variableStrValue = GetConfigSettingsStringValue(line, variableName);
										newNPCConfig.bellybulgenodesList = split(variableStrValue, ',');
									}
								}
								else if (currentSetting == "[AffectedNodes]")
								{
									newNPCConfig.AffectedNodeLines.emplace_back(line);
									ConfigLine newConfigLine;
									newConfigLine.NodeName = line;
									newNPCConfig.AffectedNodesList.emplace_back(newConfigLine);
								}
								else if (currentSetting == "[ColliderNodes]")
								{
									newNPCConfig.ColliderNodeLines.emplace_back(line);
									ConfigLine newConfigLine;
									newConfigLine.NodeName = line;
									newNPCConfig.ColliderNodesList.emplace_back(newConfigLine);
								}
								else
								{
									Sphere newSphere;
									ConfigLineSplitter(line, newSphere);

									std::string trimmedSetting = gettrimmed(currentSetting);

									if (std::find(newNPCConfig.AffectedNodeLines.begin(), newNPCConfig.AffectedNodeLines.end(), trimmedSetting) != newNPCConfig.AffectedNodeLines.end())
									{
										for (int i = 0; i < newNPCConfig.AffectedNodesList.size(); i++)
										{
											if (newNPCConfig.AffectedNodesList[i].NodeName == trimmedSetting)
											{
												newNPCConfig.AffectedNodesList[i].CollisionSpheres.emplace_back(newSphere);
												break;
											}
										}
									}
									if (std::find(newNPCConfig.ColliderNodeLines.begin(), newNPCConfig.ColliderNodeLines.end(), trimmedSetting) != newNPCConfig.ColliderNodeLines.end())
									{
										for (int i = 0; i < newNPCConfig.ColliderNodesList.size(); i++)
										{
											if (newNPCConfig.ColliderNodesList[i].NodeName == trimmedSetting)
											{
												newNPCConfig.ColliderNodesList[i].CollisionSpheres.emplace_back(newSphere);
												break;
											}
										}
									}
								}
							}
						}
					}
					//specificNPCConfigList.emplace_back(newNPCConfig);
				}
			}
		}

		LOG("Specific collision config files(if any) are loaded successfully.");
		return;
	}

	LOG("Specific collision config files are not loaded.");
	return;
}

void ConfigLineSplitter(std::string& line, Sphere& newSphere)
{
	std::vector<std::string> lowHighSplitted = split(line, '|');
	if (lowHighSplitted.size() == 1)
	{
		std::vector<std::string> splittedFloats = split(lowHighSplitted[0], ',');
		if (splittedFloats.size() > 0)
		{
			newSphere.offset0.x = strtof(splittedFloats[0].c_str(), 0);
			newSphere.offset100.x = newSphere.offset0.x;
		}
		if (splittedFloats.size() > 1)
		{
			newSphere.offset0.y = strtof(splittedFloats[1].c_str(), 0);
			newSphere.offset100.y = newSphere.offset0.y;
		}
		if (splittedFloats.size() > 2)
		{
			newSphere.offset0.z = strtof(splittedFloats[2].c_str(), 0);
			newSphere.offset100.z = newSphere.offset0.z;
		}
		if (splittedFloats.size() > 3)
		{
			newSphere.radius0 = strtof(splittedFloats[3].c_str(), 0);
			newSphere.radius100 = newSphere.radius0;
		}
	}
	else if (lowHighSplitted.size() > 1)
	{
		std::vector<std::string> splittedFloats = split(lowHighSplitted[0], ',');
		if (splittedFloats.size() > 0)
		{
			newSphere.offset0.x = strtof(splittedFloats[0].c_str(), 0);
		}
		if (splittedFloats.size() > 1)
		{
			newSphere.offset0.y = strtof(splittedFloats[1].c_str(), 0);
		}
		if (splittedFloats.size() > 2)
		{
			newSphere.offset0.z = strtof(splittedFloats[2].c_str(), 0);
		}
		if (splittedFloats.size() > 3)
		{
			newSphere.radius0 = strtof(splittedFloats[3].c_str(), 0);
		}

		splittedFloats = split(lowHighSplitted[1], ',');
		if (splittedFloats.size() > 0)
		{
			newSphere.offset100.x = strtof(splittedFloats[0].c_str(), 0);
		}
		if (splittedFloats.size() > 1)
		{
			newSphere.offset100.y = strtof(splittedFloats[1].c_str(), 0);
		}
		if (splittedFloats.size() > 2)
		{
			newSphere.offset100.z = strtof(splittedFloats[2].c_str(), 0);
		}
		if (splittedFloats.size() > 3)
		{
			newSphere.radius100 = strtof(splittedFloats[3].c_str(), 0);
		}
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
		message += std::to_string(spheres[i].offset0.x);
		message += ",";
		message += std::to_string(spheres[i].offset0.y);
		message += ",";
		message += std::to_string(spheres[i].offset0.z);
		message += ",";
		message += std::to_string(spheres[i].radius0);
		message += " | ";
		message += std::to_string(spheres[i].offset100.x);
		message += ",";
		message += std::to_string(spheres[i].offset100.y);
		message += ",";
		message += std::to_string(spheres[i].offset100.z);
		message += ",";
		message += std::to_string(spheres[i].radius100);
	}
	LOG(message.c_str());
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

bool IsActorMale(Actor* actor)
{
	TESNPC* actorNPC = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);

	auto npcSex = actorNPC ? CALL_MEMBER_FN(actorNPC, GetSex)() : 1;

	if (npcSex == 0) //Actor is male
		return true;
	else
		return false;
}

void DumpConfigToLog()
{
    // Log contents of config
    logger.Info("***** Config Dump *****\n");
    for (auto section : config) {
        logger.Info("[%s]\n", section.first.c_str());
        for (auto setting : section.second) {
            logger.Info("%s=%f\n", setting.first.c_str(), setting.second);
        }
    }

    logger.Info("***** ConfigArmor Dump *****\n");
    for (auto section : configArmor) {
        logger.Info("[%s]\n", section.first.c_str());
        for (auto setting : section.second) {
            logger.Info("%s=%f\n", setting.first.c_str(), setting.second);
        }
    }
}

void DumpWhitelistToLog() {
    logger.Info("***** Whitelist Dump *****\n");
    for (auto section : whitelist) {
        logger.Info("[%s]\n", section.first.c_str());
        for (auto setting : section.second) {
            logger.Info("%s= female: %d, male: %d\n", setting.first.c_str(), setting.second.female, setting.second.male);
        }
    }
}



void Log(const int msgLogLevel, const char * fmt, ...)
{
	if (msgLogLevel > logging)
	{
		return;
	}

	va_list args;
	char logBuffer[4096];

	va_start(args, fmt);
	vsprintf_s(logBuffer, sizeof(logBuffer), fmt, args);
	va_end(args);

	_MESSAGE(logBuffer);
}