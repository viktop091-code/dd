class AIConvoyGeneralSettings
{
    int ConfigVersion;
    bool Enabled;
    int InitialDelaySeconds;
    int MinIntervalSeconds;
    int MaxIntervalSeconds;
    int MaxActiveConvoys;
    bool NotifyOnSpawn;
    bool NotifyOnAmbush;
    bool ShowCrewKillFeed;
    bool TrackSpawnedVehicles;
    ref TStringArray RouteRecorderPlayers;
    float RouteRecorderSampleDistance;
    ref TStringArray TeleportPlayersToStart;
    float DespawnRadius;
    int DespawnTimeoutSeconds;
    int MaxConvoyLifetimeSeconds;
    bool KeepVehicleIfMoved;
    float FollowGapMeters;
    ref TStringArray AdminSteamIDs;

    ref TStringArray DefaultVehiclePool;
    ref TStringArray DefaultCrewPool;

    int AdvanceWarningSeconds;

    string NotificationTitle;
    string NotificationStarted;
    string NotificationStopped;
    string NotificationAdvance;

    string LootPresetsPath;

    int LBmasterPresets;

    bool MarkerEnabled;
    string MarkerName;
    string MarkerNameStopped;
    string MarkerIconName;
    bool Marker3D;
    float MarkerUpdateSeconds;
    float MarkerMinMoveMeters;

    int MarkerColorA;
    int MarkerColorR;
    int MarkerColorG;
    int MarkerColorB;

    int MarkerStoppedColorA;
    int MarkerStoppedColorR;
    int MarkerStoppedColorG;
    int MarkerStoppedColorB;

    static const int CONFIG_VERSION = 15;

    static const string DIR = "$profile:WildFields";
    static const string PATH = "$profile:WildFields/WildFieldsSettings.json";
    static const string VEHICLES_DIR = "$profile:WildFields/Vehicles";
    static const string CREWS_DIR = "$profile:WildFields/Crews";
    static const string ROUTES_DIR = "$profile:WildFields/Routes";
    static const string LOOT_PRESETS_PATH = "$profile:WildFields/LootPresets.json";

    void AIConvoyGeneralSettings()
    {
        ConfigVersion = CONFIG_VERSION;
        Enabled = true;
        InitialDelaySeconds = 300;
        MinIntervalSeconds = 3600;
        MaxIntervalSeconds = 3600;

        MaxActiveConvoys = 1;
        NotifyOnSpawn = true;
        NotifyOnAmbush = true;
        ShowCrewKillFeed = false;

        TrackSpawnedVehicles = false;
        RouteRecorderPlayers = new TStringArray;
        RouteRecorderSampleDistance = 15.0;
        TeleportPlayersToStart = new TStringArray;
        DespawnRadius = 600.0;
        DespawnTimeoutSeconds = 600;
        MaxConvoyLifetimeSeconds = 1800;
        KeepVehicleIfMoved = true;
        FollowGapMeters = 15.0;
        AdminSteamIDs = new TStringArray;

        DefaultVehiclePool = new TStringArray;
        DefaultVehiclePool.Insert("M1025");
        DefaultVehiclePool.Insert("Truck01");

        DefaultCrewPool = new TStringArray;
        DefaultCrewPool.Insert("Mercenaries");
        DefaultCrewPool.Insert("Driver");

        AdvanceWarningSeconds = 0;
        NotificationTitle = "";
        NotificationStarted = "";
        NotificationStopped = "";
        NotificationAdvance = "";

        LootPresetsPath = "";
        LBmasterPresets = 1;
        MarkerEnabled = false;
        MarkerName = "";
        MarkerNameStopped = "";
        MarkerIconName = "Car";
        Marker3D = false;
        MarkerUpdateSeconds = 5.0;
        MarkerMinMoveMeters = 100.0;

        MarkerColorA = 255;
        MarkerColorR = 255;
        MarkerColorG = 128;
        MarkerColorB = 0;

        MarkerStoppedColorA = 255;
        MarkerStoppedColorR = 255;
        MarkerStoppedColorG = 40;
        MarkerStoppedColorB = 40;
    }

    bool IsAdmin(string steamId)
    {
        return AdminSteamIDs && AdminSteamIDs.Find(steamId) != -1;
    }

    static ref AIConvoyGeneralSettings Load()
    {
        AIConvoyGeneralSettings settings = new AIConvoyGeneralSettings();
        string errorMsg;

        if (!FileExist(DIR))
            MakeDirectory(DIR);

        bool freshInstall = !FileExist(PATH);
        if (!FileExist(VEHICLES_DIR))
        {
            MakeDirectory(VEHICLES_DIR);
            if (freshInstall)
                AIConvoyConfigDir.MarkCreated(VEHICLES_DIR);
        }
        if (!FileExist(CREWS_DIR))
        {
            MakeDirectory(CREWS_DIR);
            if (freshInstall)
                AIConvoyConfigDir.MarkCreated(CREWS_DIR);
        }
        if (!FileExist(ROUTES_DIR))
        {
            MakeDirectory(ROUTES_DIR);
            if (freshInstall)
                AIConvoyConfigDir.MarkCreated(ROUTES_DIR);
        }

        if (FileExist(PATH))
        {

            Print("[Wild-Fields] Reading " + PATH);

            if (JsonFileLoader<AIConvoyGeneralSettings>.LoadFile(PATH, settings, errorMsg))
            {
                Print("[Wild-Fields] Loaded general settings from " + PATH);

                if (settings.ConfigVersion < CONFIG_VERSION)
                {
                    int oldVersion = settings.ConfigVersion;
                    settings.ConfigVersion = CONFIG_VERSION;

                    if (oldVersion < 9)
                    {
                        settings.DefaultVehiclePool = new TStringArray;
                        settings.DefaultVehiclePool.Insert("M1025");
                        settings.DefaultVehiclePool.Insert("Truck01");

                        settings.DefaultCrewPool = new TStringArray;
                        settings.DefaultCrewPool.Insert("Mercenaries");
                        settings.DefaultCrewPool.Insert("Driver");

                        Print("[Wild-Fields] Config v9: routes that name no vehicles now draw from DefaultVehiclePool (M1025, Truck01) instead of every preset on the server. Empty that list in WildFieldsSettings.json to restore the old behaviour.");
                    }

                    Print("[Wild-Fields] Settings file is config v" + oldVersion + ", this version is v" + CONFIG_VERSION + ". Your file is NOT rewritten: options added since then run at their defaults. Add a key to your file to change one (see the README).");
                }
            }
            else
                Print("[Wild-Fields] ERROR loading settings: " + errorMsg);
        }
        else
        {
            if (JsonFileLoader<AIConvoyGeneralSettings>.SaveFile(PATH, settings, errorMsg))
                Print("[Wild-Fields] Wrote default general settings to " + PATH);
            else
                Print("[Wild-Fields] ERROR writing default settings: " + errorMsg);
        }

        return settings;
    }
}

