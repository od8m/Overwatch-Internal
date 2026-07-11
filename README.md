## Build
Output goes to `bin/`:

```
bin/
  blind.dll
  inject.exe
  assets/
    heroes/
    abilities/
```

Keep the `assets` folder next to `blind.dll` hero portraits and ability icons load from there so if the folder is not in same path as dll png's will not load

## Use
1. I recommend you pack and virtulize the dll before using VMP or Themida
2. Drag `blind.dll` onto `inject.exe`
3. **Insert** to open/close the menu
4. To unload: Info tab > **Unload**, then inject again if you want
5. If esp bugs or doesnt render ent list go Info tab > **Reload**

## Signing
You **cannot inject** if `blind.dll` is not signed 

I'll include cert + sign tool in the repo (`cert/` folder). After you build, run batch file in cmd as admin `sign.bat blind.dll` and when asked set at your current date `07-09-26`

```
cert/
  signtool.exe
  CSC3-2010.crt
  current_cert.pfx
  imgui.ini
  sign.bat
```

## layout
```
src/       source
include/   headers
assets/    icon pngs (copied to bin on build)
ImGui/     menu UI
blind.vcxproj   → blind.dll
inject.vcxproj  → inject.exe
```


## Notes

- Offsets break when Blizzard patches the game. Update `include/offsets.hpp` when that happens
- Will continue to update offsets every patch and will maybe add my dumper
