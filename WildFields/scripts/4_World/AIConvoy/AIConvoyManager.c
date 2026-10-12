ref AIConvoyManager g_AIConvoyManager;

class AIConvoySlot
{

    AIConvoyManager m_Manager;

    int m_Index;
    ref Timer m_Timer;
    ref AIConvoyEventParams m_Pending;
    int m_PendingWarnSeconds;
    ref AIConvoyEvent m_Event;

    void AIConvoySlot(AIConvoyManager manager, int index)
    {
        m_Manager = manager;
        m_Index = index;
        m_Timer = new Timer(CALL_CATEGORY_SYSTEM);
    }

    bool IsBusy()
    {
        return m_Event != null;
    }

    void Stop()
    {
        if (m_Timer)
            m_Timer.Stop();
    }

    void OnAnnounce()
    {
        m_Manager.SlotAnnounce(this);
    }

    void OnStart()
    {
        m_Manager.SlotStart(this);
    }
}

class AIConvoyManager
{

    static const int MAX_COLUMN_VEHICLES = 6;

    static const int MAX_ACTIVE_CONVOYS = 3;

    static const int MIN_INTERVAL_MINUTES = 10;
    static const int MAX_INTERVAL_MINUTES = 180;

    protected ref AIConvoyGeneralSettings m_Settings;
    protected ref map<string, ref AIConvoyVehiclePreset> m_Vehicles;
    protected ref map<string, ref AIConvoyCrewPreset> m_Crews;
    protected ref array<ref AIConvoyRoute> m_Routes;

    protected ref array<ref AIConvoySlot> m_Slots;

    protected bool m_WarnClampLogged;

    protected ref AIConvoyMarker m_Marker;

    protected ref AIConvoyLootPresets m_LootPresets;
    protected ref AIConvoyRouteRecorder m_Recorder;

    protected ref map<string, string> m_RouteFiles;

    protected ref TStringArray m_MenuDeniedLogged;

    void AIConvoyManager()
    {
        m_Settings = AIConvoyGeneralSettings.Load();

        InstallBundledDefaults();

        LoadVehiclePresets();
        LoadCrewPresets();
        LoadRoutes();

        if (m_Settings.RouteRecorderPlayers.Count() > 0)
            m_Recorder = new AIConvoyRouteRecorder(m_Settings);

        m_Marker = new AIConvoyMarker(m_Settings);

        m_LootPresets = new AIConvoyLootPresets();
        m_LootPresets.SetUseLBmaster(m_Settings.LBmasterPresets != 0);

        string lootPath = m_Settings.LootPresetsPath;
        if (lootPath == "")
        {
            lootPath = AIConvoyGeneralSettings.LOOT_PRESETS_PATH;

            if (!FileExist(lootPath))
            {
                InstallBundledFile("LootPresets.json", lootPath);
                if (!FileExist(lootPath))
                    AIConvoyLootPresets.WriteExample(lootPath);
            }
        }

        m_LootPresets.Load(lootPath);

        if (!m_Settings.Enabled)
        {
            Print("[Wild-Fields] Disabled in settings");
            return;
        }

        if (CountEnabledRoutes() == 0)
        {
            Print("[Wild-Fields] No enabled routes in " + AIConvoyGeneralSettings.ROUTES_DIR + " — record one with the route recorder (RouteRecorderPlayers) or add route files, then set Enabled 1 in the route");
            return;
        }

        AIConvoySpawnLedger.SweepAndClear();

        SweepLeftoverVehicles();

        CreateSlots();

        int stagger = m_Settings.InitialDelaySeconds / 3;
        if (stagger < 30)
            stagger = 30;

        for (int si = 0; si < m_Slots.Count(); si++)
            ScheduleNext(m_Slots[si], m_Settings.InitialDelaySeconds + stagger * si);

        Print("[Wild-Fields] Ready: " + CountEnabledRoutes() + " route(s), " + m_Vehicles.Count() + " vehicle preset(s), " + m_Crews.Count() + " crew preset(s). First convoy in " + m_Settings.InitialDelaySeconds + " s");
    }

    protected bool IsOwnConvoyVehicleClass(string type)
    {
        type.ToLower();
        return type.IndexOf("aiconvoy_") == 0;
    }

    protected void SweepLeftoverVehicles()
    {
        int deleted = 0;
        int reportedOnly = 0;
        ref array<Object> seen = new array<Object>;

        foreach (AIConvoyRoute route : m_Routes)
        {

            for (int w = 0; w < route.Waypoints.Count(); w += 4)
            {
                array<Object> objects = new array<Object>;
                array<CargoBase> cargos = new array<CargoBase>;
                GetGame().GetObjectsAtPosition3D(route.Waypoints[w], 40.0, objects, cargos);

                foreach (Object obj : objects)
                {
                    if (!obj)
                        continue;

                    if (seen.Find(obj) != -1)
                        continue;

                    string type = obj.GetType();

                    if (IsOwnConvoyVehicleClass(type))
                    {
                        seen.Insert(obj);
                        GetGame().ObjectDelete(obj);
                        deleted++;
                        continue;
                    }

                    foreach (string presetName, AIConvoyVehiclePreset preset : m_Vehicles)
                    {
                        if (preset && preset.VehicleClassName == type)
                        {
                            seen.Insert(obj);
                            reportedOnly++;
                            break;
                        }
                    }
                }
            }
        }

        if (deleted > 0)
            Print("[Wild-Fields] STARTUP SWEEP: removed " + deleted + " leftover convoy vehicle(s) from a previous session.");

        if (reportedOnly > 0)
            Print("[Wild-Fields] STARTUP SWEEP: found " + reportedOnly + " vehicle(s) near the routes matching a preset that uses a NON-convoy class (e.g. a vanilla or modded vehicle). NOT deleted - these could belong to players. Point the preset at AIConvoy_Car / AIConvoy_Truck if you want leftovers cleaned automatically.");

        if (deleted == 0 && reportedOnly == 0)
            Print("[Wild-Fields] STARTUP SWEEP: no leftover convoy vehicles found.");
    }

    void ~AIConvoyManager()
    {

        if (m_Slots)
        {
            foreach (AIConvoySlot slot : m_Slots)
                slot.Stop();
        }

        if (m_Recorder)
            m_Recorder.Stop();
    }

