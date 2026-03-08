import {Instance, CSPlayerController} from 'cs_script/point_script'

function ShowHudTestMessage(plr: CSPlayerController) {
    plr.ShowHudMessageHTML("<span color='#D11313'>Hello from Test input!</span>", 5.0);
}

function BombTest() {
    const c4 = Instance.FindEntityByClass("planted_c4");
    const currTime = Instance.GetGameTime();
    const time = c4?.GetSchemaField("CPlantedC4", "m_flC4Blow");
    if (time === undefined) {
        Instance.Msg("C4 not found!");
        return;
    }
    Instance.Msg(`C4 will explode in ${Number(time) - currTime} seconds.`);
}

function SendUserMessage()
{
    let msg = Instance.CreateUserMessage("Fade");
    msg.SetField("duration", 2.0 * 512);
    msg.SetField("hold_time", 0.1 * 512);
    msg.AddAllRecipients();
    msg.Send();
}

Instance.OnPlayerChat((info) => {
    if(!info.player)
        return;

    switch(info.text) {
        case "!test":
            ShowHudTestMessage(info.player);
            break;
        case "!bomb":
            BombTest();
            break;
        case "!msg":
            SendUserMessage();
            break;
    }
});

Instance.OnUserMessage({ messageName: "StartSoundEvent" , callback: (info) => {
    const soundHash = info.GetField<number>("soundevent_hash");
    Instance.Msg(`StartSoundEvent user message received. soundevent_hash = ${soundHash}`);

    return false; // block all sounds
}});