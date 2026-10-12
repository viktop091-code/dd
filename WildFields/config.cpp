class CfgPatches
{
    class AIConvoy
    {
        units[] = {"AIConvoy_Car", "AIConvoy_Truck"};
        weapons[] = {};
        requiredVersion = 0.1;

        requiredAddons[] = {"DZ_Scripts", "DZ_Vehicles_Wheeled", "DayZExpansion_AI_Scripts"};
    };
};

class CfgVehicles
{
    class CarScript;

    class Offroad_02: CarScript
    {
        class SimulationModule
        {
            class Engine;
            class Clutch;
            class Gearbox;
        };
    };

    class Truck_01_Base: CarScript
    {
        class SimulationModule
        {
            class Engine;
            class Clutch;

            class Gearbox;
        };
    };

    class Truck_01_Covered: Truck_01_Base {};

    class AIConvoy_Car: Offroad_02
    {
        scope = 2;
        displayName = "Wild-Fields Convoy M1025";
        descriptionShort = "Wild-Fields AI convoy vehicle";

        class SimulationModule: SimulationModule
        {
            class Engine: Engine
            {

                torqueCurve[] = {525,0, 1000,420, 1300,560, 2500,620, 3600,520, 5120,0};
                rpmIdle = 625;
                rpmMin = 800;
                rpmClutch = 1400;
                rpmRedline = 3700;
            };

            class Clutch: Clutch
            {

                maxTorqueTransfer = 1400;
            };

            class Gearbox: Gearbox
            {

                ratios[] = {4.20, 2.20, 1.35, 0.95};
                reverse = 3.50;
            };
        };
    };

    class AIConvoy_Truck: Truck_01_Covered
    {
        scope = 2;
        displayName = "Wild-Fields Convoy Truck";
        descriptionShort = "Wild-Fields AI convoy vehicle";

        class SimulationModule: SimulationModule
        {
            class Engine: Engine
            {

                torqueCurve[] = {525,0, 1000,760, 1500,950, 2000,940, 2500,910, 3000,860, 3600,700, 4100,220};
                rpmIdle = 650;
                rpmMin = 750;
                rpmClutch = 850;

                rpmRedline = 3600;
            };

            class Clutch: Clutch
            {

                maxTorqueTransfer = 2000;
            };

            class Gearbox: Gearbox
            {

                type = "GEARBOX_MANUAL";
                reverse = 6.28;
                ratios[] = {6.19, 2.00, 1.55, 1.20};
            };
        };
    };
};

class CfgMods
{
    class AIConvoy
    {
        dir = "WildFields";
        picture = "";
        action = "https://github.com/viktop091-code/dd";
        hideName = 0;
        hidePicture = 1;
        name = "Wild-Fields";
        credits = "Wild-Fields; based on Triumph Moving AI Convoy with permission";
        author = "Wild-Fields";
        authorID = "0";
        version = "1.0.0";
        extra = 0;
        type = "mod";

        inputs = "WildFields/scripts/data/inputs.xml";

        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class worldScriptModule
            {
                value = "";
                files[] = {"WildFields/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"WildFields/scripts/5_Mission"};
            };
        };
    };
};
