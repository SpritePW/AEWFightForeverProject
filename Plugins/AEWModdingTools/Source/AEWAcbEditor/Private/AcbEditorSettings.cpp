#include "AcbEditorSettings.h"

#include "Misc/Paths.h"

#include <initializer_list>

UAcbEditorSettings::UAcbEditorSettings()
{
	BaseUnlockTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Unlockable/DT_UnlockableItemParam.DT_UnlockableItemParam"));

	struct FUnlockDef
	{
		const TCHAR* Table;
		int32 FirstID;
		int32 LastID;
	};
	const FUnlockDef NoUnlock{ nullptr, 0, 0 };
	const FUnlockDef MusicUnlock{ TEXT("DT_UnlockableItemParam_Music"), 9900000, 9999999 };
	const FUnlockDef SoundUnlock{ TEXT("DT_UnlockableItemParam_Sound"), 9800000, 9899999 };
	const FUnlockDef VoiceUnlock{ TEXT("DT_UnlockableItemParam_WrestlerParts"), 3710013, 3710999 };
	auto Add = [this](const TCHAR* Name, const TCHAR* Folder, const TCHAR* Table, int32 FirstID, int32 LastID, const FUnlockDef& Unlock)
	{
		FAcbRowTable Row;
		Row.Name = Name;
		Row.Table = FSoftObjectPath(FString::Printf(TEXT("/Game/GameData/Assets/DataTable/%s/%s.%s"), Folder, Table, Table));
		Row.NewTable = FSoftObjectPath(FString::Printf(TEXT("/Game/GameData/Assets/DataTable/%s/New/%s_New.%s_New"), Folder, Table, Table));
		Row.bNumberedRows = FirstID > 0;
		Row.FirstID = FirstID;
		Row.LastID = LastID;
		if (Unlock.Table)
		{
			Row.UnlockNewTable = FSoftObjectPath(FString::Printf(TEXT("/Game/GameData/Assets/DataTable/Unlockable/New/%s.%s"), Unlock.Table, Unlock.Table));
			Row.FirstUnlockID = Unlock.FirstID;
			Row.LastUnlockID = Unlock.LastID;
		}
		RowTables.Add(Row);
	};
	Add(TEXT("Music"), TEXT("Sound"), TEXT("DT_MMusicList"), 900001, 999999, MusicUnlock);
	Add(TEXT("Chants"), TEXT("Sound"), TEXT("DT_MCrowdChant"), 1000, 9999, NoUnlock);
	Add(TEXT("RA Roster"), TEXT("Sound"), TEXT("DT_MRAWrestler"), 900001, 999999, NoUnlock);
	Add(TEXT("RA Teams"), TEXT("Sound"), TEXT("DT_MRATeam"), 900001, 999999, SoundUnlock);
	Add(TEXT("RA CAW"), TEXT("Sound"), TEXT("DT_MRACustom"), 900001, 999999, NoUnlock);
	Add(TEXT("RA Base"), TEXT("Sound"), TEXT("DT_MRABase"), 9000, 9999, NoUnlock);
	Add(TEXT("SFX"), TEXT("Sound"), TEXT("DT_MSfxList"), 90000, 99999, NoUnlock);
	Add(TEXT("Voice Notify"), TEXT("Sound"), TEXT("DT_GameAudio_VO_CustomNotify"), 10013, 10999, NoUnlock);
	Add(TEXT("SFX Notify"), TEXT("Sound"), TEXT("DT_GameAudio_SFX_CustomNotify"), 0, 0, NoUnlock);
	Add(TEXT("By Wrestler"), TEXT("Sound"), TEXT("DT_Sound_ByWrestlerID"), 1000, 9999, NoUnlock);
	Add(TEXT("Voice Types"), TEXT("Create"), TEXT("DT_VoiceTypeList"), 10013, 10999, VoiceUnlock);
	Add(TEXT("SS Damage"), TEXT("SSMode/Sound"), TEXT("DT_SSDamageSEData"), 0, 0, NoUnlock);
}

namespace AcbEditorPresetsLocal
{
	struct FCueDef
	{
		const TCHAR* Pattern;
		const TCHAR* Category;
		const TCHAR* Template;
	};

	FAcbCuePreset Make(const TCHAR* Sheet, const TCHAR* Name, std::initializer_list<FCueDef> Cues)
	{
		FAcbCuePreset Preset;
		Preset.CueSheet = Sheet;
		Preset.Name = Name;
		for (const FCueDef& Def : Cues)
		{
			FAcbCueTemplate Cue;
			Cue.Pattern = Def.Pattern;
			Cue.Category = Def.Category;
			Cue.Template = Def.Template;
			Preset.Cues.Add(Cue);
		}
		return Preset;
	}
}

