modded class eAIBase
{
    bool m_AIConvoy_NoKillFeed;
    bool m_AIConvoy_Lootable = true;

    bool m_AIConvoy_HideCorpse;

    override void Init()
    {
        super.Init();
        RegisterNetSyncVariableBool("m_AIConvoy_HideCorpse");
    }

    void AIConvoy_SetLootable(bool lootable)
    {
        m_AIConvoy_Lootable = lootable;
        m_AIConvoy_HideCorpse = !lootable;
        SetSynchDirty();
    }

    override bool IsInventoryVisible()
    {
        if (m_AIConvoy_HideCorpse && !IsAlive())
            return false;
        return super.IsInventoryVisible();
    }
    int  m_AIConvoy_BodyDespawnSeconds;

    ref array<EntityAI> m_AIConvoy_SpawnGear;

    void AIConvoy_RecordGear()
    {
        if (!m_AIConvoy_SpawnGear)
            m_AIConvoy_SpawnGear = new array<EntityAI>;

        m_AIConvoy_SpawnGear.Clear();

        array<EntityAI> all = new array<EntityAI>;
        GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, all);
        foreach (EntityAI e : all)
        {
            if (e && e != this)
                m_AIConvoy_SpawnGear.Insert(e);
        }

        EntityAI hands = GetHumanInventory().GetEntityInHands();
        if (hands && m_AIConvoy_SpawnGear.Find(hands) == -1)
            m_AIConvoy_SpawnGear.Insert(hands);
    }

    protected void AIConvoy_RuinItem(EntityAI item)
    {
        if (!item)
            return;

        item.SetAllowDamage(true);
        item.SetHealth("", "", 0.0);

        int attCount = item.GetInventory().AttachmentCount();
        for (int a = 0; a < attCount; a++)
        {
            EntityAI sub = item.GetInventory().GetAttachmentFromIndex(a);
            if (!sub)
                continue;

            sub.SetAllowDamage(true);
            sub.SetHealth("", "", 0.0);
        }
    }

    protected void AIConvoy_DestroyGear()
    {

        array<EntityAI> keep = new array<EntityAI>;

        int count = GetInventory().AttachmentCount();
        for (int i = 0; i < count; i++)
        {
            EntityAI att = GetInventory().GetAttachmentFromIndex(i);
            if (!att)
                continue;

            if (Weapon_Base.Cast(att))
                continue;

            array<EntityAI> contents = new array<EntityAI>;
            att.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, contents);
            foreach (EntityAI c : contents)
            {
                if (c && c != att)
                    GetGame().ObjectDelete(c);
            }

            AIConvoy_RuinItem(att);
            keep.Insert(att);
        }

        if (m_AIConvoy_SpawnGear)
        {
            foreach (EntityAI g : m_AIConvoy_SpawnGear)
            {
                if (!g)
                    continue;
                if (keep.Find(g) != -1)
                    continue;

                GetGame().ObjectDelete(g);
            }
        }

        EntityAI inHands = GetHumanInventory().GetEntityInHands();
        if (inHands)
            GetGame().ObjectDelete(inHands);
    }

    protected void AIConvoy_DespawnBody()
    {

        if (IsAlive())
            return;

        Delete();
    }

    override void EEDelete(EntityAI parent)
    {

        if (GetGame() && GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM))
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AIConvoy_DespawnBody);

        super.EEDelete(parent);
    }

    override void EEKilled(Object killer)
    {
        if (!m_AIConvoy_Lootable)
            AIConvoy_DestroyGear();

        if (m_AIConvoy_BodyDespawnSeconds > 0 && GetGame().IsServer())
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AIConvoy_DespawnBody, m_AIConvoy_BodyDespawnSeconds * 1000, false);

        if (m_AIConvoy_NoKillFeed)
        {
            ExpansionNotificationSettings notif = GetExpansionSettings().GetNotification();
            bool oldKillFeedAI = notif.KillFeedAI;
            notif.KillFeedAI = false;
            super.EEKilled(killer);
            notif.KillFeedAI = oldKillFeedAI;
            return;
        }

        super.EEKilled(killer);
    }

    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        if (m_AIConvoy_NoKillFeed)
        {
            ExpansionNotificationSettings notif = GetExpansionSettings().GetNotification();
            bool oldKillFeedAI = notif.KillFeedAI;
            notif.KillFeedAI = false;
            super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
            notif.KillFeedAI = oldKillFeedAI;
            return;
        }

        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }
}
