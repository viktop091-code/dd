modded class PlayerBase
{
    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (rpc_type == AIConvoyRPC.REQ_MENU)
        {

            if (!GetGame().IsServer() || !sender || !g_AIConvoyManager)
                return;

            string steamId = sender.GetPlainId();
            if (!g_AIConvoyManager.IsAdmin(steamId))
            {
                g_AIConvoyManager.NoteMenuDenied(steamId);
                return;
            }

            SendMenuState(sender, g_AIConvoyManager.BuildMenuState(steamId, ""));
        }
        else if (rpc_type == AIConvoyRPC.ACTION)
        {
            if (!GetGame().IsServer() || !sender || !g_AIConvoyManager)
                return;

            string actorId = sender.GetPlainId();
            if (!g_AIConvoyManager.IsAdmin(actorId))
            {
                g_AIConvoyManager.NoteMenuDenied(actorId);
                return;
            }

            AIConvoyMenuAction act = new AIConvoyMenuAction();
            if (!act.Read(ctx))
            {
                Print("[Wild-Fields] Admin menu: unreadable action from " + actorId + " - ignored");
                return;
            }

            bool closeMenu;
            string status = g_AIConvoyManager.HandleMenuAction(actorId, sender.GetName(), act, closeMenu);
            SendMenuState(sender, g_AIConvoyManager.BuildMenuState(actorId, status, closeMenu));
        }
        else if (rpc_type == AIConvoyRPC.MENU_STATE)
        {

            if (GetGame().IsServer())
                return;

            AIConvoyMenuState state = new AIConvoyMenuState();
            if (!state.Read(ctx))
            {
                Print("[Wild-Fields] Admin menu: could not read the server's reply - is this client running the same @AIConvoy build as the server?");
                return;
            }

            AIConvoyClient.OnMenuState().Invoke(state);
        }
    }

    protected void SendMenuState(PlayerIdentity to, AIConvoyMenuState state)
    {
        if (!state)
            return;

        ScriptRPC rpc = new ScriptRPC();
        state.Write(rpc);
        rpc.Send(this, AIConvoyRPC.MENU_STATE, true, to);
    }
}
