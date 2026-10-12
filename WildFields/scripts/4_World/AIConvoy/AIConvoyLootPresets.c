class AIConvoyLootEntry
{
    string presetName;
    string itemname;
    ref TStringArray items;

    float chance;
    int individualChance;
    int minSpawnTries;
    int maxSpawnTries;
    int allowDuplicates;

    float quantmin;
    float quantmax;

    float lootHealthMin;
    float lootHealthMax;

    string spawnIn;

    void AIConvoyLootEntry()
    {
        presetName = "";
        itemname = "";
        items = new TStringArray;
        chance = 1.0;

        individualChance = 0;

        minSpawnTries = 1;
        maxSpawnTries = 1;
        allowDuplicates = 0;
        quantmin = -1.0;
        quantmax = -1.0;
        lootHealthMin = -1.0;
        lootHealthMax = -1.0;
        spawnIn = "cargo";
    }
}

class AIConvoyLootPresetList
{
    ref array<ref AIConvoyLootEntry> base;
    ref array<ref AIConvoyLootEntry> items;

    void AIConvoyLootPresetList()
    {
        base = new array<ref AIConvoyLootEntry>;
        items = new array<ref AIConvoyLootEntry>;
    }
}

class AIConvoyLootPresetFile
{
    ref array<ref AIConvoyLootEntry> defaultItemConfigs;
    ref array<ref AIConvoyLootPresetList> presetLists;

    void AIConvoyLootPresetFile()
    {
        defaultItemConfigs = new array<ref AIConvoyLootEntry>;
        presetLists = new array<ref AIConvoyLootPresetList>;
    }
}

class AIConvoyLootPresets
{
    protected ref map<string, ref AIConvoyLootEntry> m_Entries;
    protected ref TStringArray m_Disabled;
    protected ref TStringArray m_WarnedClasses;

    protected ref TStringArray m_Suffixes;

    protected const int MAX_DEPTH = 20;

    protected bool m_UseLBmaster;

    protected bool m_AnnouncedLBmaster;

    void AIConvoyLootPresets()
    {
        m_Entries = new map<string, ref AIConvoyLootEntry>;
        m_Disabled = new TStringArray;
        m_WarnedClasses = new TStringArray;
        m_Suffixes = new TStringArray;
        m_UseLBmaster = true;
    }

    void SetUseLBmaster(bool use)
    {
        m_UseLBmaster = use;
    }

    int Count()
    {
        return m_Entries.Count();
    }

    static void WriteExample(string path)
    {
        AIConvoyLootPresetFile file = new AIConvoyLootPresetFile();

        AIConvoyLootEntry ammo = new AIConvoyLootEntry();
        ammo.presetName = "example_ammo_partial";
        ammo.itemname = "AmmoBox_556x45_20Rnd";
        ammo.individualChance = 0;
        ammo.quantmin = 0.25;
        ammo.quantmax = 0.75;
        file.defaultItemConfigs.Insert(ammo);

        AIConvoyLootEntry vest = new AIConvoyLootEntry();
        vest.presetName = "example_worn_vest";
        vest.itemname = "PressVest_Blue";
        vest.individualChance = 0;
        vest.lootHealthMin = 0.25;
        vest.lootHealthMax = 0.5;
        file.defaultItemConfigs.Insert(vest);

        AIConvoyLootEntry medkit = new AIConvoyLootEntry();
        medkit.presetName = "example_bandage";
        medkit.itemname = "BandageDressing";
        medkit.individualChance = 0;
        medkit.chance = 0.5;
        file.defaultItemConfigs.Insert(medkit);

        AIConvoyLootEntry hMag = new AIConvoyLootEntry();
        hMag.presetName = "_MAG";
        hMag.spawnIn = "mag";
        hMag.individualChance = 1;
        file.defaultItemConfigs.Insert(hMag);

        AIConvoyLootEntry hInd = new AIConvoyLootEntry();
        hInd.presetName = "_IND";
        hInd.spawnIn = "any";
        hInd.individualChance = 1;
        file.defaultItemConfigs.Insert(hInd);

        AIConvoyLootEntry hRoot = new AIConvoyLootEntry();
        hRoot.presetName = "_ROOT";
        hRoot.spawnIn = "root";
        hRoot.individualChance = 1;
        file.defaultItemConfigs.Insert(hRoot);

        AIConvoyLootEntry h50 = new AIConvoyLootEntry();
        h50.presetName = "_50%";
        h50.spawnIn = "any";
        h50.individualChance = 1;
        h50.chance = 0.5;
        file.defaultItemConfigs.Insert(h50);

        AIConvoyLootEntry crate = new AIConvoyLootEntry();
        crate.presetName = "example_supply_drop";
        crate.minSpawnTries = 1;
        crate.maxSpawnTries = 2;
        crate.items.Insert("example_ammo_partial");
        crate.items.Insert("example_worn_vest");
        crate.items.Insert("example_bandage");
        file.defaultItemConfigs.Insert(crate);

        string err;
        if (JsonFileLoader<AIConvoyLootPresetFile>.SaveFile(path, file, err))
            Print("[Wild-Fields] Wrote an example loot preset file to " + path + " - add \"CargoPresets\": [\"example_supply_drop\"] to a vehicle preset to try it.");
        else
            Print("[Wild-Fields] ERROR writing example loot presets: " + err);
    }