    protected void InstallBundledDefaults()
    {
        TStringArray names;

        AIConvoyConfigDir.ListJsonFiles(AIConvoyGeneralSettings.VEHICLES_DIR, names);
        if (names.Count() == 0 && AIConvoyConfigDir.CanTrustEmpty(AIConvoyGeneralSettings.VEHICLES_DIR))
        {
            InstallBundledFile("Vehicles/M1025.json", AIConvoyGeneralSettings.VEHICLES_DIR + "/M1025.json");
            InstallBundledFile("Vehicles/Truck01.json", AIConvoyGeneralSettings.VEHICLES_DIR + "/Truck01.json");
        }

        AIConvoyConfigDir.ListJsonFiles(AIConvoyGeneralSettings.CREWS_DIR, names);
        if (names.Count() == 0 && AIConvoyConfigDir.CanTrustEmpty(AIConvoyGeneralSettings.CREWS_DIR))
        {
            InstallBundledFile("Crews/Mercenaries.json", AIConvoyGeneralSettings.CREWS_DIR + "/Mercenaries.json");
            InstallBundledFile("Crews/Driver.json", AIConvoyGeneralSettings.CREWS_DIR + "/Driver.json");
        }

        AIConvoyConfigDir.ListJsonFiles(AIConvoyGeneralSettings.ROUTES_DIR, names);
        if (names.Count() == 0 && AIConvoyConfigDir.CanTrustEmpty(AIConvoyGeneralSettings.ROUTES_DIR))
        {

            TStringArray routes = {"Route1", "Route2", "Route3", "Route4", "Route5", "Route6", "Route7"};
            foreach (string routeName : routes)
                InstallBundledFile("Routes/" + routeName + ".json", AIConvoyGeneralSettings.ROUTES_DIR + "/" + routeName + ".json");
        }

        string loadoutDir = "$profile:ExpansionMod/Loadouts";
        if (!FileExist("$profile:ExpansionMod"))
            MakeDirectory("$profile:ExpansionMod");
        if (!FileExist(loadoutDir))
            MakeDirectory(loadoutDir);
        if (!FileExist(loadoutDir + "/WildFieldsConvoyLoadout.json"))
            InstallBundledFile("Loadouts/WildFieldsConvoyLoadout.json", loadoutDir + "/WildFieldsConvoyLoadout.json");
    }

    protected void InstallBundledFile(string relPath, string destPath)
    {
        string srcPath = "WildFields/BundledConfigs/" + relPath;
        if (!FileExist(srcPath))
        {
            Print("[Wild-Fields] ERROR: bundled config missing from PBO: " + srcPath);
            return;
        }

        if (CopyFile(srcPath, destPath))
            Print("[Wild-Fields] Installed bundled default: " + destPath);
        else
            Print("[Wild-Fields] ERROR installing bundled default: " + destPath);
    }

    protected void LoadVehiclePresets()
    {
        m_Vehicles = new map<string, ref AIConvoyVehiclePreset>;

        TStringArray names;
        AIConvoyConfigDir.ListJsonFiles(AIConvoyGeneralSettings.VEHICLES_DIR, names);

        if (names.Count() == 0)
        {

            AIConvoyVehiclePreset preset = new AIConvoyVehiclePreset();
            preset.VehicleAttachments.Insert("Offroad_02_Wheel");
            preset.VehicleAttachments.Insert("Offroad_02_Wheel");
            preset.VehicleAttachments.Insert("Offroad_02_Wheel");
            preset.VehicleAttachments.Insert("Offroad_02_Wheel");
            preset.VehicleAttachments.Insert("Offroad_02_Wheel");
            preset.VehicleAttachments.Insert("Offroad_02_Door_1_1");
            preset.VehicleAttachments.Insert("Offroad_02_Door_1_2");
            preset.VehicleAttachments.Insert("Offroad_02_Door_2_1");
            preset.VehicleAttachments.Insert("Offroad_02_Door_2_2");
            preset.VehicleAttachments.Insert("Offroad_02_Trunk");
            preset.VehicleAttachments.Insert("Offroad_02_Hood");
            preset.VehicleAttachments.Insert("CarBattery");
            preset.VehicleAttachments.Insert("GlowPlug");
            preset.VehicleAttachments.Insert("HeadlightH7");
            preset.VehicleAttachments.Insert("HeadlightH7");

            TStringArray m4Kit = new TStringArray;
            m4Kit.Insert("M4_RISHndgrd");
            m4Kit.Insert("M4_MPBttstck");
            m4Kit.Insert("M4_CarryHandleOptic");
            m4Kit.Insert("Mag_STANAG_30Rnd");
            preset.CargoItems.Insert(AIConvoyCargoItem.CreateWith("M4A1", 1.0, m4Kit));

            preset.CargoItems.Insert(AIConvoyCargoItem.Create("AmmoBox_556x45_20Rnd", 1.0));
            preset.CargoItems.Insert(AIConvoyCargoItem.Create("AmmoBox_762x39_20Rnd", 1.0));
            preset.CargoItems.Insert(AIConvoyCargoItem.Create("Mag_STANAG_30Rnd", 1.0));
            preset.CargoItems.Insert(AIConvoyCargoItem.Create("TacticalBaconCan", 1.0));

            preset.VehicleDamageStopThreshold = 0.3;
            preset.StopDamageTotal = 6750;

            string errorMsg;
            if (AIConvoyConfigDir.CanTrustEmpty(AIConvoyGeneralSettings.VEHICLES_DIR) && !JsonFileLoader<AIConvoyVehiclePreset>.SaveFile(AIConvoyGeneralSettings.VEHICLES_DIR + "/M1025.json", preset, errorMsg))
                Print("[Wild-Fields] ERROR writing default vehicle preset: " + errorMsg);

            m_Vehicles.Set("M1025", preset);
            return;
        }

        foreach (string name : names)
        {

            string vehPath = AIConvoyGeneralSettings.VEHICLES_DIR + "/" + name + ".json";
            Print("[Wild-Fields] Reading " + vehPath);

            AIConvoyVehiclePreset loaded = new AIConvoyVehiclePreset();
            string loadError;
            if (JsonFileLoader<AIConvoyVehiclePreset>.LoadFile(vehPath, loaded, loadError))
            {
                m_Vehicles.Set(name, loaded);
                continue;
            }

            Print("[Wild-Fields] ERROR loading vehicle preset '" + name + "': " + loadError);

            AIConvoyVehiclePreset healed = LoadBundledVehicle(name);
            if (healed)
            {
                m_Vehicles.Set(name, healed);
                Print("[Wild-Fields] Using the built-in default for vehicle preset '" + name + "' until " + name + ".json is fixed. Your file was not changed - it is usually a missing comma or bracket.");
            }
        }

        Print("[Wild-Fields] Vehicle presets: " + names.Count() + " file(s), " + m_Vehicles.Count() + " loaded");
    }

    protected AIConvoyVehiclePreset LoadBundledVehicle(string name)
    {
        string bundledSrc = "WildFields/BundledConfigs/Vehicles/" + name + ".json";
        if (!FileExist(bundledSrc))
        {
            Print("[Wild-Fields] No built-in default for vehicle preset '" + name + "' - it is skipped until its file is fixed.");
            return null;
        }

        AIConvoyVehiclePreset preset = new AIConvoyVehiclePreset();
        string err;
        if (!JsonFileLoader<AIConvoyVehiclePreset>.LoadFile(bundledSrc, preset, err))
        {
            Print("[Wild-Fields] ERROR reading the built-in default for '" + name + "': " + err);
            return null;
        }
        return preset;
    }