class AIConvoyCargoItem
{
    string ClassName;
    float Chance;
    ref TStringArray Attachments;

    void AIConvoyCargoItem()
    {
        ClassName = "";
        Chance = 1.0;
        Attachments = new TStringArray;
    }

    static AIConvoyCargoItem Create(string className, float chance)
    {
        AIConvoyCargoItem item = new AIConvoyCargoItem();
        item.ClassName = className;
        item.Chance = chance;
        return item;
    }

    static AIConvoyCargoItem CreateWith(string className, float chance, TStringArray attachments)
    {
        AIConvoyCargoItem item = new AIConvoyCargoItem();
        item.ClassName = className;
        item.Chance = chance;
        if (attachments)
        {
            foreach (string a : attachments)
                item.Attachments.Insert(a);
        }
        return item;
    }
}

class AIConvoyVehiclePreset
{
    string VehicleClassName;
    ref TStringArray VehicleAttachments;
    ref array<ref AIConvoyCargoItem> CargoItems;
    ref TStringArray CargoPresets;
    float VehicleDamageStopThreshold;
    float StopDamageTotal;
    bool RuinEngineOnStop;
    ref TFloatArray ManualShiftUpSpeeds;

    void AIConvoyVehiclePreset()
    {
        VehicleClassName = "AIConvoy_Car";
        VehicleAttachments = new TStringArray;
        CargoItems = new array<ref AIConvoyCargoItem>;
        CargoPresets = new TStringArray;
        VehicleDamageStopThreshold = 0.5;
        StopDamageTotal = 0;
        RuinEngineOnStop = true;
        ManualShiftUpSpeeds = new TFloatArray;
        ManualShiftUpSpeeds.Insert(8.0);
        ManualShiftUpSpeeds.Insert(17.0);
    }
}