    void Load(string path)
    {
        m_Entries.Clear();
        m_Disabled.Clear();

        if (!FileExist(path))
        {
            Print("[Wild-Fields] No loot preset file at " + path + " - CargoPresets will do nothing. This is fine unless you meant to use them.");
            return;
        }

        Print("[Wild-Fields] Reading " + path);

        AIConvoyLootPresetFile file = new AIConvoyLootPresetFile();
        string err;
        if (!JsonFileLoader<AIConvoyLootPresetFile>.LoadFile(path, file, err))
        {
            Print("[Wild-Fields] ERROR reading loot presets from " + path + ": " + err);
            return;
        }

        if (file.defaultItemConfigs)
        {
            foreach (AIConvoyLootEntry entry : file.defaultItemConfigs)
            {
                if (!entry || entry.presetName == "")
                    continue;

                if (m_Entries.Contains(entry.presetName))
                {
                    Print("[Wild-Fields] Loot preset '" + entry.presetName + "' is defined more than once - the later one wins");
                }

                m_Entries.Set(entry.presetName, entry);
            }
        }

        int inLists = 0;
        if (file.presetLists)
        {
            foreach (AIConvoyLootPresetList list : file.presetLists)
            {
                if (!list)
                    continue;

                if (list.base)
                    inLists += list.base.Count();
                if (list.items)
                    inLists += list.items.Count();
            }

            file.presetLists.Clear();
        }

        if (inLists > 0)
        {
            #ifdef LBmaster_Rework
            if (m_UseLBmaster)
                Print("[Wild-Fields] " + inLists + " preset(s) in this file live in 'presetLists'. That is LBmaster's own layout and LBmaster Core is installed, so it resolves those names itself - they do not need to be read here.");
            else
                Print("[Wild-Fields] " + inLists + " preset(s) in this file live in 'presetLists', which only LBmaster reads - but LBmasterPresets is set to 0, so nothing will resolve them. Set it back to 1, or move the presets into 'defaultItemConfigs'.");
            #else
            Print("[Wild-Fields] " + inLists + " preset(s) in this file live in 'presetLists'. That is an LBmaster file layout and it needs LBmaster Core installed to work - this mod no longer reads it. Install LBmaster Core, or move those presets into 'defaultItemConfigs'.");
            #endif
        }

        if (m_Entries.Count() == 0)
        {
            Print("[Wild-Fields] Loot preset file " + path + " contains no usable presets - CargoPresets will do nothing.");
            return;
        }

        Normalise();
        BuildSuffixList();

        Print("[Wild-Fields] Loaded " + m_Entries.Count() + " loot preset(s) from " + path);

        Validate();
    }

