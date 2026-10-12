modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(OC_StartConvoyManager, 1000, false);
    }

    void ~MissionServer()
    {
        OC_Manager.GetInstance().Stop();
    }

    protected void OC_StartConvoyManager()
    {
        OC_Manager.GetInstance().Start();
    }
}