class AIConvoyCrewPreset
{
    int NumberOfAI;
    ref TStringArray Units;
    string Faction;
    string Loadout;
    bool LootableOnDeath;
    int BodyDespawnSeconds;

    static const int BODY_DESPAWN_MIN_SECONDS = 5;

    void AIConvoyCrewPreset()
    {
        NumberOfAI = 4;
        Units = new TStringArray;
        Faction = "Mercenaries";
        Loadout = "BanditLoadout";
        LootableOnDeath = true;
        BodyDespawnSeconds = 0;
    }

    int GetBodyDespawnSeconds()
    {
        if (BodyDespawnSeconds <= 0)
            return 0;
        if (BodyDespawnSeconds < BODY_DESPAWN_MIN_SECONDS)
            return BODY_DESPAWN_MIN_SECONDS;
        return BodyDespawnSeconds;
    }
}

class AIConvoyRoute
{
    string Name;
    bool Enabled;
    float SpeedKmh;
    int VehicleCount;
    int AmbushWaypointIndex;
    string Vehicle;
    string Crew;
    ref TStringArray VehicleSlots;
    ref TStringArray CrewSlots;
    ref TStringArray VehiclePool;
    ref TStringArray CrewPool;
    string StartLocationName;
    string EndLocationName;
    ref array<vector> Waypoints;

    void AIConvoyRoute()
    {
        Name = "Unnamed Route";
        Enabled = true;
        SpeedKmh = 30.0;
        VehicleCount = 1;
        AmbushWaypointIndex = -1;
        Vehicle = "";
        Crew = "";
        VehicleSlots = new TStringArray;
        CrewSlots = new TStringArray;
        VehiclePool = new TStringArray;
        CrewPool = new TStringArray;
        StartLocationName = "";
        EndLocationName = "";
        Waypoints = new array<vector>;
    }
}

class AIConvoyVehicleSetup
{
    ref AIConvoyVehiclePreset Vehicle;
    ref AIConvoyCrewPreset Crew;

    void AIConvoyVehicleSetup(AIConvoyVehiclePreset vehicle, AIConvoyCrewPreset crew)
    {
        Vehicle = vehicle;
        Crew = crew;
    }
}

class AIConvoyEventParams
{
    string Name;
    ref array<vector> Waypoints;
    float SpeedKmh;
    int AmbushWaypointIndex;
    string StartLocationName;
    string EndLocationName;

    ref array<ref AIConvoyVehicleSetup> Setups;

    float FollowGapMeters;
    float DespawnRadius;
    int DespawnTimeoutSeconds;
    int MaxConvoyLifetimeSeconds;
    bool KeepVehicleIfMoved;
    bool ShowCrewKillFeed;
    bool TrackSpawnedVehicles;

    void AIConvoyEventParams(AIConvoyGeneralSettings general, AIConvoyRoute route, array<ref AIConvoyVehicleSetup> setups)
    {
        Name = route.Name;
        Waypoints = route.Waypoints;
        SpeedKmh = route.SpeedKmh;
        AmbushWaypointIndex = route.AmbushWaypointIndex;
        StartLocationName = route.StartLocationName;
        EndLocationName = route.EndLocationName;

        Setups = setups;

        FollowGapMeters = general.FollowGapMeters;
        DespawnRadius = general.DespawnRadius;
        DespawnTimeoutSeconds = general.DespawnTimeoutSeconds;
        MaxConvoyLifetimeSeconds = general.MaxConvoyLifetimeSeconds;
        KeepVehicleIfMoved = general.KeepVehicleIfMoved;
        ShowCrewKillFeed = general.ShowCrewKillFeed;
        TrackSpawnedVehicles = general.TrackSpawnedVehicles;
    }
}