    protected void Normalise()
    {
        foreach (string name, AIConvoyLootEntry entry : m_Entries)
        {
            if (!entry)
                continue;

            if (entry.lootHealthMin <= 0 && entry.lootHealthMax <= 0)
            {
                entry.lootHealthMin = -1.0;
                entry.lootHealthMax = -1.0;
            }

            if (entry.quantmin <= 0 && entry.quantmax <= 0)
            {
                entry.quantmin = -1.0;
                entry.quantmax = -1.0;
            }

            if (entry.chance <= 0)
                entry.chance = 1.0;

            if (entry.spawnIn == "")
                entry.spawnIn = "cargo";

            if (entry.minSpawnTries <= 0)
                entry.minSpawnTries = 1;

            if (entry.maxSpawnTries < entry.minSpawnTries)
                entry.maxSpawnTries = entry.minSpawnTries;

            if (!entry.items)
                entry.items = new TStringArray;
        }
    }

    protected void BuildSuffixList()
    {
        m_Suffixes.Clear();

        foreach (string name, AIConvoyLootEntry unused : m_Entries)
        {
            if (name.IndexOf("_") == 0)
                m_Suffixes.Insert(name);
        }

        for (int i = 1; i < m_Suffixes.Count(); i++)
        {
            string key = m_Suffixes.Get(i);
            int j = i - 1;
            while (j >= 0 && m_Suffixes.Get(j).Length() < key.Length())
            {
                m_Suffixes.Set(j + 1, m_Suffixes.Get(j));
                j--;
            }
            m_Suffixes.Set(j + 1, key);
        }
    }

    protected string StripHelperSuffix(string name, out AIConvoyLootEntry helper)
    {
        helper = null;

        foreach (string suffix : m_Suffixes)
        {
            int cut = name.Length() - suffix.Length();
            if (cut <= 0)
                continue;

            if (name.Substring(cut, suffix.Length()) != suffix)
                continue;

            helper = m_Entries.Get(suffix);
            return name.Substring(0, cut);
        }

        return "";
    }

    protected string SampleSuffix(string sample)
    {
        if (sample == "")
            return "";

        return " (" + sample + ")";
    }

    protected string AppendSample(string sample, int count, string name)
    {
        if (count > 3)
            return sample;

        if (sample != "")
            return sample + ", '" + name + "'";

        return "'" + name + "'";
    }

    protected void Validate()
    {
        array<string> names = new array<string>;
        foreach (string key, AIConvoyLootEntry unused : m_Entries)
            names.Insert(key);

        foreach (string name : names)
        {
            TStringArray path = new TStringArray;
            string cycle;
            if (FindCycle(name, path, cycle))
            {
                m_Disabled.Insert(name);
                Print("[Wild-Fields] Loot preset '" + name + "' is CIRCULAR (" + cycle + ") and has been disabled. Nothing else is affected.");
            }
        }

        int empties = 0;
        string emptySample = "";
        foreach (string emptyName : names)
        {
            AIConvoyLootEntry check = m_Entries.Get(emptyName);
            if (!check)
                continue;

            if (emptyName.IndexOf("_") == 0)
                continue;

            if (check.itemname != "")
                continue;

            if (check.items && check.items.Count() > 0)
                continue;

            empties++;
            emptySample = AppendSample(emptySample, empties, emptyName);
        }

        if (empties > 0)
            Print("[Wild-Fields] " + empties + " loot preset(s) have no itemname and no items, so they can never spawn anything - check for a mistyped field name." + SampleSuffix(emptySample));

        int dangling = 0;
        string sample = "";
        foreach (string checkName : names)
        {
            AIConvoyLootEntry entry = m_Entries.Get(checkName);
            if (!entry || !entry.items)
                continue;

            foreach (string child : entry.items)
            {
                if (child == "" || IsSpawnable(child))
                    continue;

                dangling++;
                if (dangling <= 5)
                {
                    if (sample != "")
                        sample = sample + ", ";
                    sample = sample + "'" + child + "' (in '" + checkName + "')";
                }
            }
        }

        if (dangling > 0)
            Print("[Wild-Fields] " + dangling + " loot preset reference(s) are neither a defined preset nor an item class on this server and will be skipped. First few: " + sample);
    }