    protected void LoadCrewPresets()
    {
        m_Crews = new map<string, ref AIConvoyCrewPreset>;

        TStringArray names;
        AIConvoyConfigDir.ListJsonFiles(AIConvoyGeneralSettings.CREWS_DIR, names);

        if (names.Count() == 0)
        {

            AIConvoyCrewPreset preset = new AIConvoyCrewPreset();

            preset.LootableOnDeath = false;

            string errorMsg;
            if (AIConvoyConfigDir.CanTrustEmpty(AIConvoyGeneralSettings.CREWS_DIR) && !JsonFileLoader<AIConvoyCrewPreset>.SaveFile(AIConvoyGeneralSettings.CREWS_DIR + "/Mercenaries.json", preset, errorMsg))
                Print("[Wild-Fields] ERROR writing default crew preset: " + errorMsg);

            m_Crews.Set("Mercenaries", preset);
            return;
        }

        foreach (string name : names)
        {
            string crewPath = AIConvoyGeneralSettings.CREWS_DIR + "/" + name + ".json";
            Print("[Wild-Fields] Reading " + crewPath);

            AIConvoyCrewPreset loaded = new AIConvoyCrewPreset();
            string loadError;
            if (JsonFileLoader<AIConvoyCrewPreset>.LoadFile(crewPath, loaded, loadError))
                m_Crews.Set(name, loaded);
            else
                Print("[Wild-Fields] ERROR loading crew preset '" + name + "': " + loadError);
        }

        Print("[Wild-Fields] Crew presets: " + names.Count() + " file(s), " + m_Crews.Count() + " loaded");
    }

    protected void LoadRoutes()
    {
        m_Routes = new array<ref AIConvoyRoute>;
        m_RouteFiles = new map<string, string>;

        TStringArray names;
        AIConvoyConfigDir.ListJsonFiles(AIConvoyGeneralSettings.ROUTES_DIR, names);

        foreach (string name : names)
        {
            string routePath = AIConvoyGeneralSettings.ROUTES_DIR + "/" + name + ".json";
            Print("[Wild-Fields] Reading " + routePath);

            AIConvoyRoute route = new AIConvoyRoute();
            string loadError;
            if (JsonFileLoader<AIConvoyRoute>.LoadFile(routePath, route, loadError))
            {
                if (route.Name == "" || route.Name == "Unnamed Route")
                    route.Name = name;

                if (m_RouteFiles.Contains(route.Name))
                    Print("[Wild-Fields] WARNING: route files '" + m_RouteFiles.Get(route.Name) + ".json' and '" + name + ".json' both use the Name '" + route.Name + "'. Only the first is reachable by name (admin menu, TriggerConvoy). Give each route a unique Name.");
                else
                    m_RouteFiles.Set(route.Name, name);

                m_Routes.Insert(route);
            }
            else
            {
                Print("[Wild-Fields] ERROR loading route '" + name + "': " + loadError);
            }
        }

        Print("[Wild-Fields] Routes: " + names.Count() + " file(s), " + m_Routes.Count() + " loaded");
    }

    protected int CountEnabledRoutes()
    {
        int count = 0;
        foreach (AIConvoyRoute route : m_Routes)
        {
            if (route.Enabled && route.Waypoints.Count() >= 2)
                count++;
        }
        return count;
    }

    protected void CreateSlots()
    {
        m_Slots = new array<ref AIConvoySlot>;

        int want = m_Settings.MaxActiveConvoys;
        if (want < 1)
            want = 1;

        if (want > MAX_ACTIVE_CONVOYS)
        {
            Print("[Wild-Fields] MaxActiveConvoys is " + m_Settings.MaxActiveConvoys + ", which is above the ceiling of " + MAX_ACTIVE_CONVOYS + " - using " + MAX_ACTIVE_CONVOYS + ". Each convoy brings its own vehicles AND crew AI, so this ceiling is about what a server can carry, not about the code.");
            want = MAX_ACTIVE_CONVOYS;
        }

        for (int i = 0; i < want; i++)
            m_Slots.Insert(new AIConvoySlot(this, i + 1));

        if (want > 1)
            Print("[Wild-Fields] MaxActiveConvoys " + want + ": " + want + " convoys may run at the same time, each on its own schedule and never on the same route.");

        int enabled = CountEnabledRoutes();
        if (want > enabled)
            Print("[Wild-Fields] MaxActiveConvoys is " + want + " but only " + enabled + " route(s) are enabled. Two convoys never run the same route, so at most " + enabled + " can be active at once and the spare slot(s) will keep waiting. Enable more routes in $profile:WildFields/Routes, or lower MaxActiveConvoys.");
    }

    int ActiveConvoyCount()
    {
        int n = 0;
        if (!m_Slots)
            return 0;

        foreach (AIConvoySlot slot : m_Slots)
        {
            if (slot.IsBusy())
                n++;
        }
        return n;
    }

    protected AIConvoySlot FindSlotForEvent(AIConvoyEvent ev)
    {
        if (!m_Slots || !ev)
            return null;

        foreach (AIConvoySlot slot : m_Slots)
        {
            if (slot.m_Event == ev)
                return slot;
        }
        return null;
    }

    protected AIConvoySlot FindFreeSlot()
    {
        if (!m_Slots)
            return null;

        foreach (AIConvoySlot slot : m_Slots)
        {
            if (!slot.IsBusy())
                return slot;
        }
        return null;
    }

    protected TStringArray RoutesInUse()
    {
        TStringArray busy = new TStringArray;
        if (!m_Slots)
            return busy;

        foreach (AIConvoySlot slot : m_Slots)
        {
            if (slot.m_Event)
                busy.Insert(slot.m_Event.GetRouteName());
            else if (slot.m_Pending)
                busy.Insert(slot.m_Pending.Name);
        }
        return busy;
    }

    protected void ScheduleNext(AIConvoySlot slot, int seconds)
    {
        if (!slot)
            return;

        if (seconds < 10)
            seconds = 10;

        int warn = m_Settings.AdvanceWarningSeconds;

        if (warn > 0 && warn >= seconds)
        {
            int clamped = seconds / 2;
            if (!m_WarnClampLogged)
            {
                m_WarnClampLogged = true;
                Print("[Wild-Fields] AdvanceWarningSeconds (" + warn + ") is >= the " + seconds + " s wait until the next convoy - warning " + clamped + " s ahead this time instead. Lower AdvanceWarningSeconds, or raise whichever wait applied (InitialDelaySeconds for the first convoy after a restart, MinIntervalSeconds after that).");
            }
            warn = clamped;
        }

        if (warn > 0)
        {

            slot.m_PendingWarnSeconds = warn;
            slot.m_Timer.Run(seconds - warn, slot, "OnAnnounce", null, false);
            return;
        }

        slot.m_PendingWarnSeconds = 0;

        slot.m_Timer.Run(seconds, slot, "OnStart", null, false);
    }

    protected string SlotLabel(AIConvoySlot slot)
    {
        if (!slot || !m_Slots || m_Slots.Count() < 2)
            return "";
        return " [convoy " + slot.m_Index + "]";
    }

