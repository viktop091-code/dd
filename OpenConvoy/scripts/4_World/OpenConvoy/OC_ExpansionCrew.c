class OC_ExpansionCrew
{
    static const float SEAT_TIMEOUT_SECONDS = 45.0;

    protected ref eAIGroup m_Group;
    protected ref array<eAIBase> m_Members;
    protected ref array<ref OC_CrewSeatAssignment> m_SeatAssignments;
    protected float m_SeatWaitTime;

    void OC_ExpansionCrew()
    {
        m_Members = new array<eAIBase>;
        m_SeatAssignments = new array<ref OC_CrewSeatAssignment>;
    }

    bool Initialize()
    {
        m_Group = eAIGroup.CreateGroup(new eAIFactionMercenaries());
        if (!m_Group)
        {
            Error("[Wild-Fields] Expansion AI group could not be created.");
            return false;
        }

        return true;
    }

    void AddVehicleCrew(OC_ConvoyCar vehicle, OC_Settings settings)
    {
        if (!m_Group || !vehicle || settings.AICharacterClasses.Count() == 0)
            return;

        int requestedCrew = settings.CrewPerVehicle;
        if (requestedCrew < 0)
            requestedCrew = 0;
        if (requestedCrew > vehicle.CrewSize())
            requestedCrew = vehicle.CrewSize();

        for (int seatIndex = 0; seatIndex < requestedCrew; seatIndex++)
        {
            vector spawnPosition = vehicle.GetPosition() + vehicle.GetDirection().Perpend() * 4.0 + vehicle.GetDirection() * (seatIndex * 1.5);
            spawnPosition[1] = GetGame().SurfaceY(spawnPosition[0], spawnPosition[2]);
            string characterClass = settings.AICharacterClasses.GetRandomElement();
            eAIBase member = eAIBase.Cast(GetGame().CreateObject(characterClass, spawnPosition));
            if (!member)
            {
                Error("[Wild-Fields] Expansion AI could not spawn " + characterClass + ".");
                continue;
            }

            member.SetPosition(spawnPosition);
            member.SetGroup(m_Group);
            member.eAI_SetUnlimitedReload(1);
            if (settings.AILoadout != "")
                ExpansionHumanLoadout.Apply(member, settings.AILoadout, false);
            else
                OC_ApplyLoadout(member, settings.AILoadoutItems);

            m_Members.Insert(member);
            m_SeatAssignments.Insert(new OC_CrewSeatAssignment(member, vehicle, seatIndex));
        }
    }

    void BeginSeating()
    {
        m_SeatWaitTime = 0.0;
        OC_TrySeatAll();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(OC_CheckSeated, 500, true);
    }

    protected void OC_TrySeatAll()
    {
        foreach (OC_CrewSeatAssignment assignment : m_SeatAssignments)
        {
            eAIBase member = assignment.Member;
            if (!member || !member.IsAlive() || member.IsInTransport() || !assignment.Vehicle)
                continue;

            HumanCommandVehicle vehicleCommand = member.GetCommand_Vehicle();
            if (vehicleCommand)
                continue;

            member.DisableSimulation(false);
            if (member.Expansion_IsAnimationIdle() && !member.eAI_GetTransport())
                member.Notify_Transport(assignment.Vehicle, assignment.SeatIndex);
        }
    }

    protected void OC_CheckSeated()
    {
        m_SeatWaitTime += 0.5;

        bool allSeated = true;
        foreach (OC_CrewSeatAssignment assignment : m_SeatAssignments)
        {
            if (assignment.Member && assignment.Member.IsAlive() && !assignment.Member.IsInTransport())
            {
                allSeated = false;
                break;
            }
        }

        if (!allSeated && m_SeatWaitTime < SEAT_TIMEOUT_SECONDS)
        {
            OC_TrySeatAll();
            return;
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(OC_CheckSeated);

        if (!allSeated)
        {
            for (int i = m_SeatAssignments.Count() - 1; i >= 0; i--)
            {
                eAIBase straggler = m_SeatAssignments[i].Member;
                if (straggler && straggler.IsAlive() && !straggler.IsInTransport())
                {
                    Print("[Wild-Fields] AI did not enter its vehicle before timeout; removing it.");
                    m_Members.RemoveItem(straggler);
                    GetGame().ObjectDelete(straggler);
                }
            }
        }

        OC_Manager.GetInstance().OnCrewReady();
    }

    void Cleanup()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(OC_CheckSeated);

        foreach (eAIBase member : m_Members)
        {
            if (member)
                GetGame().ObjectDelete(member);
        }

        m_Members.Clear();
        m_SeatAssignments.Clear();
        if (m_Group)
            eAIGroup.DeleteGroup(m_Group);
        m_Group = null;
    }

    void DismountAll()
    {
        foreach (eAIBase member : m_Members)
        {
            if (!member || !member.IsAlive())
                continue;

            HumanCommandVehicle vehicleCommand = member.GetCommand_Vehicle();
            if (vehicleCommand && !vehicleCommand.IsGettingOut())
                vehicleCommand.GetOutVehicle();
        }
    }

    protected void OC_ApplyLoadout(eAIBase member, array<string> items)
    {
        foreach (string itemClass : items)
        {
            EntityAI item = member.GetInventory().CreateAttachment(itemClass);
            if (!item)
                member.GetInventory().CreateInInventory(itemClass);
        }
    }
}

class OC_CrewSeatAssignment
{
    eAIBase Member;
    OC_ConvoyCar Vehicle;
    int SeatIndex;

    void OC_CrewSeatAssignment(eAIBase member, OC_ConvoyCar vehicle, int seatIndex)
    {
        Member = member;
        Vehicle = vehicle;
        SeatIndex = seatIndex;
    }
}
