class OC_ConvoyCar: Hatchback_02
{
    protected ref array<vector> m_OC_Waypoints;
    protected int m_OC_WaypointIndex;
    protected float m_OC_TargetSpeedKmh;
    protected float m_OC_WaypointRadius;
    protected bool m_OC_RouteActive;
    protected bool m_OC_RouteComplete;

    void OC_ConvoyCar()
    {
        m_OC_Waypoints = new array<vector>;
        m_OC_WaypointIndex = 1;
        m_OC_TargetSpeedKmh = 30.0;
        m_OC_WaypointRadius = 8.0;
    }

    void OC_ConfigureRoute(array<vector> waypoints, float targetSpeedKmh, float waypointRadius)
    {
        m_OC_Waypoints.Clear();
        foreach (vector waypoint : waypoints)
            m_OC_Waypoints.Insert(waypoint);

        m_OC_WaypointIndex = 1;
        m_OC_TargetSpeedKmh = Math.Max(targetSpeedKmh, 5.0);
        m_OC_WaypointRadius = Math.Max(waypointRadius, 2.0);
        m_OC_RouteComplete = false;
        m_OC_RouteActive = m_OC_Waypoints.Count() >= 2;

        SetBrakesActivateWithoutDriver(false);
    }

    bool OC_IsRouteComplete()
    {
        return m_OC_RouteComplete;
    }

    void OC_HaltForCombat()
    {
        OC_StopAtDestination();
    }

    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

        if (!GetGame().IsServer() || !source)
            return;

        Man attacker = Man.Cast(source);
        if (!attacker)
            attacker = source.GetHierarchyRootPlayer();
        if (attacker)
            OC_Manager.GetInstance().OnVehicleAttacked(this);
    }

    override void EOnPostSimulate(IEntity other, float timeSlice)
    {
        super.EOnPostSimulate(other, timeSlice);

        if (!GetGame().IsServer() || !m_OC_RouteActive)
            return;

        OC_UpdateAutopilot(timeSlice);
    }

    protected void OC_UpdateAutopilot(float timeSlice)
    {
        if (m_OC_WaypointIndex >= m_OC_Waypoints.Count())
        {
            OC_StopAtDestination();
            return;
        }

        vector position = GetPosition();
        vector target = m_OC_Waypoints[m_OC_WaypointIndex];
        target[1] = position[1];

        vector toTarget = target - position;
        float distance = toTarget.Length();
        if (distance <= m_OC_WaypointRadius)
        {
            m_OC_WaypointIndex++;
            if (m_OC_WaypointIndex >= m_OC_Waypoints.Count())
                OC_StopAtDestination();
            return;
        }

        toTarget.Normalize();
        vector forward = GetDirection();
        forward[1] = 0;
        forward.Normalize();

        float cross = forward[0] * toTarget[2] - forward[2] * toTarget[0];
        float dot = vector.Dot(forward, toTarget);
        float steering = Math.Clamp(cross * 2.2, -1.0, 1.0);
        float turnPenalty = Math.Clamp(Math.AbsFloat(steering), 0.0, 0.75);
        float desiredSpeed = m_OC_TargetSpeedKmh * (1.0 - turnPenalty);
        float speed = GetSpeedometerAbsolute();

        if (!EngineIsOn())
            EngineStart();

        OC_UpdateGear();

        float throttle = Math.Clamp((desiredSpeed - speed) / 12.0, 0.0, 1.0);
        float brake = 0.0;

        if (dot < 0.0 || speed > desiredSpeed + 8.0)
        {
            throttle = 0.0;
            brake = Math.Clamp((speed - desiredSpeed) / 15.0, 0.2, 1.0);
        }

        SetSteering(steering);
        SetThrottle(throttle);
        SetBrake(brake);
        SetHandbrake(0.0);
        SetBrakesActivateWithoutDriver(false);
    }

    protected void OC_UpdateGear()
    {
        if (GearboxGetType() != CarGearboxType.MANUAL)
            return;

        int gear = GetGear();
        float rpm = EngineGetRPM();

        if (gear < CarGear.FIRST)
        {
            ShiftTo(CarGear.FIRST);
            return;
        }

        if (rpm > EngineGetRPMRedline() * 0.82 && gear < GetGearCount() - 1)
            ShiftUp();
        else if (rpm < EngineGetRPMIdle() * 1.6 && gear > CarGear.FIRST)
            ShiftDown();
    }

    protected void OC_StopAtDestination()
    {
        m_OC_RouteActive = false;
        m_OC_RouteComplete = true;
        SetThrottle(0.0);
        SetSteering(0.0);
        SetBrake(1.0);
        SetHandbrake(1.0);
    }
}
