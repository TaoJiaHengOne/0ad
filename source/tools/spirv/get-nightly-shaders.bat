rem **Download SPIR-V shaders from the latest nightly build**

for %%m in (mod public) do (
  svn export --force https://svn.wildfiregames.com/nightly-build/trunk/binaries/data/mods/%%m/shaders/spirv ..\..\..\binaries\data\mods\%%m\shaders\spirv
)
