class AIConvoyAdminMenu extends UIScriptedMenu
{
    static const string LAYOUT     = "WildFields/scripts/gui/layouts/admin_menu.layout";
    static const string ROW_LAYOUT = "WildFields/scripts/gui/layouts/admin_route_row.layout";

    static const int NAME_MAX_CHARS = 24;
    static const int INFO_MAX_CHARS = 53;

    static const float STOP_CONFIRM_SECONDS = 4.0;

    static const string DISCORD_URL = "https://github.com/viktop091-code/dd";

    static const int LINK_COLOR       = 0xFF7289DA;
    static const int LINK_COLOR_HOVER = 0xFFB7C4F4;

    protected ButtonWidget   m_CloseButton;
    protected CheckBoxWidget m_RecorderBox;
    protected TextWidget     m_RecorderState;
    protected EditBoxWidget  m_MinEdit;
    protected EditBoxWidget  m_MaxEdit;
    protected ButtonWidget   m_ApplyInterval;
    protected TextWidget     m_ConvoyCount;
    protected ScrollWidget   m_RouteScroller;
    protected Widget         m_RouteList;
    protected TextWidget     m_Status;
    protected ButtonWidget   m_DiscordLink;
    protected TextWidget     m_DiscordText;

    protected ref map<Widget, string> m_RowToggle;
    protected ref map<Widget, string> m_RowLaunch;
    protected ref map<Widget, string> m_RowTeleport;
    protected ref map<Widget, string> m_RowStop;

    protected ButtonWidget m_StopPending;
    protected float m_StopPendingLeft;

    protected bool m_Applying;

