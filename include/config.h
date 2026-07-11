#pragma once
#include "sdk.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include "stealth.h"

extern char g_activeConfig[64];

static inline void CfgGetDir(char* buf, int n)
{
	char root[MAX_PATH];
	StealthConfigRoot(root, MAX_PATH);
	snprintf(buf, n, S("%s\\cfg"), root);
	CreateDirectoryA(buf, nullptr);
}

static inline void CfgSanitizeName(char* name)
{
	if (!name || !name[0]) {
		strncpy(name, S("default"), 63);
		name[63] = 0;
		return;
	}
	for (char* p = name; *p; p++) {
		char c = *p;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '_' || c == '-')
			continue;
		*p = '_';
	}
	if (!name[0]) {
		strncpy(name, S("default"), 63);
		name[63] = 0;
	}
}

static inline void CfgBuildPath(const char* name, char* out, int n)
{
	char dir[MAX_PATH];
	char safe[64];
	strncpy(safe, name, 63);
	safe[63] = 0;
	CfgSanitizeName(safe);
	CfgGetDir(dir, MAX_PATH);
	snprintf(out, n, S("%s\\%s.cfg"), dir, safe);
}

static inline void CfgReset()
{
	g_drawBoxes = false; g_drawSkeleton = false; g_drawLines = false;
	g_draw3dBox = true; g_teamCheck = true; g_visCheck = false;
	g_ultBars = false; g_ultPanel = false;
	g_drawEntList = true; g_skelRotOffset = 2.5f;
	g_drawHpBars = true; g_showGameInfo = false; g_teamOverride = 0;
	g_drawRadar = false; g_radarSize = 140.f; g_radarRange = 45.f; g_radarShowAllies = true;
	g_offscreenArrows = true; g_behindWarning = true; g_distanceEsp = false;
	g_heroSwapAlert = false; g_ultReadyAlert = false; g_allyUltPanel = true;
	g_drawMatchTimer = false; g_lowHpPulse = true;
	g_matchHudTransparent = true; g_matchHudShowMap = true; g_matchHudShowPhase = true;
	g_matchHudShowPing = true; g_matchHudShowTimer = true; g_matchHudStyle = 1;
	g_drawHpPackBoxes = true; g_drawHpPackLabels = true; g_hpPackLabelOffset = 2.f;
	g_priorityMarker = true; g_fovThreatCount = true; g_offscreenUseFov = true;
	g_markerFov = 60.f; g_drawMarkerCircle = false;
	g_drawHeroIcon = true; g_heroIconSize = 50.f; g_heroIconHiddenOnly = false;
	g_chams = false; g_chamsColor[0] = 1.f; g_chamsColor[1] = 0.2f; g_chamsColor[2] = 0.9f;
	g_chamsFill = 0.6f; g_drawHpPacks = true;
	g_freecam = false; g_freecamSpeed = 18.f;
	g_thirdPerson = false; g_thirdPersonDist = 4.f;
	g_abilityPanel = false; g_abilityDebug = false;
	g_glow = false; g_glowColor[0] = 1.f; g_glowColor[1] = 0.2f; g_glowColor[2] = 0.9f;
	g_glowThickness = 1.6f;
	g_aimVisCheck = true; g_drawHeroName = false; g_drawPlayerName = false;
	g_drawHeadDot = false; g_headDotSize = 4.f;
	g_drawFacingLine = false; g_facingLineLen = 2.5f;
}