    void SlotAnnounce(AIConvoySlot slot)
    {
        if (!slot)
            return;

        if (slot.IsBusy())
        {
            ScheduleNext(slot, 300);
            return;
        }

        slot.m_Pending = AssembleEvent();
        if (!slot.m_Pending)
        {
            Print("[Wild-Fields] Could not assemble a convoy event (missing route/vehicle/crew config), retrying in 30 min" + SlotLabel(slot));
            ScheduleNext(slot, 1800);
            return;
        }

        int warn = slot.m_PendingWarnSeconds;
        if (warn <= 0)
            warn = m_Settings.AdvanceWarningSeconds;

        NotifyConvoyAdvance(slot.m_Pending, warn);

        string advStart = slot.m_Pending.StartLocationName;
        if (advStart == "")
            advStart = GetNearestLocationName(slot.m_Pending.Waypoints[0]);
        if (advStart == "")
            advStart = "(no location resolved)";

        Print("[Wild-Fields] Advance warning: route '" + slot.m_Pending.Name + "' from " + advStart + " starts in " + warn + " s" + SlotLabel(slot));

        slot.m_Timer.Run(warn, slot, "OnStart", null, false);
    }

    void SlotStart(AIConvoySlot slot)
    {
        if (!slot)
            return;

        if (slot.IsBusy())
        {
            slot.m_Pending = null;
            ScheduleNext(slot, 300);
            return;
        }

        AIConvoyEventParams params = slot.m_Pending;
        slot.m_Pending = null;

        if (!params)
            params = AssembleEvent();

        if (!params)
        {
            Print("[Wild-Fields] Could not assemble a convoy event (missing route/vehicle/crew config), retrying in 30 min" + SlotLabel(slot));
            ScheduleNext(slot, 1800);
            return;
        }

        if (!LaunchEvent(slot, params))
            ScheduleNext(slot, 1800);
    }

    protected bool LaunchEvent(AIConvoySlot slot, AIConvoyEventParams params)
    {
        if (!slot)
            return false;

        slot.m_Event = new AIConvoyEvent(this, params);
        if (!slot.m_Event.Start())
        {
            slot.m_Event = null;
            return false;
        }
        return true;
    }

    bool TriggerConvoy(string routeName)
    {
        if (!m_Settings || !m_Settings.Enabled)
            return false;

        AIConvoySlot free = FindFreeSlot();
        if (!free)
        {
            Print("[Wild-Fields] TriggerConvoy('" + routeName + "') ignored — all " + m_Slots.Count() + " convoy slot(s) are already active");
            return false;
        }

        if (RoutesInUse().Find(routeName) != -1)
        {
            Print("[Wild-Fields] TriggerConvoy('" + routeName + "') ignored — that route is already running");
            return false;
        }

        AIConvoyRoute route = FindRouteByName(routeName);
        if (!route)
        {
            Print("[Wild-Fields] TriggerConvoy: no route named '" + routeName + "'");
            return false;
        }

        AIConvoyEventParams params = AssembleEventForRoute(route);
        if (!params)
        {
            Print("[Wild-Fields] TriggerConvoy: could not assemble route '" + routeName + "' (check vehicle/crew presets)");
            return false;
        }

        Print("[Wild-Fields] TriggerConvoy: spawning route '" + route.Name + "' on demand" + SlotLabel(free));
        return LaunchEvent(free, params);
    }

    bool TriggerConvoyRandom()
    {
        if (!m_Settings || !m_Settings.Enabled)
            return false;

        AIConvoySlot freeSlot = FindFreeSlot();
        if (!freeSlot)
            return false;

        AIConvoyEventParams params = AssembleEvent();
        if (!params)
            return false;
        return LaunchEvent(freeSlot, params);
    }

    protected AIConvoyRoute FindRouteByName(string routeName)
    {
        foreach (AIConvoyRoute route : m_Routes)
        {
            if (route.Name == routeName)
                return route;
        }
        return null;
    }

    bool IsAdmin(string steamId)
    {
        return m_Settings && m_Settings.IsAdmin(steamId);
    }

    void NoteMenuDenied(string steamId)
    {
        if (!m_MenuDeniedLogged)
            m_MenuDeniedLogged = new TStringArray;

        if (m_MenuDeniedLogged.Find(steamId) != -1)
            return;

        m_MenuDeniedLogged.Insert(steamId);
        Print("[Wild-Fields] Admin menu requested by " + steamId + ", who is not in AdminSteamIDs - ignored (logged once per player per session).");
    }

    AIConvoyMenuState BuildMenuState(string steamId, string status, bool closeMenu = false)
    {
        if (!IsAdmin(steamId))
            return null;

        AIConvoyMenuState msg = new AIConvoyMenuState();

        TStringArray running = RoutesRunning();

        foreach (AIConvoyRoute route : m_Routes)
        {
            AIConvoyRouteInfo info = new AIConvoyRouteInfo();
            info.Name = route.Name;
            info.Enabled = route.Enabled;
            info.Running = running.Find(route.Name) != -1;
            info.WaypointCount = route.Waypoints.Count();
            info.Column = BuildColumnLabel(route);
            msg.Routes.Insert(info);
        }

        msg.MinIntervalMinutes = m_Settings.MinIntervalSeconds / 60;
        msg.MaxIntervalMinutes = m_Settings.MaxIntervalSeconds / 60;

        msg.RecorderArmed = m_Recorder && m_Recorder.IsArmedId(steamId);
        msg.RecorderRecording = m_Recorder && m_Recorder.IsRecording(steamId);

        msg.ActiveConvoys = ActiveConvoyCount();
        msg.MaxActiveConvoys = 1;
        if (m_Slots)
            msg.MaxActiveConvoys = m_Slots.Count();

        msg.Status = status;
        msg.CloseMenu = closeMenu;
        return msg;
    }

    string HandleMenuAction(string steamId, string adminName, AIConvoyMenuAction act, out bool closeMenu)
    {
        closeMenu = false;

        if (!act)
            return "Could not read that request.";

        string who = adminName + " (" + steamId + ")";

        if (act.Type == AIConvoyMenuActionType.TOGGLE_ROUTE)
            return SetRouteEnabled(act.RouteName, act.Value, who);

        if (act.Type == AIConvoyMenuActionType.LAUNCH_ROUTE)
            return LaunchFromMenu(act.RouteName, who);

        if (act.Type == AIConvoyMenuActionType.SET_INTERVAL)
            return ApplyIntervalMinutes(act.MinMinutes, act.MaxMinutes, who);

        if (act.Type == AIConvoyMenuActionType.SET_RECORDER)
            return SetRecorderArmed(steamId, adminName, act.Value, who);

        if (act.Type == AIConvoyMenuActionType.TELEPORT_TO)
        {
            bool teleported;
            string result = TeleportToConvoy(steamId, act.RouteName, who, teleported);
            closeMenu = teleported;
            return result;
        }

        if (act.Type == AIConvoyMenuActionType.STOP_ROUTE)
            return StopFromMenu(act.RouteName, who);

        return "Unknown action.";
    }

