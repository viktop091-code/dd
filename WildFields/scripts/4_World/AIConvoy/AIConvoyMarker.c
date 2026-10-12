class AIConvoyMarker
{
    protected AIConvoyGeneralSettings m_Settings;

    protected bool m_Active;
    protected bool m_Stopped;
    protected vector m_LastPos;

    protected static bool s_MissingWarned;

    void AIConvoyMarker(AIConvoyGeneralSettings settings)
    {
        m_Settings = settings;
    }

    void ~AIConvoyMarker()
    {
        Remove();
    }

    protected bool MarkerBackendReady()
    {
        return false;
    }

    protected bool MarkerCreate(string label, vector pos, int color)
    {
        return false;
    }

    protected void MarkerDestroy()
    {
    }

    protected int MovingColor()
    {
        return ARGB(m_Settings.MarkerColorA, m_Settings.MarkerColorR, m_Settings.MarkerColorG, m_Settings.MarkerColorB);
    }

    protected int StoppedColor()
    {
        return ARGB(m_Settings.MarkerStoppedColorA, m_Settings.MarkerStoppedColorR, m_Settings.MarkerStoppedColorG, m_Settings.MarkerStoppedColorB);
    }

    protected string MovingName()
    {
        if (m_Settings.MarkerName != "")
            return m_Settings.MarkerName;
        if (m_Settings.NotificationTitle != "")
            return m_Settings.NotificationTitle;
        return "Hostile Convoy";
    }

    protected string StoppedName()
    {
        if (m_Settings.MarkerNameStopped != "")
            return m_Settings.MarkerNameStopped;
        return MovingName();
    }

    void Begin(vector pos)
    {
        if (!m_Settings.MarkerEnabled)
            return;

        Remove();

        if (!MarkerBackendReady())
        {
            if (!s_MissingWarned)
            {
                s_MissingWarned = true;
                Print("[Wild-Fields] MarkerEnabled is on, but map markers are not available in this version - the feature was withdrawn in v1.3.1 and will return in a later update. Convoys are unaffected; you can leave the setting alone or set it back to 0.");
            }
            return;
        }

        if (!MarkerCreate(MovingName(), pos, MovingColor()))
            return;

        m_Active = true;
        m_Stopped = false;
        m_LastPos = pos;

        Print("[Wild-Fields] Convoy map marker created at " + pos);
    }

    void MoveTo(vector pos)
    {
        if (!m_Active || m_Stopped)
            return;

        float minMove = m_Settings.MarkerMinMoveMeters;
        if (minMove < 10.0)
            minMove = 10.0;

        if (vector.Distance(m_LastPos, pos) < minMove)
            return;

        if (MarkerCreate(MovingName(), pos, MovingColor()))
            m_LastPos = pos;
    }

    void SetStopped(vector pos)
    {
        if (!m_Active)
            return;

        m_Stopped = true;

        if (MarkerCreate(StoppedName(), pos, StoppedColor()))
            m_LastPos = pos;

        Print("[Wild-Fields] Convoy map marker switched to stopped at " + pos);
    }

    void Remove()
    {
        if (!m_Active)
            return;

        MarkerDestroy();

        m_Active = false;
        m_Stopped = false;
        m_LastPos = vector.Zero;

        Print("[Wild-Fields] Convoy map marker removed");
    }

    bool IsActive()
    {
        return m_Active;
    }
}
