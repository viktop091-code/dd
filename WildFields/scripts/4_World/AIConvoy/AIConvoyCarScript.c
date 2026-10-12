modded class CarScript
{
    bool m_AIConvoy_TrackPlayerDamage;
    float m_AIConvoy_PlayerDamage;
    bool m_AIConvoy_Locked;
    bool m_AIConvoy_IsConvoyVehicle;

    bool m_AIConvoy_Autopilot;
    vector m_AIConvoy_TargetPos;
    float m_AIConvoy_TargetSpeedKmh;
    CarScript m_AIConvoy_FollowVehicle;
    float m_AIConvoy_FollowGap;
    float m_AIConvoy_SpeedCap;
    ref TFloatArray m_AIConvoy_ShiftUpSpeeds;
    float m_AIConvoy_SpeedErrInt;
    float m_AIConvoy_StartRetryAccu;
    int   m_AIConvoy_StartAttempts;
    bool  m_AIConvoy_StartGaveUp;
    float m_AIConvoy_UnreachAccu;
    bool  m_AIConvoy_SpeedWarned;

    static ref TStringArray s_AIConvoy_SpeedWarnedClasses;
    bool  m_AIConvoy_Overtaking;
    bool m_AIConvoy_OverRev;

    void CarScript()
    {

        RegisterNetSyncVariableBool("m_AIConvoy_Locked");
    }

    void AIConvoy_SetLocked(bool locked)
    {
        m_AIConvoy_Locked = locked;

        if (GetInventory())
        {
            if (locked)
                GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
            else
                GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);
        }

        SetSynchDirty();
    }

    override void EEItemAttached(EntityAI item, string slot_name)
    {
        super.EEItemAttached(item, slot_name);

        if (!m_AIConvoy_IsConvoyVehicle || !item)
            return;

        ItemBase ib = ItemBase.Cast(item);
        if (!ib)
            return;

        if (!ib.IsLockedInSlot())
            ib.LockToParent();

        ib.m_AIConvoy_ForwardPlayerDamage = m_AIConvoy_TrackPlayerDamage;
    }

    override bool CanDisplayCargo()
    {
        if (m_AIConvoy_Locked)
            return false;
        return super.CanDisplayCargo();
    }

    override bool CanReleaseCargo(EntityAI cargo)
    {
        if (m_AIConvoy_Locked)
            return false;
        return super.CanReleaseCargo(cargo);
    }

    override bool CanReleaseAttachment(EntityAI attachment)
    {
        if (m_AIConvoy_Locked)
            return false;
        return super.CanReleaseAttachment(attachment);
    }

    override bool CrewCanGetThrough(int posIdx)
    {
        if (m_AIConvoy_Locked)
            return false;
        return super.CrewCanGetThrough(posIdx);
    }

    static bool AIConvoy_IsPlayerDamage(int damageType, string ammo)
    {
        if (ammo == "EnviroDmg" || ammo == "TransportHit")
            return false;

        if (damageType == DamageType.FIRE_ARM || damageType == DamageType.EXPLOSION || damageType == DamageType.CLOSE_COMBAT)
            return true;

        if (ammo.Contains("Explosion") || ammo.Contains("Grenade"))
            return true;

        return false;
    }

    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        if (m_AIConvoy_TrackPlayerDamage && AIConvoy_IsPlayerDamage(damageType, ammo))
            m_AIConvoy_PlayerDamage += damageResult.GetDamage(dmgZone, "Health");

        return super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void OnInput(float dt)
    {
        super.OnInput(dt);

        if (m_AIConvoy_Autopilot && GetGame().IsServer())
            AIConvoy_Drive(dt);
    }

    protected void AIConvoy_Drive(float dt)
    {
        dBodyActive(this, ActiveState.ACTIVE);
        DisableSimulation(false);

        SetBrakesActivateWithoutDriver(false);
        SetHandbrake(0);

        if (!EngineIsOn())
        {
            m_AIConvoy_StartRetryAccu += dt;
            if (m_AIConvoy_StartRetryAccu >= 3.0)
            {
                m_AIConvoy_StartRetryAccu = 0;
                m_AIConvoy_StartAttempts++;

                if (m_AIConvoy_StartAttempts <= 5)
                {
                    EngineStart();
                }
                else if (!m_AIConvoy_StartGaveUp)
                {
                    m_AIConvoy_StartGaveUp = true;
                    Print("[Wild-Fields] " + GetType() + " could not start its engine after 5 attempts and has been left behind. Check the vehicle preset has a battery and spark plug, and that the route does not start in water.");
                }
            }
            return;
        }

        if (m_AIConvoy_StartAttempts > 0)
        {
            m_AIConvoy_StartAttempts = 0;
            m_AIConvoy_StartRetryAccu = 0;
            m_AIConvoy_StartGaveUp = false;
        }

        bool night = GetGame().GetWorld().IsNight();
        if (night != LightIsOn())
        {
            if (night)
                LightOn();
            else
                LightOff();
        }

        if (GearboxGetType() == CarGearboxType.MANUAL)
        {

            if (!m_AIConvoy_ShiftUpSpeeds || m_AIConvoy_ShiftUpSpeeds.Count() == 0)
            {
                m_AIConvoy_ShiftUpSpeeds = new TFloatArray;
                m_AIConvoy_ShiftUpSpeeds.Insert(8.0);
                m_AIConvoy_ShiftUpSpeeds.Insert(17.0);
            }

            int gear = GetGear();
            int desiredGear = gear;
            float manualSpeed = GetSpeedometer();

            float redline = EngineGetRPMRedline();
            if (redline <= 0)
                redline = EngineGetRPMMax();
            if (redline <= 0)
                redline = 3000;

            float rpm = EngineGetRPM();
            float idle = EngineGetRPMIdle();
            if (idle <= 0)
                idle = redline * 0.2;

            float upAt = redline * 0.72;
            float downAt = Math.Max(idle * 1.35, redline * 0.35);

            int topGear = CarGear.FIRST + m_AIConvoy_ShiftUpSpeeds.Count();

            int highestGear = GetGearCount() - 1;
            if (highestGear >= CarGear.FIRST && topGear > highestGear)
                topGear = highestGear;

            if (gear <= CarGear.NEUTRAL)
            {
                desiredGear = CarGear.FIRST;
            }
            else
            {
                int gearIdx = gear - CarGear.FIRST;

                bool fastEnough = true;
                if (gearIdx < m_AIConvoy_ShiftUpSpeeds.Count())
                    fastEnough = manualSpeed >= m_AIConvoy_ShiftUpSpeeds[gearIdx] * 0.6;

                bool speedSaysUp = false;
                if (gearIdx < m_AIConvoy_ShiftUpSpeeds.Count())
                    speedSaysUp = manualSpeed >= m_AIConvoy_ShiftUpSpeeds[gearIdx];

                bool wantUp = false;
                if (rpm > upAt || speedSaysUp)
                    wantUp = true;

                if (wantUp && gear < topGear && fastEnough)
                    desiredGear = gear + 1;
                else if (rpm < downAt && gear > CarGear.FIRST)
                    desiredGear = gear - 1;
            }

            if (desiredGear != gear)
                ShiftTo(desiredGear);

            if (rpm > redline * 0.98)
                m_AIConvoy_OverRev = true;
            else if (rpm < redline * 0.88)
                m_AIConvoy_OverRev = false;
        }
        else if (GearboxGetMode() != CarAutomaticGearboxMode.D)
        {
            ShiftTo(CarAutomaticGearboxMode.D);
        }

        vector fwd = GetDirection();
        fwd[1] = 0;

        vector toTarget = m_AIConvoy_TargetPos - GetPosition();
        toTarget[1] = 0;

        float steer = 0;
        if (fwd.Length() > 0.001 && toTarget.Length() > 0.001)
        {
            fwd.Normalize();
            toTarget.Normalize();

            float dot = fwd[0] * toTarget[0] + fwd[2] * toTarget[2];
            float cross = fwd[2] * toTarget[0] - fwd[0] * toTarget[2];
            float steerRad = Math.Atan2(cross, dot);
            steer = Math.Clamp(steerRad / 0.6, -1.0, 1.0);
        }
        SetSteering(steer);

        float slope = GetDirection()[1];

        float speedBudget = m_AIConvoy_TargetSpeedKmh;

        if (slope > 0.05)
            speedBudget = Math.Max(speedBudget * (1.0 - slope * 2.0), 10.0);

        bool emergencyStop = false;
        if (m_AIConvoy_FollowVehicle)
        {
            float gap = vector.Distance(GetPosition(), m_AIConvoy_FollowVehicle.GetPosition());
            float gapError = gap - m_AIConvoy_FollowGap;
            speedBudget = Math.Clamp(m_AIConvoy_FollowVehicle.GetSpeedometer() + gapError * 2.0, 0.0, speedBudget * 2.0);

            if (gap < m_AIConvoy_FollowGap * 0.5)
                speedBudget = 0;

            float closingSpeed = GetSpeedometer() - m_AIConvoy_FollowVehicle.GetSpeedometer();
            if (gap < m_AIConvoy_FollowGap * 0.5 && closingSpeed > 5.0)
                emergencyStop = true;
        }

        if (emergencyStop)
        {
            SetThrottle(0);
            SetBrake(1.0);
            m_AIConvoy_SpeedErrInt = 0;
            return;
        }

        if (m_AIConvoy_SpeedCap > 0 && speedBudget > m_AIConvoy_SpeedCap)
            speedBudget = m_AIConvoy_SpeedCap;

        float targetSpeed = speedBudget * (1.0 - 0.5 * Math.AbsFloat(steer));
        if (targetSpeed < 12.0 && !m_AIConvoy_FollowVehicle && m_AIConvoy_SpeedCap <= 0)
            targetSpeed = 12.0;

        if (targetSpeed < 5.0 && m_AIConvoy_FollowVehicle)
        {

            SetThrottle(0);
            SetBrake(1.0);
            return;
        }

        float speedError = targetSpeed - GetSpeedometer();

        if (!m_AIConvoy_SpeedWarned && m_AIConvoy_SpeedCap <= 0 && slope < 0.02 && targetSpeed > 1.0)
        {
            float warnRedline = EngineGetRPMRedline();
            if (warnRedline <= 0)
                warnRedline = EngineGetRPMMax();

            bool engineMaxed = warnRedline > 0 && EngineGetRPM() > warnRedline * 0.92;

            bool wellShort = GetSpeedometer() < m_AIConvoy_TargetSpeedKmh * 0.85;

            if (engineMaxed && wellShort)
                m_AIConvoy_UnreachAccu += dt;
            else
                m_AIConvoy_UnreachAccu = 0;

            if (m_AIConvoy_UnreachAccu > 20.0)
            {
                m_AIConvoy_SpeedWarned = true;

                if (!s_AIConvoy_SpeedWarnedClasses)
                    s_AIConvoy_SpeedWarnedClasses = new TStringArray;

                if (s_AIConvoy_SpeedWarnedClasses.Find(GetType()) == -1)
                {
                    s_AIConvoy_SpeedWarnedClasses.Insert(GetType());

                    float shownSpeed = Math.Round(GetSpeedometer() * 10.0) / 10.0;
                    Print("[Wild-Fields] WARNING: " + GetType() + " cannot reach this route's SpeedKmh of " + m_AIConvoy_TargetSpeedKmh.ToString() + " - it is holding " + shownSpeed.ToString() + " km/h with the engine already at its redline, so it has no higher gear left. The whole convoy will travel at this vehicle's maximum. Lower the route's SpeedKmh, or use a faster vehicle for this route.");
                }
            }
        }

        if (m_AIConvoy_OverRev)
        {

            if (slope > 0.05)
            {

                SetThrottle(0.5);
                SetBrake(0);
            }
            else if (speedError < -2.0)
            {

                SetThrottle(0);
                SetBrake(0.4);
            }
            else
            {

                SetThrottle(0.35);
                SetBrake(0);
            }
        }
        else if (speedError > 0)
        {
            SetBrake(0);

            m_AIConvoy_SpeedErrInt += speedError * dt;

            m_AIConvoy_SpeedErrInt = Math.Clamp(m_AIConvoy_SpeedErrInt, -10.0, 15.0);

            float throttle = Math.Clamp(speedError * 0.08 + m_AIConvoy_SpeedErrInt * 0.05, 0.15, 1.0);

            if (slope > 0.02)
                throttle = Math.Clamp(throttle + slope * 1.5, throttle, 1.0);

            SetThrottle(throttle);
        }
        else if (speedError < -1.5 || (slope < -0.05 && speedError < -4.0))
        {

            SetThrottle(0);
            SetBrake(0.35);

            m_AIConvoy_SpeedErrInt = m_AIConvoy_SpeedErrInt + speedError * dt;
            m_AIConvoy_SpeedErrInt = Math.Clamp(m_AIConvoy_SpeedErrInt, -10.0, 15.0);
        }
        else
        {

            SetThrottle(0.12);
            SetBrake(0);

            m_AIConvoy_SpeedErrInt = m_AIConvoy_SpeedErrInt + speedError * dt;
            m_AIConvoy_SpeedErrInt = Math.Clamp(m_AIConvoy_SpeedErrInt, -10.0, 15.0);
        }
    }

    void AIConvoy_PrimeEngine()
    {
        if (!GetGame().IsServer() || EngineIsOn())
            return;

        dBodyActive(this, ActiveState.ACTIVE);
        DisableSimulation(false);

        EngineStart();
    }

    void AIConvoy_StartAutopilot(float targetSpeedKmh)
    {
        m_AIConvoy_TargetSpeedKmh = targetSpeedKmh;
        m_AIConvoy_Autopilot = true;
        m_AIConvoy_SpeedErrInt = 0;
    }

    void AIConvoy_StopAutopilot()
    {
        if (!m_AIConvoy_Autopilot)
            return;

        m_AIConvoy_Autopilot = false;
        SetThrottle(0);
        SetSteering(0);
        SetBrake(1.0);
        SetHandbrake(1.0);
        SetBrakesActivateWithoutDriver(true);

        if (EngineIsOn())
            EngineStop();
    }
}

modded class ItemBase
{
    bool m_AIConvoy_ForwardPlayerDamage;

    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {

        if (m_AIConvoy_ForwardPlayerDamage && CarScript.AIConvoy_IsPlayerDamage(damageType, ammo))
        {
            CarScript parentCar = CarScript.Cast(GetHierarchyParent());
            if (parentCar && parentCar.m_AIConvoy_TrackPlayerDamage)
                parentCar.m_AIConvoy_PlayerDamage += damageResult.GetDamage(dmgZone, "Health");
        }

        return super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }
}