const TArray<FAcbCuePreset>& UAcbEditorSettings::BuiltInPresets()
{
	using namespace AcbEditorPresetsLocal;
	static const TArray<FAcbCuePreset> Presets = []()
	{
		TArray<FAcbCuePreset> List;
		List.Add(Make(TEXT("music"), TEXT("Wrestler theme (entrance, victory, in-game, jukebox)"), {
			{ TEXT("mus_ent_"), TEXT("EntranceMusic"), TEXT("mus_ent_AdamPage") },
			{ TEXT("mus_vic_"), TEXT("VictoryMusic"), TEXT("mus_vic_AdamPage") },
			{ TEXT("mus_ingame_"), TEXT("InGameMusic"), TEXT("mus_ingame_AdamPage") },
			{ TEXT("mus_jukebox_"), TEXT("OutGameMusic"), TEXT("mus_jukebox_AdamPage") } }));
		List.Add(Make(TEXT("music"), TEXT("Wrestler theme: in-game + jukebox only"), {
			{ TEXT("mus_ingame_"), TEXT("InGameMusic"), TEXT("mus_ingame_AdamPage") },
			{ TEXT("mus_jukebox_"), TEXT("OutGameMusic"), TEXT("mus_jukebox_AdamPage") } }));
		List.Add(Make(TEXT("music"), TEXT("Wrestler theme: entrance + victory only"), {
			{ TEXT("mus_ent_"), TEXT("EntranceMusic"), TEXT("mus_ent_AdamPage") },
			{ TEXT("mus_vic_"), TEXT("VictoryMusic"), TEXT("mus_vic_AdamPage") } }));
		List.Add(Make(TEXT("music"), TEXT("Career music (mus_career_)"), {
			{ TEXT("mus_career_"), TEXT("OutGameMusic"), TEXT("mus_career_gym") } }));
		List.Add(Make(TEXT("music"), TEXT("Minigame music (mus_minigame_)"), {
			{ TEXT("mus_minigame_"), TEXT("EntranceMusic"), TEXT("mus_minigame_01") } }));

		List.Add(Make(TEXT("crowd"), TEXT("Name chant (sfx_crowd_chant_name_)"), {
			{ TEXT("sfx_crowd_chant_name_"), TEXT("Crowd + CrowdChant"), TEXT("sfx_crowd_chant_name_adamcole") } }));
		List.Add(Make(TEXT("crowd"), TEXT("Let's go chant (sfx_crowd_chant_letsgo_)"), {
			{ TEXT("sfx_crowd_chant_letsgo_"), TEXT("Crowd + CrowdChant"), TEXT("sfx_crowd_chant_letsgo_jericho") } }));
		List.Add(Make(TEXT("crowd"), TEXT("Tag chant (sfx_crowd_chant_tag_)"), {
			{ TEXT("sfx_crowd_chant_tag_"), TEXT("Crowd + CrowdChant"), TEXT("sfx_crowd_chant_tag_jeffhardy") } }));
		List.Add(Make(TEXT("crowd"), TEXT("One-shot crowd reaction (sfx_crowd_oneshot_)"), {
			{ TEXT("sfx_crowd_oneshot_"), TEXT("Crowd"), TEXT("sfx_crowd_oneshot_anticipating_long") } }));

		List.Add(Make(TEXT("ra"), TEXT("Roster name (ra_name_roster_)"), {
			{ TEXT("ra_name_roster_"), TEXT("RingAnnounce"), TEXT("ra_name_roster_AdamCole") } }));
		List.Add(Make(TEXT("ra"), TEXT("CAW name (opening + closing)"), {
			{ TEXT("ra_name_caw_open_"), TEXT("RingAnnounce"), TEXT("ra_name_caw_open_Adam") },
			{ TEXT("ra_name_caw_close_"), TEXT("RingAnnounce"), TEXT("ra_name_caw_close_Adam") } }));
		List.Add(Make(TEXT("ra"), TEXT("CAW name, opening only (ra_name_caw_open_)"), {
			{ TEXT("ra_name_caw_open_"), TEXT("RingAnnounce"), TEXT("ra_name_caw_open_Adam") } }));
		List.Add(Make(TEXT("ra"), TEXT("CAW name, closing only (ra_name_caw_close_)"), {
			{ TEXT("ra_name_caw_close_"), TEXT("RingAnnounce"), TEXT("ra_name_caw_close_Adam") } }));
		List.Add(Make(TEXT("ra"), TEXT("Team name (ra_tn_)"), {
			{ TEXT("ra_tn_"), TEXT("RingAnnounce"), TEXT("ra_tn_best_friends") } }));

		List.Add(Make(TEXT("referee"), TEXT("Referee call (vo_referee_)"), {
			{ TEXT("vo_referee_"), TEXT("RefereeVoice"), TEXT("vo_referee_caution_AubreyEdwards") } }));

		List.Add(Make(TEXT("generic_roster"), TEXT("Roster voice line, male (vo_roster_)"), {
			{ TEXT("vo_roster_"), TEXT("Voice"), TEXT("vo_roster_AdamCole_male_atk_strong") } }));
		List.Add(Make(TEXT("generic_roster"), TEXT("Roster voice line, female (vo_roster_)"), {
			{ TEXT("vo_roster_"), TEXT("Voice"), TEXT("vo_roster_AnnaJay_female_atk_strong") } }));

		List.Add(Make(TEXT("exclusive_roster"), TEXT("Exclusive voice line (vo_exclusive_)"), {
			{ TEXT("vo_exclusive_"), TEXT("Voice"), TEXT("vo_exclusive_abadon_screamingstallingtaunt") } }));
		List.Add(Make(TEXT("exclusive_roster"), TEXT("Exclusive catchphrase for the crowd (vo_exclusive_)"), {
			{ TEXT("vo_exclusive_"), TEXT("Crowd"), TEXT("vo_exclusive_adamcole_boom") } }));

		List.Add(Make(TEXT("voice"), TEXT("Voice line (vo_)"), {
			{ TEXT("vo_"), TEXT("Voice"), TEXT("vo_career_intro_01") } }));
		List.Add(Make(TEXT("gimmick"), TEXT("Weapon sound (sfx_weapon_)"), {
			{ TEXT("sfx_weapon_"), TEXT("SFX"), TEXT("sfx_weapon_bamboo_guard") } }));

		List.Add(Make(TEXT("ingame_sfx"), TEXT("Strike hit (sfx_strike_)"), {
			{ TEXT("sfx_strike_"), TEXT("SFX"), TEXT("sfx_strike_punch_strong") } }));
		List.Add(Make(TEXT("ingame_sfx"), TEXT("Ground impact (sfx_groundimpact_)"), {
			{ TEXT("sfx_groundimpact_"), TEXT("SFX"), TEXT("sfx_groundimpact_blunt_strong_mat") } }));
		List.Add(Make(TEXT("ingame_sfx"), TEXT("Ring impact (sfx_impact_)"), {
			{ TEXT("sfx_impact_"), TEXT("SFX"), TEXT("sfx_impact_turnbuckle_strong") } }));
		List.Add(Make(TEXT("ingame_sfx"), TEXT("Rope rebound (sfx_rope_rebound_)"), {
			{ TEXT("sfx_rope_rebound_"), TEXT("SFX"), TEXT("sfx_rope_rebound_strong") } }));
		List.Add(Make(TEXT("ingame_sfx"), TEXT("Footstep (sfx_footstep_)"), {
			{ TEXT("sfx_footstep_"), TEXT("SFX"), TEXT("sfx_footstep_walk_mat") } }));
		List.Add(Make(TEXT("ingame_sfx"), TEXT("Ambience loop (sfx_amb_)"), {
			{ TEXT("sfx_amb_"), TEXT("SFX"), TEXT("sfx_amb_training_room_lp") } }));
		List.Add(Make(TEXT("ingame_sfx"), TEXT("Career sound (sfx_career_)"), {
			{ TEXT("sfx_career_"), TEXT("SFX"), TEXT("sfx_career_phone_ring") } }));

		List.Add(Make(TEXT("menu_ui"), TEXT("Menu sound (sfx_ui_)"), {
			{ TEXT("sfx_ui_"), TEXT("MenuSFX"), TEXT("sfx_ui_alert") } }));
		List.Add(Make(TEXT("menu_ui"), TEXT("In-match UI sound (sfx_ui_)"), {
			{ TEXT("sfx_ui_"), TEXT("SFX"), TEXT("sfx_ui_ss_acquire_common") } }));
		List.Add(Make(TEXT("menu_ui"), TEXT("Menu sound that ducks other audio (sfx_ui_)"), {
			{ TEXT("sfx_ui_"), TEXT("MenuSFX + Ducking"), TEXT("sfx_ui_sloweffect") } }));
		return List;
	}();
	return Presets;
}

TArray<FAcbCuePreset> UAcbEditorSettings::GetPresetsFor(const FString& CueSheetName) const
{
	TArray<FAcbCuePreset> Result;
	auto Matches = [&CueSheetName](const FAcbCuePreset& Preset)
	{
		return Preset.CueSheet.IsEmpty() || CueSheetName.IsEmpty() || Preset.CueSheet.Equals(CueSheetName, ESearchCase::IgnoreCase);
	};
	for (const FAcbCuePreset& Preset : BuiltInPresets())
	{
		if (Matches(Preset))
		{
			Result.Add(Preset);
		}
	}
	for (const FAcbCuePreset& Preset : CuePresets)
	{
		if (Matches(Preset))
		{
			Result.Add(Preset);
		}
	}
	return Result;
}

FString UAcbEditorSettings::GetOutputDirectory() const
{
	const FString Dir = OutputDirectory.Path.IsEmpty()
		? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AEWAcbEditor"))
		: OutputDirectory.Path;
	return FPaths::ConvertRelativePathToFull(Dir);
}

#if WITH_EDITOR
void UAcbEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SaveConfig();
}
#endif
