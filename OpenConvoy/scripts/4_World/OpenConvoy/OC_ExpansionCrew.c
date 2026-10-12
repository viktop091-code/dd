class OC_ExpansionCrew
{
    protected ref eAIGroup m_Group;
    protected ref array<eAIBase> m_Members;

    void OC_ExpansionCrew()
    {
        m_Members = new array<eAIBase>;
    }

    bool Initialize()
    {
        m_Group = eAIGroup.CreateGroup(new eAIFactionMercenaries());
        if (!m_Group)
        {
            Error("[OpenConvoy] Expansion AI group could not be created.");
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
            vector spawnPosition = vehicle.GetPosition() + Vector(0, 0.5, 0);
            string characterClass = settings.AICharacterClasses.GetRandomElement();
            eAIBase member = eAIBase.Cast(GetGame().CreateObjectEx(characterClass, spawnPosition, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
            if (!member)
            {
                Error("[OpenConvoy] Expansion AI could not spawn " + characterClass + ".");
                continue;
            }

            member.SetGroup(m_Group);
            member.eAI_SetUnlimitedReload(1);
            OC_ApplyLoadout(member, settings.AILoadoutItems);

            int animationSeat = DayZPlayerConstants.VEHICLESEAT_CODRIVER;
            if (seatIndex == 0)
                animationSeat = DayZPlayerConstants.VEHICLESEAT_DRIVER;

            HumanCommandVehicle vehicleCommand = member.StartCommand_Vehicle(vehicle, seatIndex, animationSeat);
            if (vehicleCommand)
                vehicleCommand.SetVehicleType(vehicle.GetAnimInstance());
            else
                Error("[OpenConvoy] AI could not enter seat " + seatIndex + ".");

            m_Members.Insert(member);
        }
    }

    void Cleanup()
    {
        foreach (eAIBase member : m_Members)
        {
            if (member)
                GetGame().ObjectDelete(member);
        }

        m_Members.Clear();
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
            member.GetInventory().CreateInInventory(itemClass);
    }
}