class AIConvoyConfigDir
{
    static const string KNOWN_FILES_PATH = "$profile:WildFields/KnownConfigFiles.txt";
    static const string PROFILE_PREFIX = "$profile:";

    protected static ref map<string, ref TStringArray> s_Known;

    protected static ref TStringArray s_Reported;

    protected static ref TStringArray s_Unlisted;

    protected static ref TStringArray s_Created;

    static void MarkCreated(string dir)
    {
        if (!s_Created)
            s_Created = new TStringArray;
        if (s_Created.Find(dir) == -1)
            s_Created.Insert(dir);
    }

    static bool CanTrustEmpty(string dir)
    {
#ifdef DAYZ_1_30
        if (s_Created && s_Created.Find(dir) != -1)
            return true;
        if (s_Unlisted && s_Unlisted.Find(dir) != -1)
            return false;
#endif
        return true;
    }

    static void ListJsonFiles(string dir, out TStringArray names)
    {
        names = new TStringArray;

        if (s_Unlisted)
            s_Unlisted.RemoveItem(dir);

        if (ScanJson(dir, names))
        {
            RememberKnown(dir, names);
            return;
        }

        string relDir = RelativeProfileDir(dir);
        if (relDir != "" && ScanJson(relDir, names))
        {
            RememberKnown(dir, names);
            ReportOnce(dir, "[Wild-Fields] " + dir + ": listed through the relative path '" + relDir + "' (DayZ 1.30 FindFile workaround) - " + names.Count() + " file(s)");
            return;
        }

        if (!s_Unlisted)
            s_Unlisted = new TStringArray;
        s_Unlisted.Insert(dir);

        TStringArray known = KnownNames(dir);
        foreach (string knownName : known)
        {
            if (FileExist(dir + "/" + knownName + ".json"))
                names.Insert(knownName);
        }

#ifdef DAYZ_1_30
        if (names.Count() == 0 && !CanTrustEmpty(dir))
            ReportOnce(dir, "[Wild-Fields] ERROR: could not list " + dir + " (DayZ 1.30 FindFile change) and none of its known file names exist. Nothing is loaded from it, and NO defaults are written into it (that could overwrite your own files). Fix: start the server with -profiles=<folder INSIDE the server folder>, given as a relative path (for example -profiles=profiles).");
#endif
        if (names.Count() > 0)
            ReportOnce(dir, "[Wild-Fields] WARNING: could not list " + dir + " (DayZ 1.30 FindFile change). Loaded " + names.Count() + " file(s) from the list of known names instead. A .json you ADDED by hand will not be found. Fix: start the server with -profiles=<folder INSIDE the server folder>, given as a relative path (for example -profiles=profiles), which lets the mod list the folder again.");
    }

    protected static bool ScanJson(string dir, TStringArray names)
    {
        string fileName;
        FileAttr attr;
        FindFileHandle handle = FindFile(dir + "/*.json", fileName, attr, FindFileFlags.ALL);
        if (!handle)
            return false;

        while (true)
        {
            if (fileName != "" && !(attr & FileAttr.DIRECTORY))
            {

                int len = fileName.Length();
                if (len > 5)
                {
                    string ext = fileName.Substring(len - 5, 5);
                    ext.ToLower();

                    if (ext == ".json")
                        names.Insert(fileName.Substring(0, len - 5));
                }
            }

            if (!FindNextFile(handle, fileName, attr))
                break;
        }

        CloseFindFile(handle);
        return true;
    }