static inline void CfgWrite(FILE* f)
{
	if (!f) return;
	fprintf(f, S("# cfg\n"));
	fprintf(f, S("[ESP]\nboxes=%d\nskel=%d\nlines=%d\nbox3d=%d\nteam=%d\nvis=%d\n"),
		(int)g_drawBoxes,(int)g_drawSkeleton,(int)g_drawLines,(int)g_draw3dBox,(int)g_teamCheck,(int)g_visCheck);
	fprintf(f, S("ultbars=%d\nultpanel=%d\n"), (int)g_ultBars, (int)g_ultPanel);
	fprintf(f, S("entlist=%d\nskelrot=%f\n"), (int)g_drawEntList, g_skelRotOffset);
	fprintf(f, S("heroicon=%d\niconsize=%f\nheroiconhidden=%d\n"), (int)g_drawHeroIcon, g_heroIconSize, (int)g_heroIconHiddenOnly);
	fprintf(f, S("chams=%d\nchamsr=%f\nchamsg=%f\nchamsb=%f\nchamsfill=%f\n"),
		(int)g_chams, g_chamsColor[0], g_chamsColor[1], g_chamsColor[2], g_chamsFill);
	fprintf(f, S("freecam=%d\nfreecamspeed=%f\n"), (int)g_freecam, g_freecamSpeed);
	fprintf(f, S("thirdperson=%d\nthirdpersondist=%f\n"), (int)g_thirdPerson, g_thirdPersonDist);
	fprintf(f, S("hppacks=%d\nabilpanel=%d\nabildebug=%d\n"), (int)g_drawHpPacks, (int)g_abilityPanel, (int)g_abilityDebug);
	fprintf(f, S("glow=%d\nglowr=%f\nglowg=%f\nglowb=%f\nglowthick=%f\n"),
		(int)g_glow, g_glowColor[0], g_glowColor[1], g_glowColor[2], g_glowThickness);
	fprintf(f, S("heroname=%d\nplayername=%d\naimvis=%d\n"),
		(int)g_drawHeroName, (int)g_drawPlayerName, (int)g_aimVisCheck);
	fprintf(f, S("headdot=%d\nheaddotsz=%f\nfacingline=%d\nfacinglen=%f\n"),
		(int)g_drawHeadDot, g_headDotSize, (int)g_drawFacingLine, g_facingLineLen);
	fprintf(f, S("hpbars=%d\ngameinfo=%d\nteamoverride=%d\n"),
		(int)g_drawHpBars, (int)g_showGameInfo, g_teamOverride);
	fprintf(f, S("radar=%d\nradarsz=%f\nradarrng=%f\nradarally=%d\n"),
		(int)g_drawRadar, g_radarSize, g_radarRange, (int)g_radarShowAllies);
	fprintf(f, S("offscreen=%d\nbehind=%d\ndistesp=%d\nheroswap=%d\nultalert=%d\n"),
		(int)g_offscreenArrows, (int)g_behindWarning, (int)g_distanceEsp,
		(int)g_heroSwapAlert, (int)g_ultReadyAlert);
	fprintf(f, S("allyult=%d\nmatchtimer=%d\nlowhp=%d\n"),
		(int)g_allyUltPanel, (int)g_drawMatchTimer, (int)g_lowHpPulse);
	fprintf(f, S("hudtrans=%d\nhudmap=%d\nhudphase=%d\nhudping=%d\nhudtimer=%d\nhudstyle=%d\n"),
		(int)g_matchHudTransparent, (int)g_matchHudShowMap, (int)g_matchHudShowPhase,
		(int)g_matchHudShowPing, (int)g_matchHudShowTimer, g_matchHudStyle);
	fprintf(f, S("hppackboxes=%d\nhppacklabels=%d\nhppacklabeloff=%f\n"),
		(int)g_drawHpPackBoxes, (int)g_drawHpPackLabels, g_hpPackLabelOffset);
	fprintf(f, S("priority=%d\nfovcount=%d\nofffov=%d\nmarkerfov=%f\nmarkercircle=%d\n"),
		(int)g_priorityMarker, (int)g_fovThreatCount, (int)g_offscreenUseFov,
		g_markerFov, (int)g_drawMarkerCircle);
}

