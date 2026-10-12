class CfgPatches
{
    class OpenConvoy
    {
        units[] = {"OC_ConvoyCar"};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Vehicles_Wheeled", "DayZExpansion_AI_Scripts"};
    };
};

class CfgVehicles
{
    class Hatchback_02;

    class OC_ConvoyCar: Hatchback_02
    {
        scope = 2;
        displayName = "Wild-Fields Convoy Vehicle";
        descriptionShort = "Wild-Fields server-controlled convoy vehicle";
    };
};

class CfgMods
{
    class OpenConvoy
    {
        dir = "OpenConvoy";
        name = "Wild-Fields";
        author = "Wild-Fields";
        version = "0.1.3";
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"OpenConvoy/scripts/3_Game"};
            };

            class worldScriptModule
            {
                value = "";
                files[] = {"OpenConvoy/scripts/4_World"};
            };

            class missionScriptModule
            {
                value = "";
                files[] = {"OpenConvoy/scripts/5_Mission"};
            };
        };
    };
};
