/* Mod Launcher CustomCarSupport: a mod's CustomCarSupport.ini (port/compat/shar_modcars.cpp), per car either
 *
 *   [Car]                 (Mod Launcher 1.26+)        or     [fry_v]            (older: the section is the name)
 *   Name=fry_v                                               Index=97
 *   Index=97                                                 NoHusk=1
 *   NoHusk=1 ...
 *
 * Read: Index (car camera data chunk id, 97-255 for new cars), NoHusk, NoSkidMarks, NoFrontSkidMarks,
 * ShadowVisible, Abductable, Recolourable, Invincible, NoBumperCam. Not yet: PreviewScale, Husk, IsHusk,
 * RearWheelSparks/BackWheelSparks, steering visual and skid mark width multipliers, [FakeWheel] sections.
 * Cars the game does not know load and drive anyway (their vehicle ID is VehicleEnum::INVALID); the ini adds
 * their camera data and these switches. Read once, on first use; static storage only. */
#ifndef SHAR_COMPAT_SHAR_MODCARS_H
#define SHAR_COMPAT_SHAR_MODCARS_H

#ifdef __cplusplus
extern "C" {
#endif

/* -1 = not set (the game's own behaviour) */
typedef struct SharModCar
{
    char name[ 32 ];
    int index;
    int noHusk, noSkidMarks, noFrontSkidMarks, shadowVisible, abductable, recolourable, invincible, noBumperCam;
} SharModCar;

/* The settings for the car named (case-insensitive); NULL when no mod sets any. */
const SharModCar* shar_ModCar( const char* name );

#ifdef __cplusplus
}
#endif

#endif
