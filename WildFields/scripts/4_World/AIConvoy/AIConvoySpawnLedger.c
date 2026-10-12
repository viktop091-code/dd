class AIConvoySpawnRecord
{
    int ID1;
    int ID2;
    int ID3;
    int ID4;
    string ClassName;
    float X;
    float Y;
    float Z;

    void AIConvoySpawnRecord()
    {
        ID1 = 0; ID2 = 0; ID3 = 0; ID4 = 0;
        ClassName = "";
        X = 0; Y = 0; Z = 0;
    }

    bool MatchesId(int a, int b, int c, int d)
    {

        if (ID1 == 0 && ID2 == 0 && ID3 == 0 && ID4 == 0)
            return false;
        return ID1 == a && ID2 == b && ID3 == c && ID4 == d;
    }

    vector GetPos()
    {
        return Vector(X, Y, Z);
    }

    void SetPos(vector p)
    {
        X = p[0]; Y = p[1]; Z = p[2];
    }
}

class AIConvoySpawnLedgerData
{
    ref array<ref AIConvoySpawnRecord> Vehicles;

    void AIConvoySpawnLedgerData()
    {
        Vehicles = new array<ref AIConvoySpawnRecord>;
    }
}

class AIConvoySpawnLedger
{
    static const string PATH = "$profile:WildFields/SpawnedVehicles.json";

    protected static ref AIConvoySpawnLedgerData m_Data;

    protected static void EnsureLoaded()
    {
        if (m_Data)
            return;

        m_Data = new AIConvoySpawnLedgerData();

        if (!FileExist(PATH))
            return;

        string err;
        if (!JsonFileLoader<AIConvoySpawnLedgerData>.LoadFile(PATH, m_Data, err))
        {
            Print("[Wild-Fields] Could not read the spawn ledger (" + err + ") - starting a fresh one");
            m_Data = new AIConvoySpawnLedgerData();
        }
    }

    protected static void Save()
    {
        if (!m_Data)
            return;

        string err;
        if (!JsonFileLoader<AIConvoySpawnLedgerData>.SaveFile(PATH, m_Data, err))
            Print("[Wild-Fields] Could not write the spawn ledger: " + err);
    }

    static void Add(EntityAI vehicle)
    {
        if (!vehicle)
            return;

        EnsureLoaded();

        int b1, b2, b3, b4;
        vehicle.GetPersistentID(b1, b2, b3, b4);

        AIConvoySpawnRecord rec = new AIConvoySpawnRecord();
        rec.ID1 = b1; rec.ID2 = b2; rec.ID3 = b3; rec.ID4 = b4;
        rec.ClassName = vehicle.GetType();
        rec.SetPos(vehicle.GetPosition());

        m_Data.Vehicles.Insert(rec);
        Save();
    }

    static void UpdatePosition(EntityAI vehicle)
    {
        if (!vehicle)
            return;

        EnsureLoaded();

        int b1, b2, b3, b4;
        vehicle.GetPersistentID(b1, b2, b3, b4);

        for (int i = 0; i < m_Data.Vehicles.Count(); i++)
        {
            AIConvoySpawnRecord rec = m_Data.Vehicles[i];
            if (rec && rec.MatchesId(b1, b2, b3, b4))
            {
                rec.SetPos(vehicle.GetPosition());
                return;
            }
        }
    }

    static void Remove(EntityAI vehicle)
    {
        if (!vehicle)
            return;

        EnsureLoaded();

        if (m_Data.Vehicles.Count() == 0)
            return;

        int b1, b2, b3, b4;
        vehicle.GetPersistentID(b1, b2, b3, b4);

        bool changed = false;
        for (int i = m_Data.Vehicles.Count() - 1; i >= 0; i--)
        {
            AIConvoySpawnRecord rec = m_Data.Vehicles[i];
            if (rec && rec.MatchesId(b1, b2, b3, b4))
            {
                m_Data.Vehicles.Remove(i);
                changed = true;
            }
        }

        if (changed)
            Save();
    }

    static void Flush()
    {
        if (m_Data)
            Save();
    }

    static int SweepAndClear()
    {
        EnsureLoaded();

        int removed = 0;
        int recorded = m_Data.Vehicles.Count();

        if (recorded == 0)
            return 0;

        foreach (AIConvoySpawnRecord rec : m_Data.Vehicles)
        {
            if (!rec)
                continue;

            array<Object> objects = new array<Object>;
            array<CargoBase> cargos = new array<CargoBase>;
            GetGame().GetObjectsAtPosition3D(rec.GetPos(), 40.0, objects, cargos);

            foreach (Object obj : objects)
            {
                EntityAI ent = EntityAI.Cast(obj);
                if (!ent)
                    continue;

                if (ent.GetType() != rec.ClassName)
                    continue;

                int c1, c2, c3, c4;
                ent.GetPersistentID(c1, c2, c3, c4);

                if (rec.MatchesId(c1, c2, c3, c4))
                {
                    GetGame().ObjectDelete(ent);
                    removed++;
                }
            }
        }

        if (recorded > 0)
            Print("[Wild-Fields] SPAWN LEDGER: " + recorded + " vehicle(s) were still recorded from the previous session, removed " + removed);

        m_Data.Vehicles.Clear();
        Save();
        return removed;
    }
}