    protected string StopFromMenu(string routeName, string who)
    {
        AIConvoyEvent ev = FindRunningEvent(routeName);
        if (!ev)
            return "'" + routeName + "' is not running.";

        Print("[Wild-Fields] Admin menu: " + who + " stopped convoy '" + routeName + "'");

        int spared = ev.StopByAdmin();
        if (spared > 0)
            return "Stopped '" + routeName + "'. " + spared + " vehicle(s) with a player inside were left in the world.";

        return "Stopped and despawned '" + routeName + "'.";
    }

    protected string TeleportToConvoy(string steamId, string routeName, string who, out bool teleported)
    {
        teleported = false;

        AIConvoyEvent ev = FindRunningEvent(routeName);
        if (!ev)
            return "'" + routeName + "' is not running.";

        PlayerBase admin = FindPlayerBySteamId(steamId);
        if (!admin || !admin.IsAlive())
            return "Could not find your character on the server.";

        if (Transport.Cast(admin.GetParent()))
            return "Get out of your vehicle first, then use Go to.";

        vector leadPos;
        vector leadDir;
        if (!ev.GetLeadPose(leadPos, leadDir))
            return "'" + routeName + "' has no vehicles left to go to.";

        vector pos = leadPos + leadDir * 40.0 + leadDir.Perpend() * 20.0;
        pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.5;
        admin.SetPosition(pos);

        teleported = true;
        Print("[Wild-Fields] Admin menu: " + who + " teleported to convoy '" + routeName + "'");
        return "Teleported beside '" + routeName + "'.";
    }

    protected TStringArray RoutesRunning()
    {
        TStringArray running = new TStringArray;
        if (!m_Slots)
            return running;

        foreach (AIConvoySlot slot : m_Slots)
        {
            if (slot.m_Event)
                running.Insert(slot.m_Event.GetRouteName());
        }
        return running;
    }

    protected AIConvoyEvent FindRunningEvent(string routeName)
    {
        if (!m_Slots)
            return null;

        foreach (AIConvoySlot slot : m_Slots)
        {
            if (slot.m_Event && slot.m_Event.GetRouteName() == routeName)
                return slot.m_Event;
        }
        return null;
    }

    protected PlayerBase FindPlayerBySteamId(string steamId)
    {
        array<Man> players = new array<Man>;
        GetGame().GetWorld().GetPlayerList(players);

        foreach (Man man : players)
        {
            PlayerIdentity identity = man.GetIdentity();
            if (identity && identity.GetPlainId() == steamId)
                return PlayerBase.Cast(man);
        }
        return null;
    }

    protected string SetRouteEnabled(string routeName, bool enabled, string who)
    {
        AIConvoyRoute route = FindRouteByName(routeName);
        if (!route)
            return "No route named '" + routeName + "'.";

        if (route.Enabled == enabled)
            return "'" + routeName + "' was already " + OnOff(enabled) + ".";

        route.Enabled = enabled;

        string saveError = SaveRoute(route);
        if (saveError != "")
        {

            route.Enabled = !enabled;
            return "Could not save '" + routeName + "': " + saveError;
        }

        Print("[Wild-Fields] Admin menu: " + who + " set route '" + routeName + "' " + OnOff(enabled));
        return "'" + routeName + "' " + OnOff(enabled) + ".";
    }

    protected string LaunchFromMenu(string routeName, string who)
    {
        if (!FindRouteByName(routeName))
            return "No route named '" + routeName + "'.";

        if (RoutesInUse().Find(routeName) != -1)
            return "'" + routeName + "' is already running (or announced to start).";

        if (!FindFreeSlot())
            return "All " + m_Slots.Count() + " convoy slot(s) are busy - raise MaxActiveConvoys or wait for one to finish.";

        Print("[Wild-Fields] Admin menu: " + who + " launched route '" + routeName + "'");
        if (TriggerConvoy(routeName))
            return "Launched '" + routeName + "'.";

        return "'" + routeName + "' could not be launched - check the server log.";
    }

    protected string ApplyIntervalMinutes(int minMinutes, int maxMinutes, string who)
    {

        string note = "";

        if (minMinutes > maxMinutes)
        {
            int swapped = minMinutes;
            minMinutes = maxMinutes;
            maxMinutes = swapped;
            note = note + " Min and max were swapped.";
        }

        if (minMinutes < MIN_INTERVAL_MINUTES)
        {
            minMinutes = MIN_INTERVAL_MINUTES;
            note = note + " Minimum is " + MIN_INTERVAL_MINUTES + " min.";
        }

        if (maxMinutes < MIN_INTERVAL_MINUTES)
            maxMinutes = MIN_INTERVAL_MINUTES;

        if (maxMinutes > MAX_INTERVAL_MINUTES)
        {
            maxMinutes = MAX_INTERVAL_MINUTES;
            note = note + " WARNING: capped at the " + (MAX_INTERVAL_MINUTES / 60) + "-hour maximum.";
        }

        if (minMinutes > MAX_INTERVAL_MINUTES)
            minMinutes = MAX_INTERVAL_MINUTES;

        m_Settings.MinIntervalSeconds = minMinutes * 60;
        m_Settings.MaxIntervalSeconds = maxMinutes * 60;

        string saveError = SaveSettings();
        if (saveError != "")
            return "Could not save the settings: " + saveError;

        Print("[Wild-Fields] Admin menu: " + who + " set time between convoys to " + minMinutes + "-" + maxMinutes + " min" + note);
        return "Time between convoys: " + minMinutes + "-" + maxMinutes + " min (from each convoy's next cycle)." + note;
    }

    protected string SetRecorderArmed(string steamId, string adminName, bool armed, string who)
    {
        if (armed)
        {
            EnsureRecorder();
            m_Recorder.Arm(steamId);
            Print("[Wild-Fields] Admin menu: " + who + " armed the route recorder");
            return "Recorder armed - it starts when you next drive a vehicle, and saves when you get out.";
        }

        if (!m_Recorder)
            return "Recorder was not armed.";

        bool saved = m_Recorder.Disarm(steamId);
        Print("[Wild-Fields] Admin menu: " + who + " disarmed the route recorder");

        if (m_Recorder.IsArmedBySettings(steamId, adminName))
            return "Menu arming removed, but you are still listed in RouteRecorderPlayers in WildFieldsSettings.json, so recording stays on. Remove yourself there to stop it.";

        if (saved)
            return "Recorder disarmed - the drive in progress was saved as a new, disabled route.";
        return "Recorder disarmed.";
    }

    protected void EnsureRecorder()
    {
        if (!m_Recorder)
            m_Recorder = new AIConvoyRouteRecorder(m_Settings);
    }

    void RegisterRecordedRoute(AIConvoyRoute route, string fileBase)
    {
        if (!route || !m_Routes || !m_RouteFiles)
            return;

        if (FindRouteByName(route.Name))
            return;

        m_Routes.Insert(route);
        m_RouteFiles.Set(route.Name, fileBase);
    }

