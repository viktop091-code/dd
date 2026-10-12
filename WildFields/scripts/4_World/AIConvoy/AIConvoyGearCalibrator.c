class AIConvoyGearBand
{
    float Kph1k;
    float MaxSpeed;
    float MaxRpm;
    int   Samples;

    void AIConvoyGearBand(float kph1k)
    {
        Kph1k = kph1k;
        MaxSpeed = 0;
        MaxRpm = 0;
        Samples = 0;
    }

    void Add(float kph1k, float speed, float rpm)
    {

        Kph1k = ((Kph1k * Samples) + kph1k) / (Samples + 1);
        Samples++;

        if (speed > MaxSpeed)
            MaxSpeed = speed;
        if (rpm > MaxRpm)
            MaxRpm = rpm;
    }
}

class AIConvoyGearCalibrator
{
    protected string m_VehicleType;
    protected bool   m_IsManual;
    protected float  m_Redline;
    protected float  m_Idle;
    protected ref array<ref AIConvoyGearBand> m_Bands;

    protected const float BAND_TOLERANCE = 0.12;

    protected const int MIN_BAND_SAMPLES = 4;

    void AIConvoyGearCalibrator()
    {
        m_Bands = new array<ref AIConvoyGearBand>;
    }

    void Begin(Transport vehicle)
    {
        m_Bands.Clear();
        m_VehicleType = "";
        m_IsManual = false;
        m_Redline = 0;
        m_Idle = 0;

        CarScript car = CarScript.Cast(vehicle);
        if (!car)
            return;

        m_VehicleType = car.GetType();
        m_IsManual = car.GearboxGetType() == CarGearboxType.MANUAL;

        m_Redline = car.EngineGetRPMRedline();
        if (m_Redline <= 0)
            m_Redline = car.EngineGetRPMMax();

        m_Idle = car.EngineGetRPMIdle();
    }

    void Sample(Transport vehicle)
    {
        CarScript car = CarScript.Cast(vehicle);
        if (!car || !m_IsManual)
            return;

        float speed = car.GetSpeedometer();
        float rpm = car.EngineGetRPM();

        if (speed < 5.0)
            return;
        if (rpm < m_Idle * 1.2 || rpm < 200)
            return;

        float kph1k = speed / rpm * 1000.0;
        if (kph1k <= 0)
            return;

        foreach (AIConvoyGearBand band : m_Bands)
        {
            float diff = Math.AbsFloat(kph1k - band.Kph1k) / band.Kph1k;
            if (diff <= BAND_TOLERANCE)
            {
                band.Add(kph1k, speed, rpm);
                return;
            }
        }

        AIConvoyGearBand fresh = new AIConvoyGearBand(kph1k);
        fresh.Add(kph1k, speed, rpm);
        m_Bands.Insert(fresh);
    }

    protected void SortBands()
    {

        for (int i = 1; i < m_Bands.Count(); i++)
        {
            AIConvoyGearBand key = m_Bands[i];
            int j = i - 1;
            while (j >= 0 && m_Bands[j].Kph1k > key.Kph1k)
            {
                m_Bands.Set(j + 1, m_Bands[j]);
                j--;
            }
            m_Bands.Set(j + 1, key);
        }
    }

    protected void DropNoiseBands()
    {
        for (int i = m_Bands.Count() - 1; i >= 0; i--)
        {
            if (m_Bands[i].Samples < MIN_BAND_SAMPLES)
                m_Bands.Remove(i);
        }
    }

    string Report(out string suggestion)
    {
        suggestion = "";

        if (m_VehicleType == "")
            return "";

        if (!m_IsManual)
            return "[Convoy Gearbox] " + m_VehicleType + " has an AUTOMATIC gearbox - ManualShiftUpSpeeds is ignored for it, nothing to set.";

        DropNoiseBands();
        SortBands();

        if (m_Bands.Count() < 2)
            return "[Convoy Gearbox] " + m_VehicleType + ": only " + m_Bands.Count().ToString() + " gear(s) seen - not enough to judge. Drive it harder, accelerating up through the gears on flat ground.";

        float downAt = Math.Max(m_Idle * 1.35, m_Redline * 0.35);

        string report = "[Convoy Gearbox] " + m_VehicleType + " - " + m_Bands.Count().ToString() + " gears seen, redline " + Math.Round(m_Redline).ToString();
        string shifts = "";
        bool impossible = false;
        string warnings = "";

        for (int g = 0; g < m_Bands.Count() - 1; g++)
        {
            AIConvoyGearBand cur = m_Bands[g];
            AIConvoyGearBand next = m_Bands[g + 1];

            float shiftSpeed = cur.Kph1k * (m_Redline * 0.80) / 1000.0;

            float minLandSpeed = (downAt * 1.15) * next.Kph1k / 1000.0;
            if (shiftSpeed < minLandSpeed)
                shiftSpeed = minLandSpeed;

            if (shiftSpeed > cur.MaxSpeed)
            {
                float theoreticalMax = cur.Kph1k * m_Redline / 1000.0;
                if (minLandSpeed > theoreticalMax * 0.95)
                {

                    impossible = true;
                    warnings = warnings + " | gear " + (g + 1).ToString() + "->" + (g + 2).ToString() + " IMPOSSIBLE: tops out at " + Math.Round(theoreticalMax).ToString() + " km/h but the next gear needs " + Math.Round(minLandSpeed).ToString() + " to hold";
                }
                else
                {
                    warnings = warnings + " | gear " + (g + 1).ToString() + " never reached " + Math.Round(shiftSpeed).ToString() + " km/h while you drove (best " + Math.Round(cur.MaxSpeed).ToString() + ") - drive harder to confirm";
                    shiftSpeed = cur.MaxSpeed * 0.9;
                }
            }

            if (shifts != "")
                shifts = shifts + ", ";
            shifts = shifts + Math.Round(shiftSpeed).ToString();
        }

        suggestion = "[" + shifts + "]";

        report = report + " -> \"ManualShiftUpSpeeds\": " + suggestion;
        if (warnings != "")
            report = report + warnings;
        if (impossible)
            report = report + " | THIS GEARBOX CANNOT BE MADE TO WORK by shift points alone - the ratios are spaced too widely for the autopilot. Use a different vehicle, or re-gear it.";

        return report;
    }

    string Detail()
    {
        string s = "";
        for (int i = 0; i < m_Bands.Count(); i++)
        {
            AIConvoyGearBand b = m_Bands[i];
            s = s + " | gear " + (i + 1).ToString() + ": " + Math.Round(b.Kph1k * 10) / 10 + " km/h per 1000rpm, best " + Math.Round(b.MaxSpeed).ToString() + " km/h at " + Math.Round(b.MaxRpm).ToString() + " rpm (" + b.Samples.ToString() + " samples)";
        }
        return s;
    }
}
