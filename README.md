![GitHub all releases](https://img.shields.io/github/downloads/ABKAM2023/cs2-weapon-onlyhs/total?style=for-the-badge)

## RU
**Weapon OnlyHS** - делает указанные в конфиге виды оружия "только хедшот": урон засчитывается **только при попадании в голову**, любые другие попадания этим оружием мгновенно откатываются (HP/броня восстанавливаются).

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
	"weapons"
	{
		"deagle"   "1"
		"ak47"     "1"
		"m4a1"     "0"
		"awp"      "0"
		"glock"    "0"
		"usp_silencer" "0"
	}

	// Переопределения по конкретным картам
	"maps"
	{
		"de_mirage"
		{
			// Наследовать глобальные + применить правки
			"inherit" "1"

			// Вариант 1: через вложенную секцию
			"weapons"
			{
				"ak47"   "1"  // добавить/оставить HS-only
				"deagle" "0"  // на mirage дигл без ограничения
				"famas"  "1"
			}
		}

		"de_dust2"
		{
			// Вариант 2: сразу ключи (без "weapons")
			"inherit" "0"  // НЕ наследуем глобальные - только то, что ниже

			"deagle" "1"
			"awp"    "1"
			"glock"    "1"				
		}
	}
}
```

## EN
**Weapon OnlyHS** - makes selected weapons **headshot-only**: damage is counted **only** on headshots; any other hits from those weapons are instantly reverted (HP/armor restored).

## Requirements
* [Utils](https://github.com/Pisex/cs2-menus/releases)

## Config
```ini
"Weapon_OnlyHS"
{
	// Debug logs to console
	"debug_log"              "0"

	// Anti-spam for PrintToCenter (seconds) applied to the attacker
	"center_hint_cooldown"   "2.0"

	// Weapons list: 1 — headshot-only, 0 — normal damage
	"weapons"
	{
		"deagle"         "1"
		"ak47"           "1"
		"m4a1"           "0"
		"awp"            "0"
		"glock"          "0"
		"usp_silencer"   "0"
	}

	// Per-map overrides
	"maps"
	{
		"de_mirage"
		{
			// Inherit global settings + apply changes
			"inherit" "1"

			// Option 1: via nested "weapons" section
			"weapons"
			{
				"ak47"   "1"  // add/keep HS-only
				"deagle" "0"  // on Mirage, Deagle is unrestricted
				"famas"  "1"
			}
		}

		"de_dust2"
		{
			// Option 2: set keys directly (without "weapons")
			"inherit" "0"  // Do NOT inherit globals — use only what's below

			"deagle" "1"
			"awp"    "1"
			"glock"  "1"
		}
	}
}
```
