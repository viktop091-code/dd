class AIConvoyRPC
{
    static const int REQ_MENU   = 883210001;
    static const int MENU_STATE = 883210002;
    static const int ACTION     = 883210003;
}

class AIConvoyMenuActionType
{
    static const int TOGGLE_ROUTE = 1;
    static const int LAUNCH_ROUTE = 2;
    static const int SET_INTERVAL = 3;
    static const int SET_RECORDER = 4;
    static const int TELEPORT_TO  = 5;
    static const int STOP_ROUTE   = 6;
}

class AIConvoyMenuAction
{
    int Type;
    string RouteName;
    bool Value;
    int MinMinutes;
    int MaxMinutes;

    void Write(ParamsWriteContext ctx)
    {
        ctx.Write(Type);
        ctx.Write(RouteName);
        ctx.Write(Value);
        ctx.Write(MinMinutes);
        ctx.Write(MaxMinutes);
    }

    bool Read(ParamsReadContext ctx)
    {
        if (!ctx.Read(Type))
            return false;
        if (!ctx.Read(RouteName))
            return false;
        if (!ctx.Read(Value))
            return false;
        if (!ctx.Read(MinMinutes))
            return false;
        if (!ctx.Read(MaxMinutes))
            return false;
        return true;
    }
}

class AIConvoyRouteInfo
{
    string Name;
    bool Enabled;
    bool Running;
    int WaypointCount;
    string Column;
}

class AIConvoyMenuState
{
    ref array<ref AIConvoyRouteInfo> Routes;

    int MinIntervalMinutes;
    int MaxIntervalMinutes;

    bool RecorderArmed;
    bool RecorderRecording;

    int ActiveConvoys;
    int MaxActiveConvoys;

    string Status;

    bool CloseMenu;

    void AIConvoyMenuState()
    {
        Routes = new array<ref AIConvoyRouteInfo>;
    }

    void Write(ParamsWriteContext ctx)
    {
        ctx.Write(MinIntervalMinutes);
        ctx.Write(MaxIntervalMinutes);
        ctx.Write(RecorderArmed);
        ctx.Write(RecorderRecording);
        ctx.Write(ActiveConvoys);
        ctx.Write(MaxActiveConvoys);
        ctx.Write(Status);
        ctx.Write(CloseMenu);

        int count = Routes.Count();
        ctx.Write(count);

        foreach (AIConvoyRouteInfo info : Routes)
        {
            ctx.Write(info.Name);
            ctx.Write(info.Enabled);
            ctx.Write(info.Running);
            ctx.Write(info.WaypointCount);
            ctx.Write(info.Column);
        }
    }

    bool Read(ParamsReadContext ctx)
    {
        if (!ctx.Read(MinIntervalMinutes))
            return false;
        if (!ctx.Read(MaxIntervalMinutes))
            return false;
        if (!ctx.Read(RecorderArmed))
            return false;
        if (!ctx.Read(RecorderRecording))
            return false;
        if (!ctx.Read(ActiveConvoys))
            return false;
        if (!ctx.Read(MaxActiveConvoys))
            return false;
        if (!ctx.Read(Status))
            return false;
        if (!ctx.Read(CloseMenu))
            return false;

        int count;
        if (!ctx.Read(count))
            return false;

        if (count < 0 || count > 1000)
            return false;

        string rowName;
        bool rowEnabled;
        bool rowRunning;
        int rowWaypoints;
        string rowColumn;

        for (int i = 0; i < count; i++)
        {
            if (!ctx.Read(rowName))
                return false;
            if (!ctx.Read(rowEnabled))
                return false;
            if (!ctx.Read(rowRunning))
                return false;
            if (!ctx.Read(rowWaypoints))
                return false;
            if (!ctx.Read(rowColumn))
                return false;

            AIConvoyRouteInfo info = new AIConvoyRouteInfo();
            info.Name = rowName;
            info.Enabled = rowEnabled;
            info.Running = rowRunning;
            info.WaypointCount = rowWaypoints;
            info.Column = rowColumn;
            Routes.Insert(info);
        }

        return true;
    }
}

class AIConvoyClient
{
    protected static ref ScriptInvoker s_OnMenuState;

    static ScriptInvoker OnMenuState()
    {
        if (!s_OnMenuState)
            s_OnMenuState = new ScriptInvoker();
        return s_OnMenuState;
    }
}