    protected string SaveRoute(AIConvoyRoute route)
    {
        string fileBase;
        if (!m_RouteFiles || !m_RouteFiles.Find(route.Name, fileBase))
            return "its file is not known";

        string path = AIConvoyGeneralSettings.ROUTES_DIR + "/" + fileBase + ".json";
        string backup = path + ".bak-menu";
        if (FileExist(path) && !FileExist(backup))
            CopyFile(path, backup);

        string error;
        if (!JsonFileLoader<AIConvoyRoute>.SaveFile(path, route, error))
            return error;

        return "";
    }

    protected string SaveSettings()
    {
        string error;
        if (!JsonFileLoader<AIConvoyGeneralSettings>.SaveFile(AIConvoyGeneralSettings.PATH, m_Settings, error))
            return error;
        return "";
    }

    protected string OnOff(bool on)
    {
        if (on)
            return "enabled";
        return "disabled";
    }

    protected string BuildColumnLabel(AIConvoyRoute route)
    {
        if (route.VehicleSlots.Count() > 0)
        {
            string s = "";
            for (int i = 0; i < route.VehicleSlots.Count(); i++)
            {
                if (i > 0)
                    s += ", ";
                string v = route.VehicleSlots[i];
                if (v == "")
                    v = "random";
                s += v;
            }
            return s;
        }

        int n = Math.Clamp(route.VehicleCount, 1, MAX_COLUMN_VEHICLES);
        if (route.Vehicle != "")
            return "" + n + "x " + route.Vehicle;
        return "" + n + "x random";
    }

    static bool Trigger(string routeName)
    {
        if (!g_AIConvoyManager)
            return false;
        return g_AIConvoyManager.TriggerConvoy(routeName);
    }

    static bool TriggerRandom()
    {
        if (!g_AIConvoyManager)
            return false;
        return g_AIConvoyManager.TriggerConvoyRandom();
    }

    protected AIConvoyEventParams AssembleEvent()
    {

        TStringArray inUse = RoutesInUse();

        array<AIConvoyRoute> valid = new array<AIConvoyRoute>;
        array<AIConvoyRoute> free  = new array<AIConvoyRoute>;

        foreach (AIConvoyRoute candidate : m_Routes)
        {
            if (!candidate.Enabled || candidate.Waypoints.Count() < 2)
                continue;

            valid.Insert(candidate);

            if (inUse.Find(candidate.Name) == -1)
                free.Insert(candidate);
        }

        if (valid.Count() == 0)
            return null;

        if (free.Count() == 0)
            return null;

        return AssembleEventForRoute(free.GetRandomElement());
    }

    protected TStringArray ResolveVehicleNames(TStringArray names, string label)
    {
        TStringArray usable = new TStringArray;
        if (!names)
            return usable;

        foreach (string vehName : names)
        {
            if (vehName == "")
                continue;

            if (m_Vehicles.Get(vehName))
                usable.Insert(vehName);
            else
                Print("[Wild-Fields] " + label + " names unknown vehicle preset '" + vehName + "' - ignoring it");
        }

        return usable;
    }

    protected TStringArray ResolveCrewNames(TStringArray names, string label)
    {
        TStringArray usable = new TStringArray;
        if (!names)
            return usable;

        foreach (string crName : names)
        {
            if (crName == "")
                continue;

            if (m_Crews.Get(crName))
                usable.Insert(crName);
            else
                Print("[Wild-Fields] " + label + " names unknown crew preset '" + crName + "' - ignoring it");
        }

        return usable;
    }

    protected TStringArray ResolveVehiclePool(AIConvoyRoute route)
    {
        TStringArray usable = ResolveVehicleNames(route.VehiclePool, "Route '" + route.Name + "' VehiclePool");
        if (usable.Count() > 0)
            return usable;

        return ResolveVehicleNames(m_Settings.DefaultVehiclePool, "DefaultVehiclePool (WildFieldsSettings.json)");
    }

    protected TStringArray ResolveCrewPool(AIConvoyRoute route)
    {
        TStringArray usableCrew = ResolveCrewNames(route.CrewPool, "Route '" + route.Name + "' CrewPool");
        if (usableCrew.Count() > 0)
            return usableCrew;

        return ResolveCrewNames(m_Settings.DefaultCrewPool, "DefaultCrewPool (WildFieldsSettings.json)");
    }

    protected AIConvoyEventParams AssembleEventForRoute(AIConvoyRoute route)
    {
        if (!route || route.Waypoints.Count() < 2)
            return null;

        int count;
        if (route.VehicleSlots.Count() > 0)
            count = Math.Clamp(route.VehicleSlots.Count(), 1, MAX_COLUMN_VEHICLES);
        else
            count = Math.Clamp(route.VehicleCount, 1, MAX_COLUMN_VEHICLES);

        TStringArray vehiclePool = ResolveVehiclePool(route);
        TStringArray crewPool = ResolveCrewPool(route);

        AIConvoyVehiclePreset pinnedVehicle;
        string pinnedVehicleName = route.Vehicle;
        if (route.Vehicle != "")
        {
            pinnedVehicle = m_Vehicles.Get(route.Vehicle);
            if (!pinnedVehicle)
            {
                pinnedVehicle = PickAnyVehicle(vehiclePool, pinnedVehicleName);
                Print("[Wild-Fields] Route '" + route.Name + "' pins unknown vehicle preset '" + route.Vehicle + "' - substituting '" + pinnedVehicleName + "' for the whole column");
            }
        }

        AIConvoyCrewPreset pinnedCrew;
        string pinnedCrewName = route.Crew;
        if (route.Crew != "")
        {
            pinnedCrew = m_Crews.Get(route.Crew);
            if (!pinnedCrew)
            {
                pinnedCrew = PickAnyCrew(crewPool, pinnedCrewName);
                Print("[Wild-Fields] Route '" + route.Name + "' pins unknown crew preset '" + route.Crew + "' - substituting '" + pinnedCrewName + "' for the whole column");
            }
        }

        array<ref AIConvoyVehicleSetup> setups = new array<ref AIConvoyVehicleSetup>;
        string columnLog = "";

        for (int i = 0; i < count; i++)
        {
            AIConvoyVehiclePreset vehicle;
            string vehicleName = "";
            if (i < route.VehicleSlots.Count() && route.VehicleSlots[i] != "")
            {
                vehicle = m_Vehicles.Get(route.VehicleSlots[i]);
                vehicleName = route.VehicleSlots[i];
                if (!vehicle)
                    Print("[Wild-Fields] Route '" + route.Name + "' slot " + i + " names unknown vehicle preset '" + route.VehicleSlots[i] + "', using a fallback");
            }
            if (!vehicle && pinnedVehicle)
            {
                vehicle = pinnedVehicle;
                vehicleName = pinnedVehicleName;
            }
            if (!vehicle)
                vehicle = PickAnyVehicle(vehiclePool, vehicleName);

            AIConvoyCrewPreset crew;
            string crewName = "";
            if (i < route.CrewSlots.Count() && route.CrewSlots[i] != "")
            {
                crew = m_Crews.Get(route.CrewSlots[i]);
                crewName = route.CrewSlots[i];
                if (!crew)
                    Print("[Wild-Fields] Route '" + route.Name + "' slot " + i + " names unknown crew preset '" + route.CrewSlots[i] + "', using a fallback");
            }
            if (!crew && pinnedCrew)
            {
                crew = pinnedCrew;
                crewName = pinnedCrewName;
            }
            if (!crew)
                crew = PickAnyCrew(crewPool, crewName);

            if (!vehicle || !crew)
                return null;

            setups.Insert(new AIConvoyVehicleSetup(vehicle, crew));

            if (columnLog != "")
                columnLog = columnLog + ", ";
            columnLog = columnLog + vehicleName + "/" + crewName;
        }

        Print("[Wild-Fields] Route '" + route.Name + "' column: " + columnLog);

        return new AIConvoyEventParams(m_Settings, route, setups);
    }