    protected bool FindCycle(string name, TStringArray pathSoFar, out string cycleText)
    {
        cycleText = "";

        if (pathSoFar.Find(name) != -1)
        {
            cycleText = "";
            foreach (string step : pathSoFar)
                cycleText = cycleText + step + " -> ";
            cycleText = cycleText + name;
            return true;
        }

        AIConvoyLootEntry entry = m_Entries.Get(name);
        if (!entry || !entry.items)
            return false;

        pathSoFar.Insert(name);

        foreach (string child : entry.items)
        {
            if (child == "" || !m_Entries.Contains(child))
                continue;

            if (FindCycle(child, pathSoFar, cycleText))
                return true;
        }

        pathSoFar.Remove(pathSoFar.Count() - 1);
        return false;
    }

    #ifdef LBmaster_Rework
    protected int SpawnViaLBmaster(EntityAI container, string presetName)
    {

        if (!LB_PresetLoader.Get)
            return -1;

        TStringArray wanted = new TStringArray;
        wanted.Insert(presetName);

        array<LB_PresetBase> found = LB_PresetLoader.Get.FindPresets(wanted);
        if (!found || found.Count() == 0)
            return -1;

        if (!m_AnnouncedLBmaster)
        {
            m_AnnouncedLBmaster = true;

            Print("[Wild-Fields] LBmaster Core detected - CargoPresets names are resolved by LBmaster itself. This is the recommended setup, so leave LBmasterPresets at 1. Setting it to 0 makes this mod read those names instead, and it cannot see presets kept in an LBmaster 'presetLists' block - read the LBmaster section of the README before changing it.");
        }

        int before = container.GetInventory().CountInventory();

        CarScript vehicle = CarScript.Cast(container);
        array<EntityAI> fittedBefore = new array<EntityAI>;
        if (vehicle)
        {
            for (int a = 0; a < vehicle.GetInventory().AttachmentCount(); a++)
                fittedBefore.Insert(vehicle.GetInventory().GetAttachmentFromIndex(a));
        }

        LB_PresetBase.SpawnPresets(null, found, container);

        if (vehicle)
            MoveLootOffVehicleSlots(vehicle, fittedBefore);

        int made = container.GetInventory().CountInventory() - before;
        if (made < 0)
            made = 0;

        return made;
    }

    protected void MoveLootOffVehicleSlots(CarScript vehicle, array<EntityAI> fittedBefore)
    {

        array<EntityAI> fittedByLoot = new array<EntityAI>;
        for (int i = 0; i < vehicle.GetInventory().AttachmentCount(); i++)
        {
            EntityAI att = vehicle.GetInventory().GetAttachmentFromIndex(i);
            if (att && fittedBefore.Find(att) == -1)
                fittedByLoot.Insert(att);
        }

        int moved = 0;
        foreach (EntityAI item : fittedByLoot)
        {
            ItemBase ib = ItemBase.Cast(item);
            if (ib)
            {
                ib.m_AIConvoy_ForwardPlayerDamage = false;
                if (ib.IsLockedInSlot())
                    ib.UnlockFromParent();
            }

            if (vehicle.ServerTakeEntityToCargo(item))
            {
                moved++;
                continue;
            }

            Print("[Wild-Fields] LBmaster fitted loot '" + item.GetType() + "' onto a " + vehicle.GetType() + " and its cargo had no room for it - left in the slot, but removable once the convoy is stopped.");
        }

        if (moved > 0)
            Print("[Wild-Fields] Moved " + moved + " loot item(s) that LBmaster had fitted onto a " + vehicle.GetType() + " into its cargo, so players can take them.");
    }
    #endif

    int SpawnInto(EntityAI container, string presetName)
    {
        if (!container || presetName == "")
            return 0;

        if (m_Disabled.Find(presetName) != -1)
            return 0;

        #ifdef LBmaster_Rework
        if (m_UseLBmaster)
        {
            int viaLB = SpawnViaLBmaster(container, presetName);
            if (viaLB >= 0)
                return viaLB;
        }
        #endif

        AIConvoyLootEntry entry = m_Entries.Get(presetName);
        if (!entry)
        {

            #ifdef LBmaster_Rework
            if (m_UseLBmaster)
            {
                Print("[Wild-Fields] CargoPresets names unknown loot preset '" + presetName + "' - not found by LBmaster, and not in this mod's own preset file either. Check the spelling and the case.");
                return 0;
            }
            #endif

            Print("[Wild-Fields] CargoPresets names unknown loot preset '" + presetName + "'");
            return 0;
        }

        if (entry.chance < 1.0 && Math.RandomFloat01() > entry.chance)
            return 0;

        return SpawnNode(container, entry, 0);
    }