    protected static string RelativeProfileDir(string dir)
    {
        if (dir.IndexOf(PROFILE_PREFIX) != 0)
            return "";

        string profiles;
        if (!GetGame().CommandlineGetParam("profiles", profiles))
            return "";

        profiles = profiles.Trim();
        profiles.Replace("\"", "");
        profiles.Replace("\\", "/");

        while (profiles.IndexOf("./") == 0)
            profiles = profiles.Substring(2, profiles.Length() - 2);

        while (profiles.Length() > 0 && profiles.Substring(profiles.Length() - 1, 1) == "/")
            profiles = profiles.Substring(0, profiles.Length() - 1);

        if (profiles == "")
            return "";

        if (profiles.IndexOf(":") != -1 || profiles.IndexOf("/") == 0)
            return "";

        string rest = dir.Substring(PROFILE_PREFIX.Length(), dir.Length() - PROFILE_PREFIX.Length());
        return profiles + "/" + rest;
    }

    static void AddKnownName(string dir, string name)
    {
        LoadKnown();

        TStringArray list;
        if (!s_Known.Find(dir, list))
        {
            list = new TStringArray;
            s_Known.Set(dir, list);
        }

        if (list.Find(name) != -1)
            return;

        list.Insert(name);
        SaveKnown();
    }

    protected static void RememberKnown(string dir, TStringArray names)
    {
        LoadKnown();

        TStringArray old;
        bool same = s_Known.Find(dir, old) && old.Count() == names.Count();
        if (same)
        {
            foreach (string n : names)
            {
                if (old.Find(n) == -1)
                {
                    same = false;
                    break;
                }
            }
        }

        if (same)
            return;

        TStringArray copy = new TStringArray;
        copy.Copy(names);
        s_Known.Set(dir, copy);
        SaveKnown();
    }

    protected static TStringArray KnownNames(string dir)
    {
        LoadKnown();

        TStringArray list;
        if (s_Known.Find(dir, list) && list.Count() > 0)
            return list;

        list = new TStringArray;
        if (dir == AIConvoyGeneralSettings.VEHICLES_DIR)
        {
            list.Insert("M1025");
            list.Insert("Truck01");
        }
        else if (dir == AIConvoyGeneralSettings.CREWS_DIR)
        {
            list.Insert("Driver");
            list.Insert("Mercenaries");
        }
        else if (dir == AIConvoyGeneralSettings.ROUTES_DIR)
        {
            for (int i = 1; i <= 7; i++)
                list.Insert("Route" + i);
        }
        return list;
    }

    protected static void LoadKnown()
    {
        if (s_Known)
            return;

        s_Known = new map<string, ref TStringArray>;

        if (!FileExist(KNOWN_FILES_PATH))
            return;

        FileHandle fh = OpenFile(KNOWN_FILES_PATH, FileMode.READ);
        if (!fh)
            return;

        string line;
        while (FGets(fh, line) >= 0)
        {
            line = line.Trim();
            int bar = line.IndexOf("|");
            if (bar < 1 || bar >= line.Length() - 1)
                continue;

            string lineDir = line.Substring(0, bar);
            string lineName = line.Substring(bar + 1, line.Length() - bar - 1);

            TStringArray list;
            if (!s_Known.Find(lineDir, list))
            {
                list = new TStringArray;
                s_Known.Set(lineDir, list);
            }
            if (list.Find(lineName) == -1)
                list.Insert(lineName);
        }
        CloseFile(fh);
    }

    protected static void SaveKnown()
    {
        if (!s_Known)
            return;

        FileHandle fh = OpenFile(KNOWN_FILES_PATH, FileMode.WRITE);
        if (!fh)
            return;

        FPrintln(fh, "# Written by Wild-Fields. Lets the mod find its config files on DayZ 1.30, where folders cannot be listed. Safe to delete - it is rebuilt.");
        foreach (string dir, TStringArray list : s_Known)
        {
            foreach (string name : list)
                FPrintln(fh, dir + "|" + name);
        }
        CloseFile(fh);
    }

    protected static void ReportOnce(string dir, string message)
    {
        if (!s_Reported)
            s_Reported = new TStringArray;

        if (s_Reported.Find(dir) != -1)
            return;

        s_Reported.Insert(dir);

        Print("" + message);
    }
}
