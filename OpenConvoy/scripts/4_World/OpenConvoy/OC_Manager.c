class OC_Manager
{
    protected static ref OC_Manager s_Instance;

    protected ref OC_Settings m_Settings;
    protected ref array<OC_ConvoyCar> m_Vehicles;
    protected ref OC_ExpansionCrew m_Crew;
    protected bool m_Running;
    protected bool m_CleanupScheduled;
    protected bool m_Engaged;

    static OC_Manager GetInstance()
    {
        if (!s_Instance)
            s_Instance = new OC_Manager;
        return s_Instance;
    }

    void OC_Manager()
    {
        m_Vehicles = new array<OC_ConvoyCar>;
        m_Crew = new OC_ExpansionCrew;
    }

    void Start()
    {
        if (m_Running || !GetGame().IsServer())
            return;

        m_Settings = OC_SettingsRepository.Load();
        if (!m_Settings || !m_Settings.Enabled)
        {
            Print("[Wild-Fields] Event is disabled in settings.json.");
            return;
        }

        if (!OC_ValidateSettings())
            return;

        m_Running = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SpawnConvoy, m_Settings.StartupDelaySeconds * 1000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PollConvoy, 1000, true);
        Print("[Wild-Fields] Manager started.");
    }

    void Stop()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(SpawnConvoy);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(PollConvoy);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(CleanupConvoy);
        CleanupConvoy();
        m_Running = false;
    }

    void SpawnConvoy()
    {
        if (!m_Running || m_Vehicles.Count() > 0)
            return;

        OC_RouteConfig route = m_Settings.Routes.GetRandomElement();
        array<vector> points = OC_ParseRoute(route);
        if (!points || points.Count() < 2)
        {
            Error("[Wild-Fields] Route " + route.Name + " has fewer than two valid waypoints.");
            OC_ScheduleRespawn();
            return;
        }

        vector forward = points[1] - points[0];
        forward[1] = 0;
        forward.Normalize();
        vector right = Vector(forward[2], 0, -forward[0]);
        vector orientation = forward.VectorToAngles();

        if (m_Settings.SpawnAICrew && !m_Crew.Initialize())
            Error("[Wild-Fields] Convoy will continue without AI crew.");

        foreach (OC_VehicleConfig vehicleConfig : m_Settings.Vehicles)
        {
            vector spawnPosition = points[0] - forward * vehicleConfig.DistanceBehindLeader + right * vehicleConfig.SideOffset;
            spawnPosition[1] = GetGame().SurfaceRoadY(spawnPosition[0], spawnPosition[2]) + 0.5;

            Object spawned = GetGame().CreateObjectEx(vehicleConfig.ClassName, spawnPosition, ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS);
            OC_ConvoyCar vehicle = OC_ConvoyCar.Cast(spawned);
            if (!vehicle)
            {
                Error("[Wild-Fields] " + vehicleConfig.ClassName + " is not derived from OC_ConvoyCar.");
                if (spawned)
                    GetGame().ObjectDelete(spawned);
                continue;
            }

            vehicle.SetOrientation(orientation);
            OC_PrepareVehicle(vehicle, vehicleConfig);
            vehicle.OC_ConfigureRoute(points, m_Settings.TargetSpeedKmh, m_Settings.WaypointRadiusMeters);
            m_Vehicles.Insert(vehicle);

            if (m_Settings.SpawnAICrew)
                m_Crew.AddVehicleCrew(vehicle, m_Settings);
        }

        if (m_Vehicles.Count() == 0)
        {
            Error("[Wild-Fields] No convoy vehicles could be spawned.");
            m_Crew.Cleanup();
            OC_ScheduleRespawn();
            return;
        }

        Print("[Wild-Fields] Spawned route " + route.Name + " with " + m_Vehicles.Count() + " vehicles; waiting for AI crew to board.");

        if (m_Settings.SpawnAICrew)
        {
            foreach (OC_ConvoyCar crewVehicle : m_Vehicles)
            {
                if (crewVehicle)
                    crewVehicle.OC_PrimeEngine();
            }
            m_Crew.BeginSeating();
        }
        else
            OnCrewReady();
    }

    void OnCrewReady()
    {
        if (!m_Running || m_Vehicles.Count() == 0)
            return;

        foreach (OC_ConvoyCar vehicle : m_Vehicles)
        {
            if (vehicle && vehicle.IsAlive())
                vehicle.OC_StartRoute();
        }

        Print("[Wild-Fields] Crew seating complete; convoy started.");
    }

    void PollConvoy()
    {
        if (!m_Running || m_Vehicles.Count() == 0 || m_CleanupScheduled)
            return;

        bool allFinished = true;
        foreach (OC_ConvoyCar vehicle : m_Vehicles)
        {
            if (vehicle && vehicle.IsAlive() && !vehicle.OC_IsRouteComplete())
            {
                allFinished = false;
                break;
            }
        }

        if (allFinished)
        {
            m_CleanupScheduled = true;
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CleanupConvoy, m_Settings.CleanupDelaySeconds * 1000, false);
        }
    }

    void OnVehicleAttacked(OC_ConvoyCar attackedVehicle)
    {
        if (m_Engaged || !m_Settings || !m_Settings.DismountCrewWhenAttacked)
            return;

        m_Engaged = true;
        foreach (OC_ConvoyCar vehicle : m_Vehicles)
        {
            if (vehicle)
                vehicle.OC_HaltForCombat();
        }

        m_Crew.DismountAll();
        Print("[Wild-Fields] Convoy attacked; crew dismounting.");
    }

    void CleanupConvoy()
    {
        m_Crew.Cleanup();

        foreach (OC_ConvoyCar vehicle : m_Vehicles)
        {
            if (vehicle)
                GetGame().ObjectDelete(vehicle);
        }

        m_Vehicles.Clear();
        m_CleanupScheduled = false;
        m_Engaged = false;

        if (m_Running)
            OC_ScheduleRespawn();
    }

    protected bool OC_ValidateSettings()
    {
        if (m_Settings.Routes.Count() == 0)
        {
            Error("[Wild-Fields] No routes are configured.");
            return false;
        }

        if (m_Settings.Vehicles.Count() == 0)
        {
            Error("[Wild-Fields] No vehicles are configured.");
            return false;
        }

        return true;
    }

    protected array<vector> OC_ParseRoute(OC_RouteConfig route)
    {
        array<vector> points = new array<vector>;
        foreach (string waypointText : route.Waypoints)
        {
            vector waypoint = waypointText.ToVector();
            if (waypoint[0] == 0 && waypoint[2] == 0)
                continue;
            waypoint[1] = GetGame().SurfaceRoadY(waypoint[0], waypoint[2]);
            points.Insert(waypoint);
        }
        return points;
    }

    protected void OC_PrepareVehicle(OC_ConvoyCar vehicle, OC_VehicleConfig config)
    {
        vehicle.OC_Assemble();

        foreach (string attachmentClass : config.Attachments)
            vehicle.GetInventory().CreateAttachment(attachmentClass);
    }

    protected void OC_ScheduleRespawn()
    {
        if (m_Running)
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SpawnConvoy, m_Settings.RespawnDelaySeconds * 1000, false);
    }
}