    protected AIConvoyVehiclePreset PickAnyVehicle(TStringArray pool, out string pickedName)
    {
        pickedName = "";

        if (pool && pool.Count() > 0)
            pickedName = pool.GetRandomElement();
        else if (m_Vehicles.Count() > 0)
            pickedName = m_Vehicles.GetKey(Math.RandomInt(0, m_Vehicles.Count()));

        if (pickedName == "")
            return null;

        return m_Vehicles.Get(pickedName);
    }

    protected AIConvoyCrewPreset PickAnyCrew(TStringArray pool, out string pickedName)
    {
        pickedName = "";

        if (pool && pool.Count() > 0)
            pickedName = pool.GetRandomElement();
        else if (m_Crews.Count() > 0)
            pickedName = m_Crews.GetKey(Math.RandomInt(0, m_Crews.Count()));

        if (pickedName == "")
            return null;

        return m_Crews.Get(pickedName);
    }

    AIConvoyLootPresets GetLootPresets()
    {
        return m_LootPresets;
    }

    void OnEventEnded(AIConvoyEvent ev)
    {

        AIConvoySlot slot = FindSlotForEvent(ev);
        if (slot)
            slot.m_Event = null;

        if (ActiveConvoyCount() == 0)
            MarkerEnd();

        if (!slot)
            return;

        int wait = Math.RandomIntInclusive(m_Settings.MinIntervalSeconds, m_Settings.MaxIntervalSeconds);
        ScheduleNext(slot, wait);
        Print("[Wild-Fields] Next convoy in " + wait + " s" + SlotLabel(slot));
    }

    void TeleportDebugPlayers(vector startPos, vector routeDir)
    {
        if (m_Settings.TeleportPlayersToStart.Count() == 0)
            return;

        vector side = routeDir.Perpend();

        array<Man> players = new array<Man>;
        GetGame().GetWorld().GetPlayerList(players);

        int idx = 0;
        foreach (Man man : players)
        {
            PlayerIdentity identity = man.GetIdentity();
            if (!identity)
                continue;

            if (m_Settings.TeleportPlayersToStart.Find(identity.GetName()) == -1 && m_Settings.TeleportPlayersToStart.Find(identity.GetPlainId()) == -1)
                continue;

            vector pos = startPos + side * 20.0 + routeDir * (idx * 2.0);
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.5;
            man.SetPosition(pos);
            Print("[Wild-Fields] Teleported " + identity.GetName() + " to the convoy start (TeleportPlayersToStart)");
            idx++;
        }
    }

    protected ref array<ref ExpansionLocation> m_Locations;
    protected ref array<ref ExpansionLocation> m_Settlements;

    protected const float SETTLEMENT_PREFER_RANGE = 1500.0;

    protected bool m_LocationsWarned;
    protected bool m_LocationsLogged;

    protected string GetNearestLocationName(vector pos)
    {

        if (!m_Locations || m_Locations.Count() == 0)
        {

            m_Locations = ExpansionLocation.GetWorldLocations(0);

            m_Settlements = new array<ref ExpansionLocation>;
            if (m_Locations)
            {
                foreach (ExpansionLocation cand : m_Locations)
                {
                    if (!cand || cand.Name == "")
                        continue;

                    if (ExpansionLocation.SETTLEMENTS.Find(cand.Type) != -1)
                        m_Settlements.Insert(cand);
                }
            }

            if (m_Settlements.Count() == 0 && m_Locations && m_Locations.Count() > 0)
                Print("[Wild-Fields] No towns found on this map - convoy announcements will use the nearest named place of any kind instead.");

            if (!m_Locations || m_Locations.Count() == 0)
            {
                if (!m_LocationsWarned)
                {
                    m_LocationsWarned = true;
                    Print("[Wild-Fields] This map reports no locations at all, so convoy announcements cannot name places. Set StartLocationName and EndLocationName on your routes to name them yourself.");
                }
                return "";
            }

            if (!m_LocationsLogged)
            {
                m_LocationsLogged = true;
                string sample = "";
                int shown = 0;
                foreach (ExpansionLocation probe : m_Locations)
                {
                    if (shown >= 3)
                        break;
                    if (sample != "")
                        sample = sample + ", ";
                    sample = sample + "'" + probe.Name + "' (" + probe.Type + ")";
                    shown++;
                }
                int townCount = 0;
                if (m_Settlements)
                    townCount = m_Settlements.Count();

                Print("[Wild-Fields] Location lookup ready: " + m_Locations.Count() + " place(s), " + townCount + " of them towns. Examples: " + sample);
            }
        }

        float townDistSq;
        string town = NearestPlaceIn(m_Settlements, pos, townDistSq);

        if (town != "" && townDistSq >= 0 && townDistSq <= SETTLEMENT_PREFER_RANGE * SETTLEMENT_PREFER_RANGE)
            return town;

        float anyDistSq;
        string anywhere = NearestPlaceIn(m_Locations, pos, anyDistSq);
        if (anywhere != "")
            return anywhere;

        return town;
    }

    protected string NearestPlaceIn(array<ref ExpansionLocation> places, vector pos, out float outDistSq)
    {
        outDistSq = -1;
        string best = "";

        if (!places)
            return "";

        foreach (ExpansionLocation loc : places)
        {
            if (!loc || loc.Name == "")
                continue;

            float distSq = ExpansionMath.Distance2DSq(pos, loc.Position);
            if (outDistSq < 0 || distSq < outDistSq)
            {
                outDistSq = distSq;
                best = loc.Name;
            }
        }

        return best;
    }

    protected string GetRouteStartName(AIConvoyRoute route, vector pos)
    {
        if (route && route.StartLocationName != "")
            return route.StartLocationName;
        return GetNearestLocationName(pos);
    }

    protected string GetRouteEndName(AIConvoyRoute route, vector pos)
    {
        if (route && route.EndLocationName != "")
            return route.EndLocationName;
        return GetNearestLocationName(pos);
    }

