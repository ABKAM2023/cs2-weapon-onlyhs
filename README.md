## RU
**Weapon_OnlyHS** - делает указанные в конфиге виды оружия "только хедшот": урон засчитывается **только при попадании в голову**, любые другие попадания этим оружием мгновенно откатываются (HP/броня восстанавливаются).

## Требования
* [Utils](https://github.com/Pisex/cs2-menus/releases)

## Конфиг
```ini
"Weapon_OnlyHS"
{
	// Отладочные логи в консоль
	"debug_log"              "0"

	// Антиспам для PrintToCenter (сек) на атакующего
	"center_hint_cooldown"   "2.0"

	// Список оружий: 1 — только хедшот, 0 — обычный урон
	// Ключи — имена оружия из game events (без префикса `weapon_`, регистр не важен).
	// Допускается и форма "weapon_deagle" — префикс будет отброшен.
	"weapons"
	{
		"deagle"         "1"
		"ak47"           "1"
		"m4a1"           "0"
		"awp"            "0"
		"glock"          "0"
		"usp_silencer"   "0"
	}
}
```

## EN
**Weapon_OnlyHS** - makes selected weapons **headshot-only**: damage is counted **only** on headshots; any other hits from those weapons are instantly reverted (HP/armor restored).

## Requirements
* [Utils](https://github.com/Pisex/cs2-menus/releases)

## Config
```ini
"Weapon_OnlyHS"
{
	// Console debug logs
	"debug_log"              "0"

	// Anti-spam for PrintToCenter (sec) for the attacker
	"center_hint_cooldown"   "2.0"

	// Weapon list: 1 — headshot-only, 0 — normal damage
	// Keys are weapon names from game events (no `weapon_` prefix, case-insensitive).
	// "weapon_deagle" is also accepted — the prefix will be stripped.
	"weapons"
	{
		"deagle"         "1"
		"ak47"           "1"
		"m4a1"           "0"
		"awp"            "0"
		"glock"          "0"
		"usp_silencer"   "0"
	}
}
```