    protected int SpawnNode(EntityAI parent, AIConvoyLootEntry entry, int depth)
    {
        if (!parent || !entry || depth > MAX_DEPTH)
            return 0;

        int count = entry.minSpawnTries;
        if (entry.maxSpawnTries > entry.minSpawnTries)
            count = Math.RandomIntInclusive(entry.minSpawnTries, entry.maxSpawnTries);

        if (count < 1)
            count = 1;

        if (entry.itemname == "")
            return SpawnChildren(parent, entry, depth, count);

        int stacks = 1;
        if (entry.quantmax > 1.0)
        {
            stacks = Math.RandomIntInclusive(1, Math.Round(entry.quantmax));
            if (stacks < 1)
                stacks = 1;
        }

        int made = 0;

        for (int i = 0; i < count; i++)
        {
            for (int s = 0; s < stacks; s++)
            {
                EntityAI created = CreateItem(parent, entry);
                if (!created)
                    return made;

                if (stacks == 1)
                    ApplyQuantity(created, entry);

                ApplyHealth(created, entry);

                made++;

                made += SpawnChildren(created, entry, depth, 1);

                SyncWeaponState(created);
            }
        }

        return made;
    }

    protected int SpawnChildren(EntityAI parent, AIConvoyLootEntry entry, int depth, int draws)
    {
        if (!entry.items || entry.items.Count() == 0)
            return 0;

        int made = 0;

        TStringArray pool = new TStringArray;
        TFloatArray weights = new TFloatArray;

        foreach (string childName : entry.items)
        {
            if (childName == "" || !IsSpawnable(childName))
                continue;

            float childChance;
            if (ChildIsIndividual(childName, childChance))
            {

                if (childChance < 1.0 && Math.RandomFloat01() > childChance)
                    continue;

                made += SpawnChild(parent, childName, depth);
                continue;
            }

            pool.Insert(childName);
            weights.Insert(childChance);
        }

        if (pool.Count() == 0)
            return made;

        for (int i = 0; i < draws; i++)
        {
            if (pool.Count() == 0)
                break;

            int idx = PickWeighted(weights);
            if (idx < 0)
                break;

            string pick = pool.Get(idx);

            if (entry.allowDuplicates == 0)
            {
                pool.Remove(idx);
                weights.Remove(idx);
            }

            made += SpawnChild(parent, pick, depth);
        }

        return made;
    }

    protected bool ChildIsIndividual(string childName, out float outChance)
    {
        outChance = 1.0;

        AIConvoyLootEntry child = m_Entries.Get(childName);
        if (!child)
        {
            AIConvoyLootEntry helper;
            string bare = StripHelperSuffix(childName, helper);
            if (bare != "" && helper)
                child = helper;
        }

        if (!child)
            return true;

        outChance = child.chance;
        return child.individualChance != 0;
    }

    protected int PickWeighted(TFloatArray weights)
    {
        if (!weights || weights.Count() == 0)
            return -1;

        float total = 0;
        for (int i = 0; i < weights.Count(); i++)
        {
            if (weights.Get(i) > 0)
                total += weights.Get(i);
        }

        if (total <= 0)
            return Math.RandomInt(0, weights.Count());

        float roll = Math.RandomFloatInclusive(0, total);
        float running = 0;

        for (int j = 0; j < weights.Count(); j++)
        {
            float w = weights.Get(j);
            if (w <= 0)
                continue;

            running += w;
            if (roll <= running)
                return j;
        }

        return weights.Count() - 1;
    }

    protected int SpawnChild(EntityAI parent, string childName, int depth)
    {
        if (childName == "")
            return 0;

        AIConvoyLootEntry child = m_Entries.Get(childName);
        if (child)
            return SpawnNode(parent, child, depth + 1);

        if (ClassExists(childName))
        {

            AIConvoyLootEntry leaf = new AIConvoyLootEntry();
            leaf.presetName = childName;
            leaf.itemname = childName;

            return SpawnNode(parent, leaf, depth + 1);
        }

        AIConvoyLootEntry helper;
        string bare = StripHelperSuffix(childName, helper);
        if (bare != "" && helper)
        {
            AIConvoyLootEntry inner = m_Entries.Get(bare);
            if (inner)
                return SpawnNode(parent, inner, depth + 1);

            if (ClassExists(bare))
            {

                AIConvoyLootEntry mod = new AIConvoyLootEntry();
                mod.presetName = childName;
                mod.itemname = bare;
                mod.spawnIn = helper.spawnIn;
                mod.chance = helper.chance;

                return SpawnNode(parent, mod, depth + 1);
            }
        }

        WarnNameOnce(childName);
        return 0;
    }