static inline void _ln(const char* k, const char* v)
{
#define C(k) !strcmp(k,S(k))
#define ATOI(v) atoi(v)
#define ATOF(v) (float)atof(v)
	if      (C("boxes"))   g_drawBoxes      = ATOI(v)!=0;
	else if (C("skel"))    g_drawSkeleton   = ATOI(v)!=0;
	else if (C("lines"))   g_drawLines      = ATOI(v)!=0;
	else if (C("box3d"))   g_draw3dBox      = ATOI(v)!=0;
	else if (C("team"))    g_teamCheck      = ATOI(v)!=0;
	else if (C("vis"))     g_visCheck       = ATOI(v)!=0;
	else if (C("ultbars")) g_ultBars        = ATOI(v)!=0;
	else if (C("ultpanel"))g_ultPanel       = ATOI(v)!=0;
	else if (C("entlist")) g_drawEntList    = ATOI(v)!=0;
	else if (C("skelrot")) g_skelRotOffset  = ATOF(v);
	else if (C("heroicon")) g_drawHeroIcon  = ATOI(v)!=0;
	else if (C("iconsize")) g_heroIconSize  = ATOF(v);
	else if (C("heroiconhidden")) g_heroIconHiddenOnly = ATOI(v)!=0;
	else if (C("chams"))   g_chams          = ATOI(v)!=0;
	else if (C("chamsr"))  g_chamsColor[0]  = ATOF(v);
	else if (C("chamsg"))  g_chamsColor[1]  = ATOF(v);
	else if (C("chamsb"))  g_chamsColor[2]  = ATOF(v);
	else if (C("chamsfill")) g_chamsFill    = ATOF(v);
	else if (C("freecam"))   g_freecam      = ATOI(v)!=0;
	else if (C("freecamspeed")) g_freecamSpeed = ATOF(v);
	else if (C("thirdperson")) g_thirdPerson = ATOI(v)!=0;
	else if (C("thirdpersondist")) g_thirdPersonDist = ATOF(v);
	else if (C("hppacks")) g_drawHpPacks    = ATOI(v)!=0;
	else if (C("abilpanel")) g_abilityPanel = ATOI(v)!=0;
	else if (C("abildebug")) g_abilityDebug = ATOI(v)!=0;
	else if (C("glow"))      g_glow          = ATOI(v)!=0;
	else if (C("glowr"))     g_glowColor[0]  = ATOF(v);
	else if (C("glowg"))     g_glowColor[1]  = ATOF(v);
	else if (C("glowb"))     g_glowColor[2]  = ATOF(v);
	else if (C("glowthick")) g_glowThickness = ATOF(v);
	else if (C("heroname"))  g_drawHeroName   = ATOI(v)!=0;
	else if (C("playername"))g_drawPlayerName = ATOI(v)!=0;
	else if (C("headdot"))   g_drawHeadDot     = ATOI(v)!=0;
	else if (C("headdotsz")) g_headDotSize     = ATOF(v);
	else if (C("facingline"))g_drawFacingLine  = ATOI(v)!=0;
	else if (C("facinglen")) g_facingLineLen   = ATOF(v);
	else if (C("aimvis"))    g_aimVisCheck    = ATOI(v)!=0;
	else if (C("hpbars"))    g_drawHpBars     = ATOI(v)!=0;
	else if (C("gameinfo"))  g_showGameInfo   = ATOI(v)!=0;
	else if (C("teamoverride")) g_teamOverride = ATOI(v);
	else if (C("radar"))     g_drawRadar       = ATOI(v)!=0;
	else if (C("radarsz"))   g_radarSize       = ATOF(v);
	else if (C("radarrng"))  g_radarRange      = ATOF(v);
	else if (C("radarally")) g_radarShowAllies = ATOI(v)!=0;
	else if (C("offscreen")) g_offscreenArrows = ATOI(v)!=0;
	else if (C("behind"))    g_behindWarning   = ATOI(v)!=0;
	else if (C("distesp"))   g_distanceEsp     = ATOI(v)!=0;
	else if (C("heroswap"))  g_heroSwapAlert   = ATOI(v)!=0;
	else if (C("ultalert"))  g_ultReadyAlert   = ATOI(v)!=0;
	else if (C("allyult"))   g_allyUltPanel    = ATOI(v)!=0;
	else if (C("matchtimer"))g_drawMatchTimer  = ATOI(v)!=0;
	else if (C("hudtrans"))  g_matchHudTransparent = ATOI(v)!=0;
	else if (C("hudmap"))   g_matchHudShowMap = ATOI(v)!=0;
	else if (C("hudphase"))  g_matchHudShowPhase = ATOI(v)!=0;
	else if (C("hudping"))   g_matchHudShowPing = ATOI(v)!=0;
	else if (C("hudtimer"))  g_matchHudShowTimer = ATOI(v)!=0;
	else if (C("hudstyle"))  g_matchHudStyle = ATOI(v);
	else if (C("hppackboxes")) g_drawHpPackBoxes = ATOI(v)!=0;
	else if (C("hppacklabels")) g_drawHpPackLabels = ATOI(v)!=0;
	else if (C("hppacklabeloff")) g_hpPackLabelOffset = ATOF(v);
	else if (C("lowhp"))     g_lowHpPulse      = ATOI(v)!=0;
	else if (C("priority"))  g_priorityMarker   = ATOI(v)!=0;
	else if (C("fovcount"))  g_fovThreatCount   = ATOI(v)!=0;
	else if (C("offfov"))    g_offscreenUseFov  = ATOI(v)!=0;
	else if (C("markerfov")) g_markerFov        = ATOF(v);
	else if (C("markercircle")) g_drawMarkerCircle = ATOI(v)!=0;
	else if (C("aimfov"))    g_markerFov        = ATOF(v);
#undef C
#undef ATOI
#undef ATOF
}

