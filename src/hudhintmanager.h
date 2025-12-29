#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "ehandle.h"
#include "entity/ccsplayercontroller.h"

struct HudHintInfo {
	HudHintInfo(CPlayerSlot _slot, std::string _msg, double _endTime) 
		: targetSlot(_slot), strMessage(_msg), endTime(_endTime) {}
	CPlayerSlot targetSlot; // what player to send this to (userid)
	std::string strMessage; // message contents
	double endTime; // game time at which the message expires
};

class HudHintManager {
public:

	HudHintManager();
	void AddHintMessage(CPlayerSlot targetPlr, const std::string msg, double duration);
	void CancelHintMessage(CPlayerSlot targetPlr);
	void CancelAllHintMessages();
	void Update();

private:
	void Display(const HudHintInfo& info);
	std::vector<HudHintInfo> hintMessages;
};