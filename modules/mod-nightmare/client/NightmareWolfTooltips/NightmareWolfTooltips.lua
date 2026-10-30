-- Nightmare Diseased Wolves tooltip for WoW 3.3.5a (Interface 30300).
-- Blizzard spell descriptions are client-side; this addon replaces the tooltip
-- of our self-cast Infected Bite aura without changing any combat mechanics.
--
-- These values match mod-nightmare.conf.dist defaults. If the server changes
-- its wolf settings, adjust the numbers here too.
local INFECTED_BITE_SPELL_ID = 7367
local DAMAGE_PERCENT_PER_STACK = 10
local SLOW_PERCENT_PER_STACK = 10
local MAX_STACKS = 10
local EXPECTED_DURATION = 10

local function ShowNightmareTooltip(tooltip, unit, name, stacks, duration, expirationTime, caster, spellID)
  -- Restrict to the player's own debuff. The stock 7367 spell can also be
  -- applied by unrelated creatures; do not rewrite those tooltips.
  if not name or spellID ~= INFECTED_BITE_SPELL_ID then
    return
  end
  if not unit or not UnitIsUnit(unit, "player") then
    return
  end
  if caster and not UnitIsUnit(caster, "player") then
    return
  end
  -- Nightmare gives the aura 10 seconds, unlike the stock WotLK spell.
  if duration and duration > 0 and math.abs(duration - EXPECTED_DURATION) > 1 then
    return
  end

  stacks = math.max(1, math.min(tonumber(stacks) or 1, MAX_STACKS))
  local damage = stacks * DAMAGE_PERCENT_PER_STACK
  local slow = stacks * SLOW_PERCENT_PER_STACK

  tooltip:ClearLines()
  tooltip:AddLine(name, 1, 0.3, 0.3)
  tooltip:AddLine("Disease - Nightmare", 1, 0.78, 0.15)
  tooltip:AddLine("Damage taken from all sources increased by " .. damage .. "%.", 1, 1, 1, true)
  tooltip:AddLine("Movement speed reduced by " .. slow .. "%.", 1, 1, 1, true)
  if slow >= 100 then
    tooltip:AddLine("You are immobilized.", 1, 0.4, 0.4, true)
  end

  tooltip:AddLine("Stacks: " .. stacks .. ". Each successful wolf bite refreshes the duration.",
    0.75, 0.75, 0.75, true)
  if expirationTime and expirationTime > 0 then
    local remaining = math.max(0, expirationTime - GetTime())
    tooltip:AddLine(string.format("%.1f sec remaining", remaining), 1, 0.82, 0.2)
  end
  tooltip:Show()
end

-- WotLK 3.3.5 returns spellID as the eleventh value of UnitDebuff/UnitAura.
-- Hook both standard tooltip entry points; custom third-party tooltip frames
-- may choose not to call either one.
hooksecurefunc(GameTooltip, "SetUnitDebuff", function(tooltip, unit, index, raidFilter)
  if not unit or not index then
    return
  end
  local name, _, _, stacks, _, duration, expirationTime, caster, _, _, spellID =
    UnitDebuff(unit, index, raidFilter)
  ShowNightmareTooltip(tooltip, unit, name, stacks, duration, expirationTime, caster, spellID)
end)

hooksecurefunc(GameTooltip, "SetUnitAura", function(tooltip, unit, index, filter)
  if not unit or not index or not filter or not string.find(filter, "HARMFUL", 1, true) then
    return
  end
  local name, _, _, stacks, _, duration, expirationTime, caster, _, _, spellID =
    UnitAura(unit, index, filter)
  ShowNightmareTooltip(tooltip, unit, name, stacks, duration, expirationTime, caster, spellID)
end)
