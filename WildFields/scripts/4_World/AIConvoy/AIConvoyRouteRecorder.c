class AIConvoyRouteRecorder
{
    protected AIConvoyGeneralSettings m_Settings;

    protected bool m_Active;
    protected Transport m_Vehicle;
    protected PlayerBase m_Player;
    protected ref array<vector> m_Points;

    protected ref AIConvoyGearCalibrator m_Gears;

    protected ref TStringArray m_MenuArmed;

    void AIConvoyRouteRecorder(AIConvoyGeneralSettings settings)
    {
        m_Settings = settings;
        m_Points = new array<vector>;
        m_Gears = new AIConvoyGearCalibrator();
        m_MenuArmed = new TStringArray;

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, 500, true);

        if (settings.RouteRecorderPlayers.Count() > 0)
            Print("[Wild-Fields] Route recorder armed for " + settings.RouteRecorderPlayers.Count() + " player(s) (sample every " + settings.RouteRecorderSampleDistance + " m)");
        else
            Print("[Wild-Fields] Route recorder started from the admin menu (sample every " + settings.RouteRecorderSampleDistance + " m)");
    }

    void Arm(string steamId)
    {
        if (m_MenuArmed.Find(steamId) == -1)
            m_MenuArmed.Insert(steamId);
    }

    bool Disarm(string steamId)
    {
        int idx = m_MenuArmed.Find(steamId);
        if (idx != -1)
            m_MenuArmed.Remove(idx);

        if (!IsRecording(steamId))
            return false;

        if (IsArmedBySettings(steamId, m_Player.GetIdentity().GetName()))
            return false;

        bool saved = Save();
        m_Active = false;
        m_Vehicle = null;

        if (saved && m_Player && m_Player.GetIdentity())
            Message(m_Player, "[Convoy Recorder] Recorder disarmed - route saved (disabled). Enable it in the admin menu.");

        m_Player = null;
        return saved;
    }

    bool IsArmedId(string steamId)
    {
        if (m_MenuArmed.Find(steamId) != -1)
            return true;
        return m_Settings.RouteRecorderPlayers.Find(steamId) != -1;
    }

    bool IsArmedBySettings(string steamId, string playerName)
    {
        if (m_Settings.RouteRecorderPlayers.Find(steamId) != -1)
            return true;
        return playerName != "" && m_Settings.RouteRecorderPlayers.Find(playerName) != -1;
    }

    bool IsRecording(string steamId)
    {
        if (!m_Active || !m_Player || !m_Player.GetIdentity())
            return false;
        return m_Player.GetIdentity().GetPlainId() == steamId;
    }

    void Stop()
    {
        if (GetGame() && GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM))
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Tick);
    }

    protected void Tick()
    {

        PlayerBase player = m_Player;
        if (!player || !player.GetIdentity())
            player = FindArmedPlayerInVehicle();

        Transport vehicle;
        if (player)
            vehicle = Transport.Cast(player.GetParent());

        if (vehicle)
        {
            if (!m_Active || vehicle != m_Vehicle)
            {

                if (m_Active)
                    Save();

                m_Active = true;
                m_Vehicle = vehicle;
                m_Player = player;
                m_Points.Clear();
                m_Points.Insert(vehicle.GetPosition());
                m_Gears.Begin(vehicle);

                Print("[Wild-Fields] Route recording started (" + vehicle.GetType() + ", " + player.GetIdentity().GetName() + ")");
                Message(player, "[Convoy Recorder] Recording route - drive from A to B, get out to save. Working out this vehicle's gears as you go.");
            }
            else
            {
                vector pos = vehicle.GetPosition();
                if (vector.Distance(pos, m_Points[m_Points.Count() - 1]) >= m_Settings.RouteRecorderSampleDistance)
                    m_Points.Insert(pos);

                m_Gears.Sample(vehicle);
            }
        }
        else if (m_Active)
        {

            Save();
            m_Active = false;
            m_Vehicle = null;

            if (m_Player && m_Player.GetIdentity())
                Message(m_Player, "[Convoy Recorder] Route saved to Routes/ (disabled - set Enabled 1 to activate).");

            m_Player = null;
        }
    }

    protected PlayerBase FindArmedPlayerInVehicle()
    {
        array<Man> players = new array<Man>;
        GetGame().GetWorld().GetPlayerList(players);

        foreach (Man man : players)
        {
            PlayerIdentity identity = man.GetIdentity();
            if (!identity)
                continue;

            if (m_Settings.RouteRecorderPlayers.Find(identity.GetName()) == -1 && !IsArmedId(identity.GetPlainId()))
                continue;

            if (!Transport.Cast(man.GetParent()))
                continue;

            return PlayerBase.Cast(man);
        }

        return null;
    }

    protected bool Save()
    {

        if (m_Vehicle)
        {
            vector endPos = m_Vehicle.GetPosition();
            if (vector.Distance(endPos, m_Points[m_Points.Count() - 1]) > 2.0)
                m_Points.Insert(endPos);
        }

        if (m_Points.Count() < 2)
        {
            Print("[Wild-Fields] Route recording discarded (fewer than 2 waypoints)");
            m_Points.Clear();
            return false;
        }

        int year, month, day, hour, minute, second;
        GetYearMonthDay(year, month, day);
        GetHourMinuteSecond(hour, minute, second);
        string stamp = string.Format("%1-%2-%3_%4-%5-%6", year, month.ToStringLen(2), day.ToStringLen(2), hour.ToStringLen(2), minute.ToStringLen(2), second.ToStringLen(2));

        AIConvoyRoute route = new AIConvoyRoute();
        route.Name = "Recorded " + stamp;
        route.Enabled = false;
        foreach (vector point : m_Points)
            route.Waypoints.Insert(point);

        string defaultVehicle = PickDefaultPreset(AIConvoyGeneralSettings.VEHICLES_DIR, "M1025");
        string defaultCrew = PickDefaultPreset(AIConvoyGeneralSettings.CREWS_DIR, "Mercenaries");

        string middleVehicle = PickDefaultPreset(AIConvoyGeneralSettings.VEHICLES_DIR, "Truck01");
        string middleCrew = PickDefaultPreset(AIConvoyGeneralSettings.CREWS_DIR, "Driver");

        if (defaultVehicle != "" && middleVehicle != "")
        {
            route.VehicleSlots.Insert(defaultVehicle);
            route.VehicleSlots.Insert(middleVehicle);
            route.VehicleSlots.Insert(defaultVehicle);
        }
        if (defaultCrew != "" && middleCrew != "")
        {
            route.CrewSlots.Insert(defaultCrew);
            route.CrewSlots.Insert(middleCrew);
            route.CrewSlots.Insert(defaultCrew);
        }

        string fileBase = "Recorded_" + stamp;
        string path = AIConvoyGeneralSettings.ROUTES_DIR + "/" + fileBase + ".json";
        string errorMsg;
        bool saved = JsonFileLoader<AIConvoyRoute>.SaveFile(path, route, errorMsg);
        if (saved)
        {
            Print("[Wild-Fields] Recorded route saved: " + m_Points.Count() + " waypoints -> " + path + " (Enabled 0 - review, then enable). Column: " + defaultVehicle + "/" + defaultCrew + ", " + middleVehicle + "/" + middleCrew + ", " + defaultVehicle + "/" + defaultCrew + " - edit VehicleSlots/CrewSlots to change it.");

            AIConvoyConfigDir.AddKnownName(AIConvoyGeneralSettings.ROUTES_DIR, fileBase);

            if (g_AIConvoyManager)
                g_AIConvoyManager.RegisterRecordedRoute(route, fileBase);
        }
        else
        {
            Print("[Wild-Fields] ERROR saving recorded route: " + errorMsg);
        }

        string suggestion;
        string gearReport = m_Gears.Report(suggestion);
        if (gearReport != "")
        {
            Print("[Wild-Fields] " + gearReport);
            Print("[Wild-Fields] gear detail:" + m_Gears.Detail());

            if (m_Player && m_Player.GetIdentity())
                Message(m_Player, gearReport);
        }

        m_Points.Clear();
        return saved;
    }

    protected string PickDefaultPreset(string dir, string preferred)
    {
        TStringArray names;
        AIConvoyConfigDir.ListJsonFiles(dir, names);

        if (!names || names.Count() == 0)
            return "";

        if (names.Find(preferred) != -1)
            return preferred;

        return names[0];
    }

    protected void Message(Man man, string text)
    {
        Param1<string> msg = new Param1<string>(text);
        GetGame().RPCSingleParam(man, ERPCs.RPC_USER_ACTION_MESSAGE, msg, true, man.GetIdentity());
    }
}
