class AIConvoyVehicleUnit
{
    Transport m_Vehicle;
    eAIGroup m_Group;
    ref array<eAIBase> m_AI;
    ref array<int> m_SeatAssignments;
    AIConvoyVehicleSetup m_Setup;
    int m_WaypointIdx;
    float m_StuckTime;
    int m_StuckStrikes;
    vector m_StopPosition;
    bool m_VehicleVanishLogged;

    void AIConvoyVehicleUnit(AIConvoyVehicleSetup setup)
    {
        m_Setup = setup;
        m_AI = new array<eAIBase>;
        m_SeatAssignments = new array<int>;
        m_WaypointIdx = 1;
    }

    CarScript GetCar()
    {
        return CarScript.Cast(m_Vehicle);
    }
}

class AIConvoyEvent
{
    static const int STATE_SPAWNING = 0;
    static const int STATE_DRIVING  = 1;
    static const int STATE_STOPPED  = 2;
    static const int STATE_DONE     = 3;

    protected static const float WAYPOINT_REACHED_DIST = 6.0;
    protected static const float WATCHDOG_INTERVAL = 5.0;
    protected static const float SEAT_TIMEOUT = 45.0;
    protected static const float CE_LIFETIME_SECONDS = 7200.0;

    protected AIConvoyManager m_Manager;
    protected ref AIConvoyEventParams m_Config;

    protected ref array<ref AIConvoyVehicleUnit> m_Units;

    protected int m_State;
    protected float m_CheckAccu;
    protected float m_SeatWaitTime;
    protected float m_NoPlayerTime;
    protected bool m_MoveLoopActive;
    protected bool m_WatchdogActive;
    protected float m_LeadFollowGap;
    protected int m_LastCrewSeen;
    protected bool m_CrewCountInitialised;
    protected int m_SpawnUnitIdx;
    protected int m_SpawnAiIdx;

    void AIConvoyEvent(AIConvoyManager manager, AIConvoyEventParams config)
    {
        m_Manager = manager;
        m_Config = config;
        m_Units = new array<ref AIConvoyVehicleUnit>;
        m_State = STATE_SPAWNING;
    }

    void ~AIConvoyEvent()
    {
        if (!GetGame())
            return;

        StopLoops();
    }

    protected void StopLoops()
    {
        ScriptCallQueue queue = GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM);
        if (!queue)
            return;

