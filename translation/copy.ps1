# USER DEFINED
$outDir = "D:\Skyrim Mod Organizer\mods\True Directional Movement - Modernized Third Person Gameplay\Interface\Translations"

$strings = @('chinese', 'czech', 'english', 'french', 'german', 'italian', 'japanese', 'polish', 'russian', 'spanish')

ForEach ($string in $strings)
{
    Copy-Item "TrueDirectionalMovement_english.txt" -Destination "$outDir\TrueDirectionalMovement_$string.txt"
}