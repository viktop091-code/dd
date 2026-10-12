modded class MissionGameplay
{

    protected ref AIConvoyAdminMenu m_AIConvoyMenu;

    protected bool m_AIConvoyMenuRequested;

    protected float m_AIConvoyMenuCooldown;

    override void OnInit()
    {
        super.OnInit();
        AIConvoyClient.OnMenuState().Insert(AIConvoy_OnMenuState);
    }

    override void OnMissionFinish()
    {

        AIConvoyClient.OnMenuState().Remove(AIConvoy_OnMenuState);
        super.OnMissionFinish();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (m_AIConvoyMenuCooldown > 0)
            m_AIConvoyMenuCooldown -= timeslice;

        UAInput key = GetUApi().GetInputByName("UAAIConvoyAdminMenu");
        if (!key || !key.LocalPress())
            return;

        if (GetGame().GetUIManager().GetMenu())
            return;

        if (m_AIConvoyMenuCooldown > 0)
            return;

        m_AIConvoyMenuCooldown = 1.0;
        m_AIConvoyMenuRequested = true;
        AIConvoyAdminMenu.RequestState();
    }

    void AIConvoy_OnMenuState(AIConvoyMenuState state)
    {
        if (!state)
            return;

        UIScriptedMenu current = GetGame().GetUIManager().GetMenu();

        AIConvoyAdminMenu openMenu = AIConvoyAdminMenu.Cast(current);
        if (openMenu)
        {
            openMenu.ApplyState(state);
            return;
        }

        if (!m_AIConvoyMenuRequested)
            return;
        m_AIConvoyMenuRequested = false;

        if (current)
            return;

        m_AIConvoyMenu = new AIConvoyAdminMenu();
        GetGame().GetUIManager().ShowScriptedMenu(m_AIConvoyMenu, null);
        m_AIConvoyMenu.ApplyState(state);
    }
}
