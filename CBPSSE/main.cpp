#include "common/ITypes.h"
#include <string>
#include "f4se/PluginAPI.h"
#include "f4se_common/f4se_version.h"
#include "f4se_common/SafeWrite.h"
#include "f4se/GameAPI.h"
#include "f4se/GameEvents.h"
#include "log.h"
#include "config.h"
#include "PapyrusOCBP.h"


bool RegisterFuncs(VirtualMachine* vm);

PluginHandle    g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface         * g_messagingInterface = NULL;

//F4SEScaleformInterface       * g_scaleform = NULL;
//F4SESerializationInterface   * g_serialization = NULL;
F4SETaskInterface              * g_task = nullptr;
F4SEPapyrusInterface           * g_papyrus = nullptr;
//IDebugLog    gLog("Data\\F4SE\\Plugins\\hook.log");


void DoHook();



void MessageHandler(F4SEMessagingInterface::Message * msg)
{
    switch (msg->type)
    {
    case F4SEMessagingInterface::kMessage_GameDataReady:
    {
        LOG_INFO("kMessage_GameDataReady\n");
        // Load initial config
        LOG_ERROR("Loading Config");
        LoadConfig();
        LoadCollisionConfig();
        LOG_ERROR("Hooking Game");
        DoHook();
        LOG_ERROR("CBP Load Complete\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_GameLoaded:
    {
        LOG_INFO("kMessage_GameLoaded\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_NewGame:
    {
        LOG_INFO("kMessage_NewGame\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_PreLoadGame:
    {
        LOG_INFO("kMessage_PreLoadGame\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_PostLoad:
    {
        LOG_INFO("kMessage_PostLoad\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_PostPostLoad:
    {
        LOG_INFO("kMessage_PostPostLoad\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_PostLoadGame:
    {
        LOG_INFO("kMessage_PostLoadGame\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_PreSaveGame:
    {
        LOG_INFO("kMessage_PreSaveGame\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_PostSaveGame:
    {
        LOG_INFO("kMessage_PostSaveGame\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_DeleteGame:
    {
        LOG_INFO("kMessage_DeleteGame\n");
    }
    break;
    case F4SEMessagingInterface::kMessage_InputLoaded:
    {
        LOG_INFO("kMessage_InputLoaded\n");
    }
    break;

    }
}


extern "C"
{

    bool F4SEPlugin_Query(const F4SEInterface * f4se, PluginInfo * info)
    {
        LOG_INFO("OCBPC Physics F4SE Plugin\n");
        LOG_ERROR("Query called\n");


        // populate info structure
        info->infoVersion = PluginInfo::kInfoVersion;
        info->name = "OCBPC plugin";
        info->version = 24;

        // store plugin handle so we can identify ourselves later
        g_pluginHandle = f4se->GetPluginHandle();

        if (f4se->isEditor)
        {
            LOG_ERROR("loaded in editor, marking as incompatible\n");
            return false;
        }
        else if (f4se->runtimeVersion != RUNTIME_VERSION)
        {
            LOG_ERROR("unsupported runtime version %08X", f4se->runtimeVersion);
            return false;
        }
        // supported runtime version

        g_papyrus = (F4SEPapyrusInterface*)f4se->QueryInterface(kInterface_Papyrus);
        if (!g_papyrus)
        {
            _WARNING("couldn't get papyrus interface");
        }

        LOG_ERROR("Query complete\n");
        return true;
    }

    bool F4SEPlugin_Load(const F4SEInterface * f4se)
    {
        LOG_ERROR("CBPC Loading\n");

        g_task = (F4SETaskInterface *)f4se->QueryInterface(kInterface_Task);
        if (!g_task)
        {
            LOG_ERROR("Couldn't get Task interface\n");
            return false;
        }

        if (g_papyrus)
            g_papyrus->Register(RegisterFuncs);

        g_messagingInterface = (F4SEMessagingInterface*)f4se->QueryInterface(kInterface_Messaging);
        if (!g_messagingInterface)
        {
            LOG_ERROR("Couldn't get messaging interface");
            return false;
        }

        g_messagingInterface->RegisterListener(g_pluginHandle, "F4SE", MessageHandler);

        return true;
    }
};

bool RegisterFuncs(VirtualMachine* vm)
{
    papyrusOCBP::RegisterFuncs(vm);
    return true;
}

BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    return true;
}