rem **Download translations from the latest nightly build**

rem **This will overwrite any uncommitted changes to messages.json files**

svn export --force --depth files https://svn.wildfiregames.com/nightly-build/trunk/binaries/data/l10n ..\..\..\binaries\data\l10n

for %%m in (mod public) do (
  svn export --force --depth files https://svn.wildfiregames.com/nightly-build/trunk/binaries/data/mods/%%m/l10n ..\..\..\binaries\data\mods\%%m\l10n
)

svn export --force https://svn.wildfiregames.com/nightly-build/trunk/binaries/data/mods/public/gui/credits/texts/translators.json ..\..\..\binaries\data\mods\public\gui\credits\texts\translators.json