static inline void CfgRead(FILE* f)
{
	if (!f) return;
	char line[256];
	char* eq;
	while (fgets(line, sizeof(line), f)) {
		int n = (int)strlen(line);
		while (n > 0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]='\0';
		if (!n || line[0]=='#' || line[0]=='[') continue;
		eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		_ln(line, eq+1);
	}
}

static inline bool CfgSavNamed(const char* name)
{
	char path[MAX_PATH];
	char safe[64];
	strncpy(safe, name ? name : g_activeConfig, 63);
	safe[63] = 0;
	CfgSanitizeName(safe);
	CfgBuildPath(safe, path, MAX_PATH);
	FILE* f = fopen(path, S("w"));
	if (!f) return false;
	CfgWrite(f);
	fclose(f);
	strncpy(g_activeConfig, safe, 63);
	g_activeConfig[63] = 0;
	return true;
}

static inline bool CfgLdNamed(const char* name)
{
	char path[MAX_PATH];
	char safe[64];
	strncpy(safe, name ? name : g_activeConfig, 63);
	safe[63] = 0;
	CfgSanitizeName(safe);
	CfgBuildPath(safe, path, MAX_PATH);
	FILE* f = fopen(path, S("r"));
	if (!f) return false;
	CfgReset();
	CfgRead(f);
	fclose(f);
	strncpy(g_activeConfig, safe, 63);
	g_activeConfig[63] = 0;
	return true;
}

static inline bool CfgDeleteNamed(const char* name)
{
	char path[MAX_PATH];
	char safe[64];
	strncpy(safe, name, 63);
	safe[63] = 0;
	CfgSanitizeName(safe);
	CfgBuildPath(safe, path, MAX_PATH);
	return DeleteFileA(path) != 0;
}

static inline void CfgList(std::vector<std::string>& out)
{
	out.clear();
	char dir[MAX_PATH];
	CfgGetDir(dir, MAX_PATH);
	char search[MAX_PATH];
	snprintf(search, MAX_PATH, S("%s\\*.cfg"), dir);

	WIN32_FIND_DATAA fd = {};
	HANDLE h = FindFirstFileA(search, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		std::string fname = fd.cFileName;
		size_t dot = fname.rfind('.');
		if (dot != std::string::npos) fname = fname.substr(0, dot);
		out.push_back(fname);
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	std::sort(out.begin(), out.end());
}

static inline void CfgSav() { CfgSavNamed(g_activeConfig); }
static inline void CfgLd()  { if (!CfgLdNamed(g_activeConfig)) CfgReset(); }
static inline bool CfgExists()
{
	char path[MAX_PATH];
	CfgBuildPath(g_activeConfig, path, MAX_PATH);
	FILE* f = fopen(path, S("r"));
	if (!f) return false;
	fclose(f);
	return true;
}

static inline void CfgGetPath(char* buf, int n)
{
	CfgBuildPath(g_activeConfig, buf, n);
}
