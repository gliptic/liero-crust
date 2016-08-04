#include "config.hpp"

namespace liero {

#define LIERO_CDEFS(_) \
	_(NRInitialLength) \
	_(NRAttachLength) \
	_(MinBounceUp) \
	_(MinBounceDown) \
	_(MinBounceLeft) \
	_(MinBounceRight) \
	_(WormGravity) \
	_(WalkVel) \
	_(MaxVel) \
	_(JumpForce) \
	_(MaxAimVel) \
	_(AimAcc) \
	_(NinjaropeGravity) \
	_(NRMinLength) \
	_(NRMaxLength) \
	_(BonusGravity) \
	_(WormFricMult) \
	_(WormFricDiv) \
	_(WormMinSpawnDistLast) \
	_(WormMinSpawnDistEnemy) \
	_(WormSpawnRectX) \
	_(WormSpawnRectY) \
	_(WormSpawnRectW) \
	_(WormSpawnRectH) \
	_(AimFricMult) \
	_(NRThrowVelX) \
	_(NRThrowVelY) \
	_(NRForceShlX) \
	_(NRForceDivX) \
	_(NRForceShlY) \
	_(NRForceDivY) \
	_(NRForceLenShl) \
	_(BonusBounceMul) \
	_(BonusBounceDiv) \
	_(BonusFlickerTime) \
	_(AimMaxRight) \
	_(AimMinRight) \
	_(AimMaxLeft) \
	_(AimMinLeft) \
	_(NRPullVel) \
	_(NRReleaseVel) \
	_(NRColourBegin) \
	_(NRColourEnd) \
	_(BonusExplodeRisk) \
	_(BonusHealthVar) \
	_(BonusMinHealth) \
	_(LaserWeapon) \
	_(FirstBloodColour) \
	_(NumBloodColours) \
	_(BObjGravity) \
	_(BonusDropChance) \
	_(SplinterLarpaVelDiv) \
	_(SplinterCracklerVelDiv) \
	_(BloodStepUp) \
	_(BloodStepDown) \
	_(BloodLimit) \
	_(FallDamageRight) \
	_(FallDamageLeft) \
	_(FallDamageDown) \
	_(FallDamageUp) \
	_(WormFloatLevel) \
	_(WormFloatPower) \
	_(BonusSpawnRectX) \
	_(BonusSpawnRectY) \
	_(BonusSpawnRectW) \
	_(BonusSpawnRectH) \
	_(RemExpObject)

#define DEFENUMC(x) C##x,

enum {
	LIERO_CDEFS(DEFENUMC)
	MaxC
};

#define LC(name) (mod.c[C##name])
#if LIERO_FIXED
#define LF(name) Fixed::from_raw(mod.c[C##name])
#else
#define LF(name) mod.c[C##name]
#endif

}