    protected bool IsSpawnable(string name)
    {
        if (m_Entries.Contains(name))
            return true;

        if (ClassExists(name))
            return true;

        AIConvoyLootEntry helper;
        string bare = StripHelperSuffix(name, helper);
        if (bare == "" || !helper)
            return false;

        if (m_Entries.Contains(bare))
            return true;

        return ClassExists(bare);
    }

    protected bool ClassExists(string cls)
    {
        if (cls == "")
            return false;

        if (GetGame().ConfigIsExisting("CfgVehicles " + cls))
            return true;

        if (GetGame().ConfigIsExisting("CfgWeapons " + cls))
            return true;

        if (GetGame().ConfigIsExisting("CfgMagazines " + cls))
            return true;

        return false;
    }

    protected void WarnNameOnce(string name)
    {
        if (m_WarnedClasses.Find(name) != -1)
            return;

        m_WarnedClasses.Insert(name);
        Print("[Wild-Fields] Loot preset entry '" + name + "' is neither a defined preset nor an item class on this server - skipped.");
    }

    static void SyncWeaponState(EntityAI item)
    {
        Weapon_Base weapon = Weapon_Base.Cast(item);
        if (!weapon)
            return;

        weapon.RandomizeFSMState();
        weapon.Synchronize();
    }

    protected EntityAI CreateItem(EntityAI parent, AIConvoyLootEntry entry)
    {
        EntityAI made;
        string where = entry.spawnIn;
        EntityAI target = parent;

        if (where == "root")
        {
            EntityAI hierarchyRoot = EntityAI.Cast(parent.GetHierarchyRoot());
            if (hierarchyRoot)
                target = hierarchyRoot;
        }

        if (CarScript.Cast(target))
            where = "cargo";

        if (where == "att" || where == "mag")
        {
            made = target.GetInventory().CreateAttachment(entry.itemname);
        }
        else if (where == "cargo")
        {
            made = target.GetInventory().CreateEntityInCargo(entry.itemname);
        }
        else
        {

            made = target.GetInventory().CreateAttachment(entry.itemname);
            if (!made)
                made = target.GetInventory().CreateEntityInCargo(entry.itemname);
        }

        if (!made)
            WarnClassOnce(entry);

        return made;
    }

    protected void WarnClassOnce(AIConvoyLootEntry entry)
    {
        if (m_WarnedClasses.Find(entry.itemname) != -1)
            return;

        m_WarnedClasses.Insert(entry.itemname);
        Print("[Wild-Fields] Loot preset '" + entry.presetName + "' could not create '" + entry.itemname + "' (unknown classname, no room, or no slot for spawnIn '" + entry.spawnIn + "'). Warned once.");
    }

    protected void ApplyQuantity(EntityAI ent, AIConvoyLootEntry entry)
    {
        if (entry.quantmin < 0)
            return;

        ItemBase item = ItemBase.Cast(ent);
        if (!item)
            return;

        float hi = entry.quantmax;
        if (hi < entry.quantmin)
            hi = entry.quantmin;

        float roll = Math.RandomFloatInclusive(entry.quantmin, hi);

        if (roll <= 1.0)
            item.SetQuantityNormalized(roll, false);
        else
            item.SetQuantity(roll, false);
    }

    protected void ApplyHealth(EntityAI ent, AIConvoyLootEntry entry)
    {
        if (entry.lootHealthMin < 0)
            return;

        float hiH = entry.lootHealthMax;
        if (hiH < entry.lootHealthMin)
            hiH = entry.lootHealthMin;

        ent.SetHealth01("", "", Math.RandomFloatInclusive(entry.lootHealthMin, hiH));
    }
}