        queue.Remove(MoveTick);
        queue.Remove(CheckSeated);
        queue.Remove(WatchdogTick);
        queue.Remove(SpawnNextAI);
        queue.Remove(SeatCrews);
        queue.Remove(DismountCrews);
        queue.Remove(FinalizeDismount);
        queue.Remove(ForceLifetimeCleanup);
        m_MoveLoopActive = false;
        m_WatchdogActive = false;
    }

    string GetRouteName()
    {
        if (!m_Config)
            return "";
        return m_Config.Name;
    }

    vector GetConvoyCenter()
    {
        if (!m_Units)
            return vector.Zero;

        float sumX = 0;
        float sumY = 0;
        float sumZ = 0;
        int counted = 0;

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (!unit)
                continue;

            CarScript car = unit.GetCar();
            if (!car || !car.IsAlive())
                continue;

            vector pos = car.GetPosition();
            sumX += pos[0];
            sumY += pos[1];
            sumZ += pos[2];
            counted++;
        }

        if (counted == 0)
            return vector.Zero;

        return Vector(sumX / counted, sumY / counted, sumZ / counted);
    }

    bool GetLeadPose(out vector pos, out vector dir)
    {
        if (!m_Units)
            return false;

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (!unit)
                continue;

            CarScript car = unit.GetCar();
            if (!car || !car.IsAlive())
                continue;

            pos = car.GetPosition();
            dir = car.GetDirection();
            dir[1] = 0;
            if (dir.Length() < 0.01)
                dir = "0 0 1";
            dir.Normalize();
            return true;
        }

        return false;
    }

    bool Start()
    {
        if (m_Config.Waypoints.Count() < 2)
        {
            Log("Route '" + m_Config.Name + "' has fewer than 2 waypoints, aborting");
            return false;
        }

        vector routeDir = m_Config.Waypoints[1] - m_Config.Waypoints[0];
        routeDir[1] = 0;
        routeDir.Normalize();
        float yaw = routeDir.VectorToAngles()[0];

        for (int i = 0; i < m_Config.Setups.Count(); i++)
        {
            AIConvoyVehicleUnit unit = new AIConvoyVehicleUnit(m_Config.Setups[i]);

            vector spawnPos = m_Config.Waypoints[0] - routeDir * (m_Config.FollowGapMeters * i);
            spawnPos[1] = GetGame().SurfaceRoadY(spawnPos[0], spawnPos[2]);

            Object obj = GetGame().CreateObjectEx(unit.m_Setup.Vehicle.VehicleClassName, spawnPos, ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_NOPERSISTENCY_WORLD);
            unit.m_Vehicle = Transport.Cast(obj);
            if (!unit.m_Vehicle)
            {
                Log("Could not spawn vehicle '" + unit.m_Setup.Vehicle.VehicleClassName + "' (not a Transport?)");
                if (obj)
                    GetGame().ObjectDelete(obj);
                continue;
            }

            unit.m_Vehicle.SetOrientation(Vector(yaw, 0, 0));

            if (m_Config.TrackSpawnedVehicles)
                AIConvoySpawnLedger.Add(unit.m_Vehicle);

            CarScript trackedCar = unit.GetCar();
            if (trackedCar)
            {
                trackedCar.m_AIConvoy_TrackPlayerDamage = true;

                trackedCar.m_AIConvoy_IsConvoyVehicle = true;
            }

            foreach (string attachment : unit.m_Setup.Vehicle.VehicleAttachments)
            {
                EntityAI att = unit.m_Vehicle.GetInventory().CreateAttachment(attachment);
                if (att)
                {

                    ItemBase attItem = ItemBase.Cast(att);
                    if (attItem)
                    {
                        attItem.m_AIConvoy_ForwardPlayerDamage = true;

                        attItem.LockToParent();
                    }
                }
                else
                {
                    Log("Could not attach '" + attachment + "' to " + unit.m_Setup.Vehicle.VehicleClassName);
                }
            }

            foreach (AIConvoyCargoItem cargoItem : unit.m_Setup.Vehicle.CargoItems)
            {
                if (!cargoItem || cargoItem.ClassName == "")
                    continue;

                if (cargoItem.Chance < 1.0 && Math.RandomFloat01() > cargoItem.Chance)
                    continue;
                EntityAI cargoEnt = unit.m_Vehicle.GetInventory().CreateEntityInCargo(cargoItem.ClassName);
                if (!cargoEnt)
                {
                    Log("Could not create cargo item '" + cargoItem.ClassName + "'");
                    continue;
                }

                if (cargoItem.Attachments)
                {
                    foreach (string cargoAtt : cargoItem.Attachments)
                    {
                        if (cargoAtt == "")
                            continue;
                        if (!cargoEnt.GetInventory().CreateAttachment(cargoAtt))
                            Log("Could not attach '" + cargoAtt + "' to cargo item '" + cargoItem.ClassName + "' (wrong classname, or that item has no matching slot)");
                    }

                    AIConvoyLootPresets.SyncWeaponState(cargoEnt);
                }
            }

            if (unit.m_Setup.Vehicle.CargoPresets && unit.m_Setup.Vehicle.CargoPresets.Count() > 0)
            {
                AIConvoyLootPresets loot = m_Manager.GetLootPresets();
                if (loot)
                {
                    int lootMade = 0;
                    foreach (string presetName : unit.m_Setup.Vehicle.CargoPresets)
                    {
                        if (presetName != "")
                            lootMade += loot.SpawnInto(unit.m_Vehicle, presetName);
                    }

                    if (lootMade > 0)
                        Log("Loot presets placed " + lootMade + " item(s) in " + unit.m_Setup.Vehicle.VehicleClassName);
                }
            }

            if (trackedCar)
                trackedCar.AIConvoy_SetLocked(true);

            unit.m_Vehicle.SetLifetimeMax(CE_LIFETIME_SECONDS);
            unit.m_Vehicle.SetLifetime(CE_LIFETIME_SECONDS);

            Car car = Car.Cast(unit.m_Vehicle);
            if (car)
            {
                car.Fill(CarFluid.FUEL, car.GetFluidCapacity(CarFluid.FUEL));
                car.Fill(CarFluid.COOLANT, car.GetFluidCapacity(CarFluid.COOLANT));
                car.Fill(CarFluid.OIL, car.GetFluidCapacity(CarFluid.OIL));
                car.Fill(CarFluid.BRAKE, car.GetFluidCapacity(CarFluid.BRAKE));
            }

            m_Units.Insert(unit);
        }

        if (m_Units.Count() == 0)
        {
            Log("No convoy vehicles could be spawned, aborting");
            return false;
        }

        Log("Spawned " + m_Units.Count() + " convoy vehicle(s) at " + m_Config.Waypoints[0] + " for route '" + m_Config.Name + "'");

        m_SpawnUnitIdx = 0;
        m_SpawnAiIdx = 0;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SpawnNextAI, 500, false);

        return true;
    }

    protected void SpawnNextAI()
    {

        while (m_SpawnUnitIdx < m_Units.Count() && m_SpawnAiIdx >= m_Units[m_SpawnUnitIdx].m_Setup.Crew.NumberOfAI)
        {
            m_SpawnUnitIdx++;
            m_SpawnAiIdx = 0;
        }

        if (m_SpawnUnitIdx >= m_Units.Count())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SeatCrews, 1500, false);
            return;
        }

        AIConvoyVehicleUnit unit = m_Units[m_SpawnUnitIdx];
        if (!unit.m_Vehicle)
        {
            Abort("vehicle disappeared during AI spawn");
            return;
        }

        AIConvoyCrewPreset crewCfg = unit.m_Setup.Crew;

        string unitClass;
        if (crewCfg.Units.Count() > 0)
            unitClass = crewCfg.Units.GetRandomElement();
        else
            unitClass = eAISurvivor.GetQuasiRandom();

        vector spawnPos = unit.m_Vehicle.GetPosition() + unit.m_Vehicle.GetDirection().Perpend() * 4.0 + unit.m_Vehicle.GetDirection() * (m_SpawnAiIdx * 1.5);
        spawnPos[1] = GetGame().SurfaceY(spawnPos[0], spawnPos[2]);

        eAIBase ai = eAIBase.Cast(GetGame().CreateObject(unitClass, spawnPos));
        if (ai)
        {
            ai.SetPosition(spawnPos);
            ExpansionHumanLoadout.Apply(ai, crewCfg.Loadout, false);

            if (!unit.m_Group)
            {
                unit.m_Group = ai.GetGroup();
                unit.m_Group.SetName("AIConvoy " + m_Config.Name + " #" + (m_SpawnUnitIdx + 1));

                eAIFaction faction = eAIFaction.Create(crewCfg.Faction);
                if (!faction)
                    faction = new eAIFactionMercenaries();
                unit.m_Group.SetFaction(faction);
                unit.m_Group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);

                ai.m_eAI_GroupMemberID = unit.m_Group.m_NextGroupMemberID++;
            }
            else
            {
                ai.SetGroup(unit.m_Group);
            }

            ai.SetMovementSpeedLimits(2.0, 3.0);
            ai.eAI_SetUnlimitedReload(true);

            ai.eAI_SetPassive(true);

            ai.SetAllowDamage(false);

            ai.m_AIConvoy_NoKillFeed = !m_Config.ShowCrewKillFeed;

            ai.AIConvoy_SetLootable(unit.m_Setup.Crew.LootableOnDeath);

            ai.m_AIConvoy_BodyDespawnSeconds = unit.m_Setup.Crew.GetBodyDespawnSeconds();

            if (!ai.m_AIConvoy_Lootable)
                ai.AIConvoy_RecordGear();

            unit.m_AI.Insert(ai);
        }
        else
        {
            Log("Could not spawn AI unit '" + unitClass + "'");
        }

        m_SpawnAiIdx++;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SpawnNextAI, 500, false);
    }

    protected void SeatCrews()
    {
        int totalAI = 0;

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (!unit.m_Vehicle)
                continue;

            for (int i = unit.m_AI.Count() - 1; i >= 0; i--)
            {
                if (!unit.m_AI[i])
                    unit.m_AI.Remove(i);
            }

            int crewSize = unit.m_Vehicle.CrewSize();
            for (int j = unit.m_AI.Count() - 1; j >= 0; j--)
            {
                if (j >= crewSize)
                {
                    Log("No free seat for AI (vehicle has " + crewSize + " seats) - removing extra unit");
                    GetGame().ObjectDelete(unit.m_AI[j]);
                    unit.m_AI.Remove(j);
                }
            }

            unit.m_SeatAssignments.Clear();
            for (int s = 0; s < unit.m_AI.Count(); s++)
                unit.m_SeatAssignments.Insert(s);

            totalAI += unit.m_AI.Count();
        }

        if (totalAI == 0)
        {
            Abort("nothing to seat");
            return;
        }

        foreach (AIConvoyVehicleUnit engineUnit : m_Units)
        {
            CarScript engineCar = engineUnit.GetCar();
            if (engineCar)
                engineCar.AIConvoy_PrimeEngine();
        }

        m_SeatWaitTime = 0;
        TrySeatAll();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckSeated, 500, true);
    }

    protected void TrySeatAll()
    {
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (!unit.m_Vehicle)
                continue;

            for (int i = 0; i < unit.m_AI.Count(); i++)
            {
                eAIBase ai = unit.m_AI[i];
                if (!ai || !ai.IsAlive() || ai.IsInTransport())
                    continue;

                HumanCommandVehicle vehCmd = ai.GetCommand_Vehicle();
                if (vehCmd)
                    continue;

                ai.DisableSimulation(false);

                if (ai.Expansion_IsAnimationIdle() && !ai.eAI_GetTransport())
                    ai.Notify_Transport(unit.m_Vehicle, unit.m_SeatAssignments[i]);
            }
        }
    }

    protected void CheckSeated()
    {
        m_SeatWaitTime += 0.5;

        for (int v = 0; v < m_Units.Count(); v++)
        {
            if (!m_Units[v].m_Vehicle && !m_Units[v].m_VehicleVanishLogged)
            {
                m_Units[v].m_VehicleVanishLogged = true;
                Log("WARNING: a convoy vehicle disappeared during spawn/seating — the convoy will continue with the rest.");
            }
        }

        bool allSeated = true;
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            foreach (eAIBase ai : unit.m_AI)
            {
                if (ai && ai.IsAlive() && !ai.IsInTransport())
                {
                    allSeated = false;
                    break;
                }
            }
        }

        if (!allSeated && m_SeatWaitTime < SEAT_TIMEOUT)
        {
            TrySeatAll();
            return;
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(CheckSeated);

        if (!allSeated)
        {

            foreach (AIConvoyVehicleUnit lateUnit : m_Units)
            {
                for (int i = lateUnit.m_AI.Count() - 1; i >= 0; i--)
                {
                    eAIBase straggler = lateUnit.m_AI[i];
                    if (straggler && straggler.IsAlive() && !straggler.IsInTransport())
                    {
                        Log("AI did not enter the vehicle in time, removing it");
                        lateUnit.m_AI.Remove(i);
                        GetGame().ObjectDelete(straggler);
                    }
                }
            }
        }

        BeginDriving();
    }

    protected void BeginDriving()
    {
        CarScript previousCar;
        int totalAI = 0;

        for (int i = m_Units.Count() - 1; i >= 0; i--)
        {
            if (!m_Units[i].m_Vehicle || !m_Units[i].GetCar())
            {
                Log("WARNING: convoy vehicle '" + m_Units[i].m_Setup.Vehicle.VehicleClassName + "' was not available at drive start — continuing without it.");
                m_Units.Remove(i);
            }
        }

        if (m_Units.Count() == 0)
        {
            Abort("no drivable convoy vehicles left");
            return;
        }

        m_State = STATE_DRIVING;
        m_CheckAccu = 0;

        m_LeadFollowGap = Math.Max(m_Config.FollowGapMeters - 2.0, 8.0);

        int slotIdx = 0;
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            CarScript car = unit.GetCar();

            if (unit.m_AI.Count() == 0)
                Log("Convoy vehicle '" + unit.m_Setup.Vehicle.VehicleClassName + "' is driving with no crew (a rider didn't board in time) — it still drives and stays lootable.");

            unit.m_WaypointIdx = 1;
            unit.m_StuckTime = 0;
            car.m_AIConvoy_TargetPos = m_Config.Waypoints[1];

            car.m_AIConvoy_FollowVehicle = previousCar;
            if (slotIdx == 1)
                car.m_AIConvoy_FollowGap = m_LeadFollowGap;
            else
                car.m_AIConvoy_FollowGap = m_Config.FollowGapMeters;

            car.m_AIConvoy_ShiftUpSpeeds = unit.m_Setup.Vehicle.ManualShiftUpSpeeds;
            car.AIConvoy_StartAutopilot(m_Config.SpeedKmh);
            previousCar = car;
            slotIdx++;

            totalAI += unit.m_AI.Count();
        }

        m_MoveLoopActive = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MoveTick, 250, true);

        if (m_Config.MaxConvoyLifetimeSeconds > 0)
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ForceLifetimeCleanup, m_Config.MaxConvoyLifetimeSeconds * 1000, false);

        Log("Convoy '" + m_Config.Name + "' is rolling: " + m_Units.Count() + " vehicle(s), " + totalAI + " AI");
        m_Manager.NotifyConvoyStarted(m_Config);

        m_Manager.MarkerBegin(m_Config.Waypoints[0]);

        vector debugRouteDir = m_Config.Waypoints[1] - m_Config.Waypoints[0];
        debugRouteDir[1] = 0;
        debugRouteDir.Normalize();
        m_Manager.TeleportDebugPlayers(m_Config.Waypoints[0], debugRouteDir);
    }

    protected void MoveTick()
    {

        bool unitsChanged = false;
        for (int u = m_Units.Count() - 1; u >= 0; u--)
        {
            if (!m_Units[u].m_Vehicle)
            {
                Log("Convoy vehicle disappeared while driving, continuing with the rest");
                m_Units.Remove(u);
                unitsChanged = true;
            }
        }

        if (m_Units.Count() == 0)
        {
            Abort("all convoy vehicles disappeared while driving");
            return;
        }

        if (unitsChanged)
            RelinkFollowChain();

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            CarScript car = unit.GetCar();
            vector pos = unit.m_Vehicle.GetPosition();
            vector target = m_Config.Waypoints[unit.m_WaypointIdx];

            vector dir = target - pos;
            dir[1] = 0;

            if (dir.Length() <= WAYPOINT_REACHED_DIST)
            {

                unit.m_StuckStrikes = 0;

                if (unit == m_Units[0] && unit.m_WaypointIdx == m_Config.AmbushWaypointIndex)
                {
                    SpringAmbush("reached ambush waypoint " + unit.m_WaypointIdx);
                    return;
                }

                unit.m_WaypointIdx++;

                if (unit.m_WaypointIdx >= m_Config.Waypoints.Count())
                {
                    if (unit == m_Units[0])
                    {

                        FinishQuiet("reached end of route without being engaged");
                        return;
                    }

                    unit.m_WaypointIdx = m_Config.Waypoints.Count() - 1;
                }
            }

            car.m_AIConvoy_TargetPos = m_Config.Waypoints[unit.m_WaypointIdx];

            if (car.m_AIConvoy_Overtaking && unit.m_WaypointIdx > 0)
            {
                vector otDir = m_Config.Waypoints[unit.m_WaypointIdx] - m_Config.Waypoints[unit.m_WaypointIdx - 1];
                otDir[1] = 0;
                if (otDir.Length() > 0.1)
                {
                    otDir.Normalize();
                    car.m_AIConvoy_TargetPos = car.m_AIConvoy_TargetPos + otDir.Perpend() * 3.5;
                }
            }

            bool holdingFormation = false;
            if (car.m_AIConvoy_FollowVehicle)
            {
                float gap = vector.Distance(pos, car.m_AIConvoy_FollowVehicle.GetPosition());
                holdingFormation = gap < car.m_AIConvoy_FollowGap * 1.5;
            }

            if (car.EngineIsOn() && Math.AbsFloat(car.GetSpeedometer()) < 2.0 && !holdingFormation)
            {
                unit.m_StuckTime += 0.25;
                if (unit.m_StuckTime >= 10.0)
                {
                    unit.m_StuckTime = 0;
                    unit.m_StuckStrikes++;

                    vector transform[4];
                    unit.m_Vehicle.GetTransform(transform);
                    bool flipped = transform[1][1] < 0.5;

                    string who = "vehicle " + (m_Units.Find(unit) + 1) + "/" + m_Units.Count() + " (" + unit.m_Setup.Vehicle.VehicleClassName + ")";

                    vector recoveryPos;
                    if (unit.m_StuckStrikes >= 2)
                    {

                        recoveryPos = m_Config.Waypoints[unit.m_WaypointIdx];
                        unit.m_StuckStrikes = 0;
                        Log("Convoy " + who + " still stuck, skipped ahead to waypoint " + unit.m_WaypointIdx);
                    }
                    else
                    {
                        recoveryPos = pos + dir.Normalized() * 3.0;
                        if (flipped)
                            Log("Convoy " + who + " was flipped, righted it and nudged toward waypoint " + unit.m_WaypointIdx);
                        else
                            Log("Convoy " + who + " was stuck, nudged 3 m toward waypoint " + unit.m_WaypointIdx);
                    }

                    recoveryPos[1] = GetGame().SurfaceRoadY(recoveryPos[0], recoveryPos[2]) + 0.3;
                    unit.m_Vehicle.SetPosition(recoveryPos);

                    vector recoveryDir = m_Config.Waypoints[unit.m_WaypointIdx] - recoveryPos;
                    recoveryDir[1] = 0;
                    if (recoveryDir.Length() > 0.1)
                        unit.m_Vehicle.SetOrientation(Vector(recoveryDir.VectorToAngles()[0], 0, 0));
                    SetVelocity(unit.m_Vehicle, vector.Zero);
                    dBodySetAngularVelocity(unit.m_Vehicle, vector.Zero);
                }
            }
            else
            {
                unit.m_StuckTime = 0;
            }
        }

        UpdateColumnCohesion();

        m_CheckAccu += 0.25;
        if (m_CheckAccu >= 1.0)
        {
            m_CheckAccu = 0;

            RunTriggerChecks();
            RefreshVehicleLifetimes();
            CheckCrewIntact();
            UpdateSpawnLedgerPositions();
        }
    }

    protected void UpdateSpawnLedgerPositions()
    {
        if (!m_Config.TrackSpawnedVehicles)
            return;

        foreach (AIConvoyVehicleUnit u : m_Units)
        {
            if (u && u.m_Vehicle)
                AIConvoySpawnLedger.UpdatePosition(u.m_Vehicle);
        }
        AIConvoySpawnLedger.Flush();
    }

    protected void RefreshVehicleLifetimes()
    {
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (unit.m_Vehicle)
                unit.m_Vehicle.SetLifetime(CE_LIFETIME_SECONDS);

            foreach (eAIBase crew : unit.m_AI)
            {
                if (crew)
                    crew.SetLifetime(CE_LIFETIME_SECONDS);
            }
        }
    }

    protected void CheckCrewIntact()
    {
        int alive = 0;
        int dead = 0;
        int gone = 0;

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            foreach (eAIBase crew : unit.m_AI)
            {
                if (!crew)
                    gone++;
                else if (!crew.IsAlive())
                    dead++;
                else
                    alive++;
            }
        }

        int accounted = alive + dead + gone;
        if (!m_CrewCountInitialised)
        {
            m_CrewCountInitialised = true;
            m_LastCrewSeen = alive;
            return;
        }

        if (alive < m_LastCrewSeen)
            Log("Crew check: " + alive + " riding (was " + m_LastCrewSeen + ") - " + dead + " dead, " + gone + " entity missing, " + accounted + " tracked");

        m_LastCrewSeen = alive;
    }

    protected void UpdateColumnCohesion()
    {
        if (m_Units.Count() < 2)
            return;

        CarScript leadCar = m_Units[0].GetCar();
        CarScript secondCar = m_Units[1].GetCar();
        if (!leadCar || !secondCar)
            return;

        int strayIdx = -1;
        for (int si = 0; si < m_Units.Count() - 1; si++)
        {
            if (m_Units[si].m_WaypointIdx < m_Units[si + 1].m_WaypointIdx)
            {
                strayIdx = si;
                break;
            }
        }

        if (strayIdx >= 0)
        {

            CarScript strayCar = m_Units[strayIdx].GetCar();
            if (strayCar)
            {
                strayCar.m_AIConvoy_SpeedCap = 0;
                strayCar.m_AIConvoy_Overtaking = true;
            }

            float waitCap = Math.Max(m_Config.SpeedKmh * 0.35, 8.0);
            for (int wi = 0; wi < m_Units.Count(); wi++)
            {
                if (wi == strayIdx)
                    continue;

                CarScript waitCar = m_Units[wi].GetCar();
                if (!waitCar)
                    continue;

                waitCar.m_AIConvoy_Overtaking = false;

                if (m_Units[wi].m_WaypointIdx > m_Units[strayIdx].m_WaypointIdx)
                    waitCar.m_AIConvoy_SpeedCap = waitCap;
                else
                    waitCar.m_AIConvoy_SpeedCap = 0;
            }
            return;
        }

        for (int ci = 0; ci < m_Units.Count(); ci++)
        {
            CarScript clearCar = m_Units[ci].GetCar();
            if (!clearCar)
                continue;

            clearCar.m_AIConvoy_Overtaking = false;
            if (ci > 0)
                clearCar.m_AIConvoy_SpeedCap = 0;
        }

        float holdGap = m_Config.FollowGapMeters;
        float slack = holdGap * 0.5;
        float reactAt = holdGap + slack;

        float gap01 = vector.Distance(leadCar.GetPosition(), secondCar.GetPosition());
        float cap = m_Config.SpeedKmh;
        if (gap01 > reactAt)
            cap = m_Config.SpeedKmh - (gap01 - reactAt) * 1.0;

        float splitLimit = m_Config.FollowGapMeters * 2.0;
        for (int i = 2; i < m_Units.Count(); i++)
        {
            CarScript car = m_Units[i].GetCar();
            if (!car || !car.m_AIConvoy_FollowVehicle)
                continue;

            float gap = vector.Distance(car.GetPosition(), car.m_AIConvoy_FollowVehicle.GetPosition());
            if (gap > splitLimit)
            {
                float splitCap = m_Config.SpeedKmh - (gap - splitLimit) * 2.5;
                if (splitCap < cap)
                    cap = splitCap;
            }
        }

        if (cap >= m_Config.SpeedKmh)
            leadCar.m_AIConvoy_SpeedCap = 0;
        else
            leadCar.m_AIConvoy_SpeedCap = Math.Clamp(cap, 8.0, m_Config.SpeedKmh);
    }

    protected void RelinkFollowChain()
    {
        CarScript previousCar;
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            CarScript car = unit.GetCar();
            if (!car)
                continue;

            car.m_AIConvoy_FollowVehicle = previousCar;
            previousCar = car;
        }
    }

    protected void RunTriggerChecks()
    {

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            CarScript car = unit.GetCar();
            if (!car)
                continue;

            float maxHealth = car.GetMaxHealth("", "");
            float damageToStop;
            if (unit.m_Setup.Vehicle.StopDamageTotal > 0)
                damageToStop = unit.m_Setup.Vehicle.StopDamageTotal;
            else
                damageToStop = maxHealth * (1.0 - unit.m_Setup.Vehicle.VehicleDamageStopThreshold);

            float visualHealth = maxHealth * (1.0 - car.m_AIConvoy_PlayerDamage / damageToStop);
            car.SetHealth("", "", Math.Max(visualHealth, maxHealth * 0.1));

            for (int a = 0; a < car.GetInventory().AttachmentCount(); a++)
            {
                EntityAI att = car.GetInventory().GetAttachmentFromIndex(a);
                if (att)
                    att.SetHealth("", "", att.GetMaxHealth("", ""));
            }

            if (car.m_AIConvoy_PlayerDamage >= damageToStop)
            {
                SpringAmbush("players damaged a convoy vehicle below the stop threshold (" + car.m_AIConvoy_PlayerDamage + " of " + damageToStop + " damage)");
                return;
            }
        }
    }

    protected PlayerBase FindPlayerWithin(vector pos, float radius)
    {
        array<Man> players = new array<Man>;
        GetGame().GetWorld().GetPlayerList(players);

        foreach (Man man : players)
        {

            if (!man || !man.GetIdentity() || !man.IsAlive())
                continue;

            if (vector.Distance(man.GetPosition(), pos) <= radius)
                return PlayerBase.Cast(man);
        }

        return null;
    }

    protected void SpringAmbush(string reason)
    {
        if (m_State != STATE_DRIVING)
            return;

        m_State = STATE_STOPPED;
        m_MoveLoopActive = false;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(MoveTick);

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ForceLifetimeCleanup);

        Log("Convoy '" + m_Config.Name + "' stopping: " + reason);

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (!unit.m_Vehicle)
                continue;

            unit.m_StopPosition = unit.m_Vehicle.GetPosition();

            CarScript car = unit.GetCar();
            if (car)
            {
                car.AIConvoy_StopAutopilot();
                car.m_AIConvoy_TrackPlayerDamage = false;
                car.AIConvoy_SetLocked(false);
                car.SetHealth("", "", car.GetMaxHealth("", "") * unit.m_Setup.Vehicle.VehicleDamageStopThreshold);

                if (unit.m_Setup.Vehicle.RuinEngineOnStop)
                    car.SetHealth("Engine", "Health", 0);
            }
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DismountCrews, 400, false);
    }

    protected void DismountCrews()
    {
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            foreach (eAIBase ai : unit.m_AI)
            {
                if (!ai || !ai.IsAlive())
                    continue;

                ai.eAI_SetPassive(false);
                ai.SetAllowDamage(true);

                HumanCommandVehicle vehCmd = ai.GetCommand_Vehicle();
                if (vehCmd)
                    vehCmd.GetOutVehicle();
            }
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinalizeDismount, 4000, false);
    }

    protected void FinalizeDismount()
    {

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            foreach (eAIBase ai : unit.m_AI)
            {
                if (!ai || !ai.IsAlive())
                    continue;

                if (ai.IsInTransport())
                {
                    HumanCommandVehicle vehCmd = ai.GetCommand_Vehicle();
                    if (vehCmd)
                        vehCmd.GetOutVehicle();
                }
            }

            if (unit.m_Group)
            {
                unit.m_Group.ClearWaypoints();
                unit.m_Group.AddWaypoint(unit.m_StopPosition);
                unit.m_Group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);
            }
        }

        vector stopPos = vector.Zero;
        foreach (AIConvoyVehicleUnit stopUnit : m_Units)
        {
            if (stopUnit.m_StopPosition != vector.Zero)
            {
                stopPos = stopUnit.m_StopPosition;
                break;
            }
        }
        m_Manager.NotifyConvoyStopped(this, stopPos);
        m_Manager.MarkerStopped(stopPos);

        m_NoPlayerTime = 0;
        m_WatchdogActive = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(WatchdogTick, 5000, true);
    }

    protected void ForceLifetimeCleanup()
    {
        if (m_State == STATE_DONE)
            return;
        FinishQuiet("exceeded max lifetime (" + m_Config.MaxConvoyLifetimeSeconds + " s) - force cleanup");
    }

    protected void FinishQuiet(string reason)
    {
        if (m_State == STATE_DONE)
            return;

        StopLoops();

        Log("Convoy '" + m_Config.Name + "' despawning: " + reason);

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            CarScript quietCar = unit.GetCar();
            if (quietCar)
                quietCar.AIConvoy_StopAutopilot();

            if (unit.m_Group)
            {
                unit.m_Group.ClearAI(true);
                unit.m_Group = null;
            }
            unit.m_AI.Clear();

            if (unit.m_Vehicle)
            {
                AIConvoySpawnLedger.Remove(unit.m_Vehicle);
                GetGame().ObjectDelete(unit.m_Vehicle);
                unit.m_Vehicle = null;
            }
        }

        m_State = STATE_DONE;
        m_Manager.OnEventEnded(this);
    }

    protected void WatchdogTick()
    {

        RefreshVehicleLifetimes();

        vector watchPos = vector.Zero;
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (unit.m_Vehicle)
            {
                watchPos = unit.m_Vehicle.GetPosition();
                break;
            }
            if (unit.m_StopPosition != vector.Zero)
                watchPos = unit.m_StopPosition;
        }

        if (FindPlayerWithin(watchPos, m_Config.DespawnRadius))
            m_NoPlayerTime = 0;
        else
            m_NoPlayerTime += WATCHDOG_INTERVAL;

        if (m_NoPlayerTime >= m_Config.DespawnTimeoutSeconds)
            Finish();
    }

    protected void Finish()
    {
        StopLoops();

        Log("Convoy '" + m_Config.Name + "' cleaning up (no players nearby for " + m_Config.DespawnTimeoutSeconds + " s)");

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (unit.m_Group)
            {
                unit.m_Group.ClearAI(true);
                unit.m_Group = null;
            }

            if (unit.m_Vehicle)
            {
                bool moved = vector.Distance(unit.m_Vehicle.GetPosition(), unit.m_StopPosition) > 100.0;
                if (moved && m_Config.KeepVehicleIfMoved)
                {

                    unit.m_Vehicle.SetLifetimeMax(0);
                    unit.m_Vehicle.SetLifetime(unit.m_Vehicle.GetLifetimeMax());
                    Log("Convoy vehicle was moved by players, leaving it in the world (lifetime handed back to the economy)");
                }
                else
                {
                    AIConvoySpawnLedger.Remove(unit.m_Vehicle);
                    GetGame().ObjectDelete(unit.m_Vehicle);
                }
                unit.m_Vehicle = null;
            }
        }

        m_State = STATE_DONE;
        m_Manager.OnEventEnded(this);
    }

    protected void Abort(string reason)
    {
        StopLoops();

        Log("Convoy '" + m_Config.Name + "' aborted: " + reason);

        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            if (unit.m_Group)
            {
                unit.m_Group.ClearAI(true);
                unit.m_Group = null;
            }
            else
            {
                foreach (eAIBase ai : unit.m_AI)
                {
                    if (ai)
                        GetGame().ObjectDelete(ai);
                }
            }
            unit.m_AI.Clear();

            if (unit.m_Vehicle)
            {
                AIConvoySpawnLedger.Remove(unit.m_Vehicle);
                GetGame().ObjectDelete(unit.m_Vehicle);
                unit.m_Vehicle = null;
            }
        }

        m_State = STATE_DONE;
        m_Manager.OnEventEnded(this);
    }

    int StopByAdmin()
    {
        if (m_State == STATE_DONE)
            return 0;

        StopLoops();

        Log("Convoy '" + m_Config.Name + "' stopped from the admin menu - despawning");

        int spared = 0;
        foreach (AIConvoyVehicleUnit unit : m_Units)
        {
            CarScript stopCar = unit.GetCar();
            if (stopCar)
                stopCar.AIConvoy_StopAutopilot();

            if (unit.m_Group)
            {
                unit.m_Group.ClearAI(true);
                unit.m_Group = null;
            }
            else
            {
                foreach (eAIBase ai : unit.m_AI)
                {
                    if (ai)
                        GetGame().ObjectDelete(ai);
                }
            }
            unit.m_AI.Clear();

            if (unit.m_Vehicle)
            {
                if (HasPlayerAboard(unit.m_Vehicle))
                {
                    unit.m_Vehicle.SetLifetimeMax(0);
                    unit.m_Vehicle.SetLifetime(unit.m_Vehicle.GetLifetimeMax());
                    spared++;
                }
                else
                {
                    AIConvoySpawnLedger.Remove(unit.m_Vehicle);
                    GetGame().ObjectDelete(unit.m_Vehicle);
                }
                unit.m_Vehicle = null;
            }
        }

        if (spared > 0)
            Log("Convoy '" + m_Config.Name + "': " + spared + " vehicle(s) with a player aboard left in the world (lifetime handed back to the economy)");

        m_State = STATE_DONE;
        m_Manager.OnEventEnded(this);
        return spared;
    }

    protected bool HasPlayerAboard(Transport vehicle)
    {
        for (int i = 0; i < vehicle.CrewSize(); i++)
        {
            Human crew = vehicle.CrewMember(i);
            if (crew && PlayerBase.Cast(crew) && !eAIBase.Cast(crew))
                return true;
        }
        return false;
    }

    int GetState()
    {
        return m_State;
    }

    protected void Log(string msg)
    {
        Print("[Wild-Fields] " + msg);
    }
}
