# Mods (Lucas' Mod Launcher format)

Docs: docs.donutteam.com/docs/LucasSimpsonsHitAndRunModLauncher/

## Loading (`port/compat/shar_mods.cpp`)
- Mod = folder: `Meta.ini` (`RequiredHack=`), `CustomFiles/`, per-hack configs, optional Lua (`CustomFiles.lua`,
  `CustomFiles.ini`). `.lmlm` = "LSPA" package (512-byte sectors; `mod/tools/lmlm_extract.py`), read in place.
- `SHAR_MODS` ':'-separated, earlier wins. Unsupported hacks reported at startup.
- Every read-only open -> `shar_ModResolve` once (`requests.cpp` -> `shar_ModTakeResolved` in `linuxdrive.cpp`;
  `binkffmpeg.cpp`): Lua handlers, CustomFiles.ini (OccludedPath, PathRedirections/AdditionalFiles), CustomFiles.
  Result: mod file (packaged = fd on .lmlm + slice), Lua output (memfd), redirect, or missing.
- Sound opens by key (decimal radMakeCaseInsensitiveKey32 of backslashed path): files/rules indexed by key too.
- Lua 5.4.8 (`port/third_party/lua`): one sandboxed state per mod, launcher API subset, VFS /Mods /GameData /GameDir
  (`shar_modlua.cpp`).
- `Main=1` mods save to ModSaves/<InternalName>/. Settings = Meta.ini defaults unless ModSettings.ini overrides.
- Config APIs: `shar_ModConfig*`, `shar_ModConfigFolder[Language]`, XML via `shar_modxml.h`; limits
  `shar_ModLimit*`/`shar_ModLimitFloat`/`shar_ModLimitBlank`; `shar_ModHackRequired`.
- Logs: `SHAR_MOD_LOG=1` (files provided, passengers, checkpoints), `SHAR_MOD_DUMP=dir`.

## Hacks implemented (where)
- OggVorbisSupport: x.ogg serves x.rsd (`radsound/linux/oggdecoder.cpp`, "OggS" in `rsdfiledatasource.cpp`).
- CustomLimits (doubles; several mods: largest wins, smallest for BoundsMinimumY):
  [Billboards]; [Regions] Limit/EntityLimit/RoadSegmentLimit; [Scripting] ArgumentLengthLimit; [Roads] RoadManager;
  [Pedestrians] PathLimit/GroupLimit (PathManager, `GetMaxModelGroups`, allocated); [TreeNodes] DrawDistance/Bounds*
  (WorldScene; blank keeps k-d tree's); [CollisionIndices] WorldPhysicsManager (+16 KB Temp per extra area + 256 KB in
  `PrepareHeapsStartup`); [Heaps] TempSize/LevelHUD*Size (MB, `PrepareHeaps*`); [Cars] CarLimit (VehicleCentral, 127
  slots, `GetMaxActiveVehicles()`), HuskLimit, SkidMarkLimit; [AnimEntities] (<=127); [Missions] StageLimit (128,
  `Mission::GetMaxStages`); [Triggers] VolumeLimit (TriggerVolumeTracker, allocated); [Miscellaneous]
  DeletedEntityLimit (RenderManager), ActionButtonLimit, old CarLimit/PathLimit.
  Not done: [Sound] Playing*PlayerLimit (docs' defaults 25/8 match nothing here: 64 clip / 9 stream fixed arrays,
  `playermanager.cpp`).
- DynamicTreeNodeEntityLimits: tree node lists grow (`WorldScene::AddGrowing`, `SwapArray::Grow`, level heap); always on.
- CustomInteriorSupport (`shar_modinteriors.cpp`, InteriorManager): [Interior] list replaces `ClassifyPoint` positions;
  Mirror*/ResetAmbientCharacter override Moe's/Bart's room; AllowJumping/Kicking/Sprinting (`CanJump/CanKick/CanSprint`).
- CustomRoadBehaviour (`shar_modroads.cpp`): DisableTrafficGroundSnapping (`Road::GetModNoGroundSnap`, set by
  RoadLoader from the name; release keeps no road names). UTurnTolerance ignored.
