#include "model.hpp"

namespace liero {

u8 NObjectType::_defaults[176] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 255, 255, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 240, 191, 
	0, 0, 0, 0, 0, 0, 240, 63, 
	0, 0, 0, 0, 0, 0, 240, 63, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	85, 85, 85, 85, 85, 85, 213, 63, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 255, 255, 255, 255, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 WeaponTypeList::_defaults[8] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 WeaponType::_defaults[72] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 PlayerSettings::_defaults[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 PlayerControls::_defaults[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 TcData::_defaults[288] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 64, 111, 64, 
	0, 0, 0, 0, 0, 32, 60, 64, 
	0, 0, 0, 0, 0, 64, 37, 64, 
	0, 0, 0, 0, 0, 64, 111, 64, 
	0, 0, 0, 0, 0, 0, 16, 64, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 80, 2, 0, 208, 0, 0, 
	220, 5, 0, 0, 184, 11, 0, 0, 
	0, 114, 0, 0, 0, 219, 0, 0, 
	112, 17, 1, 0, 160, 15, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 224, 122, 236, 63, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 64, 143, 234, 63, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	116, 0, 0, 0, 64, 0, 0, 0, 
	12, 0, 0, 0, 64, 0, 0, 0, 
	232, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 SObjectType::_defaults[32] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 255, 255, 255, 255, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

u8 LevelEffect::_defaults[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
};

}