    protected string GetCompassWord(vector dir)
    {
        float yaw = Math.NormalizeAngle(dir.VectorToAngles()[0]);

        if (yaw >= 337.5 || yaw < 22.5) return "north";
        if (yaw < 67.5) return "north-east";
        if (yaw < 112.5) return "east";
        if (yaw < 157.5) return "south-east";
        if (yaw < 202.5) return "south";
        if (yaw < 247.5) return "south-west";
        if (yaw < 292.5) return "west";
        return "north-west";
    }

    protected string ApplyPlaceholders(string text, string startLoc, string endLoc, string heading, string routeName, int minutes, int vehicles)
    {
        string outText = text;

        string minuteS = "s";
        if (minutes == 1)
            minuteS = "";

        string vehicleS = "s";
        if (vehicles == 1)
            vehicleS = "";

        outText.Replace("{start}", startLoc);
        outText.Replace("{end}", endLoc);
        outText.Replace("{heading}", heading);
        outText.Replace("{route}", routeName);
        outText.Replace("{minutes}", minutes.ToString());
        outText.Replace("{vehicles}", vehicles.ToString());
        outText.Replace("{s}", minuteS);
        outText.Replace("{vs}", vehicleS);

        return outText;
    }

    protected void SendNotification(string custom, CF_Localiser builtIn)
    {
        CF_Localiser body = builtIn;
        if (custom != "")
            body = new CF_Localiser(custom);

        CF_Localiser title = new CF_Localiser("STR_AICONVOY_TITLE");
        if (m_Settings.NotificationTitle != "")
            title = new CF_Localiser(m_Settings.NotificationTitle);

        ExpansionNotification(title, body, EXPANSION_NOTIFICATION_ICON_CAR, COLOR_EXPANSION_NOTIFICATION_ORANGE, 10).Create();
    }

    protected string GetCompassName(vector dir)
    {
        float yaw = Math.NormalizeAngle(dir.VectorToAngles()[0]);

        if (yaw >= 337.5 || yaw < 22.5) return "STR_AICONVOY_DIR_N";
        if (yaw < 67.5) return "STR_AICONVOY_DIR_NE";
        if (yaw < 112.5) return "STR_AICONVOY_DIR_E";
        if (yaw < 157.5) return "STR_AICONVOY_DIR_SE";
        if (yaw < 202.5) return "STR_AICONVOY_DIR_S";
        if (yaw < 247.5) return "STR_AICONVOY_DIR_SW";
        if (yaw < 292.5) return "STR_AICONVOY_DIR_W";
        return "STR_AICONVOY_DIR_NW";
    }

    void NotifyConvoyStarted(AIConvoyEventParams params)
    {
        if (!m_Settings.NotifyOnSpawn)
            return;

        vector startPos = params.Waypoints[0];
        vector endPos = params.Waypoints[params.Waypoints.Count() - 1];

        string startLoc = params.StartLocationName;
        if (startLoc == "")
            startLoc = GetNearestLocationName(startPos);

        string endLoc = params.EndLocationName;
        if (endLoc == "")
            endLoc = GetNearestLocationName(endPos);

        string heading = GetCompassName(endPos - startPos);

        CF_Localiser text;
        if (startLoc != "" && endLoc != "" && endLoc != startLoc)
            text = new CF_Localiser("STR_AICONVOY_STARTED_FULL", startLoc, heading, endLoc);
        else if (startLoc != "")
            text = new CF_Localiser("STR_AICONVOY_STARTED_NEAR", startLoc, heading);
        else
            text = new CF_Localiser("STR_AICONVOY_STARTED_DIR", heading);

        string custom = "";
        if (m_Settings.NotificationStarted != "")
            custom = ApplyPlaceholders(m_Settings.NotificationStarted, startLoc, endLoc, GetCompassWord(endPos - startPos), params.Name, 0, params.Setups.Count());

        SendNotification(custom, text);
    }

    void MarkerBegin(vector pos)
    {
        if (!m_Marker || !m_Settings.MarkerEnabled)
            return;

        m_Marker.Begin(pos);

        int ms = Math.Round(m_Settings.MarkerUpdateSeconds * 1000.0);
        if (ms < 1000)
            ms = 1000;

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(MarkerTick);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MarkerTick, ms, true);
    }

    void MarkerTick()
    {

        AIConvoyEvent tracked = null;
        if (m_Slots)
        {
            foreach (AIConvoySlot slot : m_Slots)
            {
                if (slot.m_Event)
                {
                    tracked = slot.m_Event;
                    break;
                }
            }
        }

        if (!m_Marker || !tracked)
        {
            MarkerEnd();
            return;
        }

        vector pos = tracked.GetConvoyCenter();
        if (pos == vector.Zero)
            return;

        m_Marker.MoveTo(pos);
    }

    void MarkerStopped(vector pos)
    {
        if (!m_Marker)
            return;

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(MarkerTick);
        m_Marker.SetStopped(pos);
    }

    void MarkerEnd()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(MarkerTick);
        if (m_Marker)
            m_Marker.Remove();
    }

    void NotifyConvoyAdvance(AIConvoyEventParams params, int seconds)
    {
        vector startPos = params.Waypoints[0];
        vector endPos = params.Waypoints[params.Waypoints.Count() - 1];

        string startLoc = params.StartLocationName;
        if (startLoc == "")
            startLoc = GetNearestLocationName(startPos);

        string endLoc = params.EndLocationName;
        if (endLoc == "")
            endLoc = GetNearestLocationName(endPos);

        int minutes = Math.Round(seconds / 60.0);
        if (minutes < 1)
            minutes = 1;

        string headingKey = GetCompassName(endPos - startPos);

        CF_Localiser text;
        if (startLoc != "")
            text = new CF_Localiser("STR_AICONVOY_ADVANCE_NEAR", startLoc, minutes.ToString(), headingKey);
        else
            text = new CF_Localiser("STR_AICONVOY_ADVANCE", minutes.ToString(), headingKey);

        string custom = "";
        if (m_Settings.NotificationAdvance != "")
            custom = ApplyPlaceholders(m_Settings.NotificationAdvance, startLoc, endLoc, GetCompassWord(endPos - startPos), params.Name, minutes, params.Setups.Count());

        SendNotification(custom, text);
    }

    void NotifyConvoyStopped(AIConvoyEvent ev, vector stopPos)
    {
        if (!m_Settings.NotifyOnAmbush)
            return;

        string loc = GetNearestLocationName(stopPos);

        CF_Localiser text;
        if (loc != "")
            text = new CF_Localiser("STR_AICONVOY_STOPPED_NEAR", loc);
        else
            text = new CF_Localiser("STR_AICONVOY_STOPPED");

        string custom = "";
        if (m_Settings.NotificationStopped != "")
        {

            string routeName = "";
            if (ev)
                routeName = ev.GetRouteName();
            custom = ApplyPlaceholders(m_Settings.NotificationStopped, loc, "", "", routeName, 0, 0);
        }

        SendNotification(custom, text);
    }
}
