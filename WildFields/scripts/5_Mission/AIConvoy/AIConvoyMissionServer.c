modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();

        if (GetGame().IsServer())
        {

            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AIConvoy_CreateManager, 10000, false);
        }
    }

    private void AIConvoy_CreateManager()
    {
        g_AIConvoyManager = new AIConvoyManager();
    }
}
