//var g_CurrentMusic = null;
//var g_CurrentAmbient = null;
//
//var g_MusicGain = 0.5;
//
//const MUSIC_PATH = "audio/music/";
//var g_PeaceTracks = [];
//var g_BattleTracks = [];
//
//
//const DEFEAT_CUE = "gen_loss_cue";
//const DEFEAT_MUSIC = "gen_loss_track";
//const VICTORY_MUSIC = "Victory_Track";
//
///*
// * At some point, this ought to be extended to do dynamic music selection and
// * crossfading - it at least needs to pick the music track based on the player's
// * civ and peace/battle
// */
//
//function storeTracks(civMusic)
//{
//	for each (var music in civMusic)
//	{
//		if ("peace" == music["Type"])
//		{
//			g_PeaceTracks.push(music["File"]);
//		}
//		else if ("battle" == music["Type"])
//		{
//			g_BattleTracks.push(music["File"]);
//		}
//	}
//}
//
//function getRandomPeaceTrack()
//{
//	return MUSIC_PATH + g_PeaceTracks[getRandom(0, g_PeaceTracks.length-1)];
//}
//
//function getRandomBattleTrack()
//{
//	return MUSIC_PATH + g_BattleTracks[getRandom(0, g_BattleTracks.length-1)];
//}
//
//function playDefeatMusic()
//{
//	switchMusic(DEFEAT_CUE, 0.0, false);
//	switchMusic(DEFEAT_MUSIC, 10.0, true);
//}
//
//function playVictoryMusic()
//{
//	switchMusic(VICTORY_MUSIC, 0.0, true);
//}
//
//function isEndingMusicPlaying()
//{
//	if (g_CurrentMusic)
//	{
//		if (g_CurrentMusic[DEFEAT_CUE] ||
//		    g_CurrentMusic[DEFEAT_MUSIC] ||
//		    g_CurrentMusic[VICTORY_MUSIC])
//		{
//			return true;
//		}
//	}
//
//	return false;
//}
//
//
//function isMusicPlaying()
//{
//	if (g_CurrentMusic)
//		return g_CurrentMusic.isPlaying();
//
//	return false;
//}
//
//function startSound(civMusic)
//{
//	storeTracks(civMusic);
//
//	g_CurrentAmbient = new Sound("audio/ambient/dayscape/day_temperate_gen_03.ogg");
//	if (g_CurrentAmbient)
//	{
//		g_CurrentAmbient.loop();
//		g_CurrentAmbient.setGain(0.8);
//	}
//
//	g_CurrentMusic = new Sound(getRandomPeaceTrack());
//	if (g_CurrentMusic)
//	{
//		g_CurrentMusic.loop();
//		g_CurrentMusic.fade(0.0, g_MusicGain, 10.0);
//	}
//}
//
//function switchMusic(track, fadeInPeriod, isLooping)
//{
//	if (g_CurrentMusic)
//		g_CurrentMusic.fade(-1, 0.0, 5.0);
//
//	g_CurrentMusic = new Sound(MUSIC_PATH + track + ".ogg");
//
//	if (g_CurrentMusic)
//	{
//		if (isLooping)
//			g_CurrentMusic.loop();
//		else
//		    g_CurrentMusic.play();
//
//		if (fadeInPeriod)
//			g_CurrentMusic.fade(0.0, g_MusicGain, fadeInPeriod);
//	}
//}
//
//function stopSound()
//{
//	stopMusic();
//	stopAmbient();
//}
//
//function stopMusic()
//{
//	if (g_CurrentMusic)
//	{
//		g_CurrentMusic.fade(-1, 0.0, 5.0);
//		g_CurrentMusic = null;
//	}
//}
//
//function stopAmbient()
//{
//	if (g_CurrentAmbient)
//	{
//		g_CurrentAmbient.fade(-1, 0.0, 5.0);
//		g_CurrentAmbient = null;
//	}
//}