- CustomLicenseScreenTime (`bootupcontext.cpp`; Skippable via `BootupContext::SkipLicenseScreen`).
- NoMainMenuCameraAnimations (`CGuiScreen::StartTransitionAnimation`). HUDMapIgnorePlayerHeight (`HudMapCam::Update`).
- IncreasedVideoResolutionSupport (see platform.md, movies).
- CustomTrafficSupport (`shar_modtraffic.cpp`, `TrafficManager::ApplyModSettings`): AllocatedCars, MaxTraffic,
  MaxTrafficOnFoot, road/lane limits, intervals/distances, TrafficAI accel/decel, colours (level car -> car -> level
  -> root -> game's 25; also parked cars), group multiplier/weights; DynamicTraffic None/Models/Slots. PreallocatedCars
  (Slots: 0) cars made at level load; the rest and model swaps in `GetFreeTrafficVehicle`, ONE car at a time
  (`InitVehicle` queues its .con load; one per spawn overflowed the loading queue): the slot is unusable while
  `VehicleCentral::IsUnderConstruction`, and spawns wait for it (`mWantedModel`). Never load .con sync mid-game
  (`radFile::WaitForCompletion` runs other files' callbacks). Swaps free only cars with refcount 1, not in the
  active list (`ReleaseSlotVehicle`). AllocatedCars caps parked cars (`ParkedCarManager`, zone load).
  `SHAR_TRAFFIC_LOG=1` (slots made/freed, group changes, 5 s summary).
- CustomShopSupport: <CarShop> (`actionbuttonhandler.cpp`); <PhoneBooth>/<SkinShop> (`shar_modshops.cpp`,
  `IGuiScreenRewards::ApplyModShop`: Included/Excluded/FreeItems, HideLocked, Drawable). Release keeps only locator
  name UIDs: `ActionEventLocator::IsNamed` compares UIDs; locators named before `AddToGame`. Clothes menu 16 items.
- CustomDialogueCharacterCodes: [CharacterCodes] -> `dialogline.cpp` characterTable; [NongenericOutfits]/
  [NongenericCharacters] -> `dialoglist.cpp` (skinNameTable, characterMatches, GetStinkySkinPointer).
- CustomText (`guitextbible.cpp`, hook in FeLanguage::GetString): sections, $(Variables); language = ModSettings
  Language, else game's language title, else English, else first.
- 3DPhoneBoothPreviewSupport (`guiscreenphonebooth.cpp`): DarkenLockedCars (`Pure3dObject::SetLightScale`), FullScreen.
- CustomCarSupport (`shar_modcars.cpp`; `Vehicle::mModCar` from `AssignEnumBasedOnName`; unknown cars have
  `mVehicleID` INVALID 96): Index (`GetID`, follow-cam chunk), NoHusk, NoSkidMarks/NoFrontSkidMarks, ShadowVisible,
  Abductable, Recolourable, Invincible, NoBumperCam, RearWheelSparks, PreviewScale, steering/skid multipliers,
  [FakeWheel] (`FakeWheelJointDriver`), IsHusk/Husk= (`Vehicle::IsHuskModel()`, `HuskPool`). Unplayed: custom husk,
  PreviewScale, IsHusk.
- AdditionalScriptFunctionality (`code/mission/modscript.cpp`, `objectives/modobjectives.cpp`,
  `conditions/modconditions.cpp`; LINUX_EXTRA): all commands registered (console table 512); unimplemented log once.
  Objectives run as type "dummy": `ModCounter` (coins, wrench, wasps, destroycars, event, hitandrun*, peds, jump,
  speed, spring), AnimatedCam (+ SetObjUseCameraPosition -> `ModScript::SpawnCenter`), insidetrigger/outsidetrigger
  (`ModTrigger`). Conditions: counters after SetCondDelay, maintainspeed, triggers; unknown names skipped.
  `event` numbers are retail's (+1 from 177; `ModTranslateEvent`). Stage settings `MissionStage::GetModStage()`
  applied end of `Reset`, restored in Finalize. Checkpoints `Mission::mModCheckpoint` (`ModScript::OnRetry`;
  `Mission::Initialize` moves reset stage/locators/dynaload/peds). Passengers `Character::SetModSeat`.
  Traffic groups -> `TrafficManager::SetModTrafficGroup`: SetStageTrafficGroup at stage start (kept after);
  UseTrafficGroup / a retry's SetCheckpointTrafficGroup in `Mission::InitDynaLoad` (restart/select, not SD -> main).
  SetHitAndRunDecayInterior corrected only when hack required.
- CustomStatsTotals (`charactersheet/modstatstotals.cpp`): totals for scrapbook, pause progress, mission select,
  `QueryPercent*`; defaults reproduce original (100.3% on a complete save).
- Not done: CustomAnimationSetSupport, CustomAudioSupport, CustomBonusMissionSupport, CustomSkinPreviewAnimations,
  OverrideShaderParameters, CustomTriggerActions, HoverCarRefraction. Only the first mod's `Shaders/` is read.

## Test mods
- Futurama demo 0.1.3: L1 missions m0-m4 pass with autopass (not hand-played); shops/booth user-tested; booths FB001-6.
- Fully Connected Map 1.0.7: L1 plays; its doors: Moe's 872.3,7.1,-473.8, DMV 951.9,6.9,-799.9, Android's
  426.5,36.3,-1145.9, Observatory -67.4,33.5,-991.1. Its L2/L5 interiors lie outside the L2 render tree (data issue).
  Bridge-road traffic unchecked.
- SHAR Remastered 1.0: boots.