    void AIConvoyAdminMenu()
    {
        m_RowToggle = new map<Widget, string>;
        m_RowLaunch = new map<Widget, string>;
        m_RowTeleport = new map<Widget, string>;
        m_RowStop = new map<Widget, string>;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets(LAYOUT);
        if (!layoutRoot)
        {
            Print("[Wild-Fields] Admin menu layout failed to load: " + LAYOUT + " - is it packed into the PBO? (.layout must pass the build's staging filter)");
            return null;
        }

        m_CloseButton   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("CloseButton"));
        m_RecorderBox   = CheckBoxWidget.Cast(layoutRoot.FindAnyWidget("RecorderBox"));
        m_RecorderState = TextWidget.Cast(layoutRoot.FindAnyWidget("RecorderState"));
        m_MinEdit       = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("MinEdit"));
        m_MaxEdit       = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("MaxEdit"));
        m_ApplyInterval = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ApplyIntervalButton"));
        m_ConvoyCount   = TextWidget.Cast(layoutRoot.FindAnyWidget("ConvoyCount"));
        m_RouteScroller = ScrollWidget.Cast(layoutRoot.FindAnyWidget("RouteScroller"));
        m_RouteList     = layoutRoot.FindAnyWidget("RouteList");
        m_Status        = TextWidget.Cast(layoutRoot.FindAnyWidget("Status"));
        m_DiscordLink   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("DiscordLink"));
        m_DiscordText   = TextWidget.Cast(layoutRoot.FindAnyWidget("DiscordText"));

        SetStatus("Loading...");
        return layoutRoot;
    }

    override void OnShow()
    {
        super.OnShow();

        MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
        if (mission)
        {
            mission.GetHud().ShowHud(false);
            mission.GetHud().ShowQuickBar(false);
            mission.AddActiveInputExcludes({"menu"});
            mission.AddActiveInputRestriction(EInputRestrictors.INVENTORY);
        }

        GetUApi().SupressNextFrame(true);
    }

    override void OnHide()
    {
        super.OnHide();

        MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
        if (mission)
        {

            mission.RemoveActiveInputExcludes({"menu"}, true);
            mission.RemoveActiveInputRestriction(EInputRestrictors.INVENTORY);
            mission.GetHud().ShowHud(true);
            mission.GetHud().ShowQuickBar(true);
        }
    }

    void ApplyState(AIConvoyMenuState s)
    {
        if (!s || !layoutRoot)
            return;

        m_Applying = true;

        if (m_RecorderBox)
            m_RecorderBox.SetChecked(s.RecorderArmed);

        if (m_RecorderState)
        {

            if (s.RecorderRecording)
                m_RecorderState.SetText("Status: recording - get out of the vehicle to save the route.");
            else if (s.RecorderArmed)
                m_RecorderState.SetText("Status: armed, waiting for you to get in a vehicle.");
            else
                m_RecorderState.SetText("Status: off.");
        }

        if (m_MinEdit && GetFocus() != m_MinEdit)
            m_MinEdit.SetText(s.MinIntervalMinutes.ToString());
        if (m_MaxEdit && GetFocus() != m_MaxEdit)
            m_MaxEdit.SetText(s.MaxIntervalMinutes.ToString());

        if (m_ConvoyCount)
            m_ConvoyCount.SetText("Convoys running: " + s.ActiveConvoys + " / " + s.MaxActiveConvoys);

        RebuildRoutes(s);

        m_Applying = false;

        SetStatus(s.Status);

        if (s.CloseMenu)
            Close();
    }

    protected void RebuildRoutes(AIConvoyMenuState s)
    {
        if (!m_RouteList)
            return;

        ClearRows();

        foreach (AIConvoyRouteInfo info : s.Routes)
        {
            Widget row = GetGame().GetWorkspace().CreateWidgets(ROW_LAYOUT, m_RouteList);
            if (!row)
            {
                Print("[Wild-Fields] Admin menu row layout failed to load: " + ROW_LAYOUT);
                return;
            }

            CheckBoxWidget toggle = CheckBoxWidget.Cast(row.FindAnyWidget("RowEnabled"));
            TextWidget rowName    = TextWidget.Cast(row.FindAnyWidget("RowName"));
            TextWidget rowInfo    = TextWidget.Cast(row.FindAnyWidget("RowInfo"));
            TextWidget running    = TextWidget.Cast(row.FindAnyWidget("RowRunning"));
            ButtonWidget launch   = ButtonWidget.Cast(row.FindAnyWidget("RowLaunch"));
            ButtonWidget teleport = ButtonWidget.Cast(row.FindAnyWidget("RowTeleport"));
            ButtonWidget stop     = ButtonWidget.Cast(row.FindAnyWidget("RowStop"));

            if (toggle)
            {
                toggle.SetChecked(info.Enabled);
                m_RowToggle.Set(toggle, info.Name);
            }

            if (rowName)
                rowName.SetText(Shorten(info.Name, NAME_MAX_CHARS));

            if (rowInfo)
            {

                string suffix = " - " + info.WaypointCount + " wp";
                rowInfo.SetText(Shorten(info.Column, INFO_MAX_CHARS - suffix.Length()) + suffix);
            }

            if (running)
                running.Show(info.Running);

            if (launch)
            {

                m_RowLaunch.Set(launch, info.Name);
            }

            if (teleport)
            {
                teleport.Show(info.Running);
                if (info.Running)
                    m_RowTeleport.Set(teleport, info.Name);
            }

            if (stop)
            {
                stop.Show(info.Running);
                if (info.Running)
                    m_RowStop.Set(stop, info.Name);
            }
        }

        m_RouteList.Update();
        if (m_RouteScroller)
        {
            m_RouteScroller.Update();
            m_RouteScroller.VScrollToPos(0);
        }
    }

    protected void ClearRows()
    {
        m_RowToggle.Clear();
        m_RowLaunch.Clear();
        m_RowTeleport.Clear();
        m_RowStop.Clear();

        m_StopPending = null;
        m_StopPendingLeft = 0;

        Widget child = m_RouteList.GetChildren();
        while (child)
        {
            Widget next = child.GetSibling();
            child.Unlink();
            child = next;
        }
    }

    protected void SetStatus(string text)
    {
        if (m_Status)
            m_Status.SetText(text);
    }

    protected string Shorten(string text, int maxChars)
    {
        if (maxChars < 1)
            return "";
        if (text.Length() <= maxChars)
            return text;
        if (maxChars <= 3)
            return text.Substring(0, maxChars);
        return text.Substring(0, maxChars - 3) + "...";
    }

    static void RequestState()
    {
        Man player = GetGame().GetPlayer();
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Send(player, AIConvoyRPC.REQ_MENU, true, null);
    }

    protected void SendAction(AIConvoyMenuAction act)
    {
        Man player = GetGame().GetPlayer();
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        act.Write(rpc);
        rpc.Send(player, AIConvoyRPC.ACTION, true, null);
        SetStatus("...");
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        super.OnClick(w, x, y, button);

        if (w == m_CloseButton)
        {
            Close();
            return true;
        }

        if (w == m_DiscordLink)
        {
            GetGame().OpenURL(DISCORD_URL);
            return true;
        }

        if (w == m_ApplyInterval)
        {
            AIConvoyMenuAction setInterval = new AIConvoyMenuAction();
            setInterval.Type = AIConvoyMenuActionType.SET_INTERVAL;
            setInterval.MinMinutes = m_MinEdit.GetText().ToInt();
            setInterval.MaxMinutes = m_MaxEdit.GetText().ToInt();
            SendAction(setInterval);
            return true;
        }

        string routeName;
        if (m_RowLaunch.Find(w, routeName))
        {
            AIConvoyMenuAction launch = new AIConvoyMenuAction();
            launch.Type = AIConvoyMenuActionType.LAUNCH_ROUTE;
            launch.RouteName = routeName;
            SendAction(launch);
            return true;
        }

        string goToRoute;
        if (m_RowTeleport.Find(w, goToRoute))
        {
            AIConvoyMenuAction goTo = new AIConvoyMenuAction();
            goTo.Type = AIConvoyMenuActionType.TELEPORT_TO;
            goTo.RouteName = goToRoute;
            SendAction(goTo);
            return true;
        }

        string stopRoute;
        if (m_RowStop.Find(w, stopRoute))
        {
            ButtonWidget stopButton = ButtonWidget.Cast(w);

            if (!stopButton || stopButton != m_StopPending)
            {
                CancelStopConfirm();
                if (stopButton)
                {
                    m_StopPending = stopButton;
                    m_StopPendingLeft = STOP_CONFIRM_SECONDS;
                    stopButton.SetText("Sure?");
                }
                SetStatus("Click again to stop '" + stopRoute + "' and despawn its vehicles, crew and bodies.");
                return true;
            }

            m_StopPending = null;
            m_StopPendingLeft = 0;

            AIConvoyMenuAction stopAct = new AIConvoyMenuAction();
            stopAct.Type = AIConvoyMenuActionType.STOP_ROUTE;
            stopAct.RouteName = stopRoute;
            SendAction(stopAct);
            return true;
        }

        CancelStopConfirm();
        return false;
    }

    protected void CancelStopConfirm()
    {
        if (!m_StopPending)
            return;

        m_StopPending.SetText("Stop");
        m_StopPending = null;
        m_StopPendingLeft = 0;
        SetStatus("");
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        super.OnChange(w, x, y, finished);

        if (m_Applying)
            return false;

        if (w == m_RecorderBox)
        {
            AIConvoyMenuAction rec = new AIConvoyMenuAction();
            rec.Type = AIConvoyMenuActionType.SET_RECORDER;
            rec.Value = m_RecorderBox.IsChecked();
            SendAction(rec);
            return true;
        }

        string routeName;
        if (m_RowToggle.Find(w, routeName))
        {
            CheckBoxWidget box = CheckBoxWidget.Cast(w);
            AIConvoyMenuAction toggle = new AIConvoyMenuAction();
            toggle.Type = AIConvoyMenuActionType.TOGGLE_ROUTE;
            toggle.RouteName = routeName;
            toggle.Value = box && box.IsChecked();
            SendAction(toggle);
            return true;
        }

        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (w == m_DiscordLink && m_DiscordText)
        {
            m_DiscordText.SetColor(LINK_COLOR_HOVER);
            return true;
        }
        return super.OnMouseEnter(w, x, y);
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (w == m_DiscordLink && m_DiscordText)
        {
            m_DiscordText.SetColor(LINK_COLOR);
            return true;
        }
        return super.OnMouseLeave(w, enterW, x, y);
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);

        if (m_StopPending)
        {
            m_StopPendingLeft -= timeslice;
            if (m_StopPendingLeft <= 0)
                CancelStopConfirm();
        }

        Widget focus = GetFocus();
        bool typing = focus && (focus == m_MinEdit || focus == m_MaxEdit);

        if (GetUApi().GetInputByID(UAUIBack).LocalPress())
        {
            Close();
            return;
        }

        if (!typing)
        {
            UAInput toggleKey = GetUApi().GetInputByName("UAAIConvoyAdminMenu");
            if (toggleKey && toggleKey.LocalPress())
                Close();
        }
    }
}
