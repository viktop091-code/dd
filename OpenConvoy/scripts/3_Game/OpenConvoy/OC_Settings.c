class OC_VehicleConfig
{
    string ClassName = "OC_ConvoyCar";
    float DistanceBehindLeader = 0.0;
    float SideOffset = 0.0;
    ref array<string> Attachments;

    void OC_VehicleConfig()
    {
        Attachments = new array<string>;
    }
}

class OC_RouteConfig
{
    string Name = "ExampleRoute";
    ref array<string> Waypoints;

    void OC_RouteConfig()
    {
        Waypoints = new array<string>;
    }
}

class OC_Settings
{
    int ConfigVersion = 1;
    bool Enabled = false;
    int StartupDelaySeconds = 30;
    int RespawnDelaySeconds = 1800;
    int CleanupDelaySeconds = 180;
    float TargetSpeedKmh = 32.0;
    float WaypointRadiusMeters = 8.0;
    bool SpawnAICrew = true;
    int CrewPerVehicle = 2;
    bool DismountCrewWhenAttacked = true;
    ref array<string> AICharacterClasses;
    ref array<string> AILoadoutItems;
    ref array<ref OC_VehicleConfig> Vehicles;
    ref array<ref OC_RouteConfig> Routes;

    void OC_Settings()
    {
        AICharacterClasses = new array<string>;
        AILoadoutItems = new array<string>;
        Vehicles = new array<ref OC_VehicleConfig>;
        Routes = new array<ref OC_RouteConfig>;
    }
}

class OC_SettingsRepository
{
    static const string DIRECTORY = "$profile:OpenConvoy";
    static const string FILE_PATH = "$profile:OpenConvoy/settings.json";

    static OC_Settings Load()
    {
        if (!FileExist(DIRECTORY))
            MakeDirectory(DIRECTORY);

        OC_Settings settings = new OC_Settings;
        if (!FileExist(FILE_PATH))
        {
            settings = CreateDefaults();
            Save(settings);
            Print("[OpenConvoy] Created " + FILE_PATH + ". Set Enabled to true after editing the sample route.");
            return settings;
        }

        string error;
        if (!JsonFileLoader<OC_Settings>.LoadFile(FILE_PATH, settings, error))
        {
            Error("[OpenConvoy] Cannot load settings: " + error);
            return null;
        }

        return settings;
    }

    static void Save(OC_Settings settings)
    {
        string error;
        if (!JsonFileLoader<OC_Settings>.SaveFile(FILE_PATH, settings, error))
            Error("[OpenConvoy] Cannot save settings: " + error);
    }

    static OC_Settings CreateDefaults()
    {
        OC_Settings settings = new OC_Settings;

        OC_VehicleConfig leader = new OC_VehicleConfig;
        leader.Attachments.Insert("Hatchback_02_Wheel");
        leader.Attachments.Insert("Hatchback_02_Wheel");
        leader.Attachments.Insert("Hatchback_02_Wheel");
        leader.Attachments.Insert("Hatchback_02_Wheel");
        leader.Attachments.Insert("CarBattery");
        leader.Attachments.Insert("SparkPlug");
        leader.Attachments.Insert("CarRadiator");
        settings.Vehicles.Insert(leader);

        OC_VehicleConfig follower = new OC_VehicleConfig;
        follower.DistanceBehindLeader = 18.0;
        follower.Attachments.Copy(leader.Attachments);
        settings.Vehicles.Insert(follower);

        settings.AICharacterClasses.Insert("SurvivorM_Mirek");
        settings.AICharacterClasses.Insert("SurvivorM_Boris");
        settings.AICharacterClasses.Insert("SurvivorM_Denis");
        settings.AILoadoutItems.Insert("M65Jacket_Black");
        settings.AILoadoutItems.Insert("CargoPants_Black");
        settings.AILoadoutItems.Insert("WorkingBoots_Brown");
        settings.AILoadoutItems.Insert("PlateCarrierVest");
        settings.AILoadoutItems.Insert("AK74");
        settings.AILoadoutItems.Insert("Mag_AK74_30Rnd");

        OC_RouteConfig route = new OC_RouteConfig;
        route.Name = "ChernarusSouthCoastSample";
        route.Waypoints.Insert("4518 0 2438");
        route.Waypoints.Insert("4788 0 2444");
        route.Waypoints.Insert("5120 0 2380");
        route.Waypoints.Insert("5500 0 2295");
        route.Waypoints.Insert("5900 0 2240");
        route.Waypoints.Insert("6300 0 2380");
        route.Waypoints.Insert("6700 0 2570");
        route.Waypoints.Insert("7100 0 2740");
        settings.Routes.Insert(route);

        return settings;
    }
}
