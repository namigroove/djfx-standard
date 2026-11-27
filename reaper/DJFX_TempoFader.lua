-- DJFX Standard: TEMPO フェーダー + MASTER TEMPO (DJプレーヤー風)  for REAPER
-- アクションリストから読み込んで実行してください(ReaScript / Lua)。
-- ・フェーダーは基準BPMからの±%。幅は ±6 / ±10 / ±16 / WIDE(±100%)
-- ・REC をオンにして再生すると、フェーダーの動きがプロジェクトのテンポ(テンポエンベロープ)に書き込まれる
-- ・REC がオフのときは、フェーダーでプロジェクトのテンポを直接変える
-- ・MASTER TEMPO オン = ピッチを保つ / オフ = テンポと一緒にピッチも変わる(選択アイテム、なければ全オーディオ)

local R = reaper
local ranges = { 6, 10, 16, 100 }
local rnames = { "±6", "±10", "±16", "WIDE" }
local ri = 2
local base = R.Master_GetTempo()
local pct = 0.0          -- -range..+range
local master = true
local rec = false
local lastBpm, lastT = nil, -1
local drag = false

local function bpmNow() return base * (1 + pct / 100) end

local function applyMaster()
  local n = R.CountSelectedMediaItems(0)
  local total = (n > 0) and n or R.CountMediaItems(0)
  R.Undo_BeginBlock()
  for i = 0, total - 1 do
    local it = (n > 0) and R.GetSelectedMediaItem(0, i) or R.GetMediaItem(0, i)
    local tk = R.GetActiveTake(it)
    if tk and not R.TakeIsMIDI(tk) then
      R.SetMediaItemInfo_Value(it, "C_BEATATTACHMODE", 1) -- 位置・長さ・レートをビートに紐づける
      R.SetMediaItemTakeInfo_Value(tk, "B_PPITCH", master and 1 or 0)
      R.UpdateItemInProject(it)
    end
  end
  R.Undo_EndBlock("DJFX: MASTER TEMPO " .. (master and "ON" or "OFF"), -1)
  R.UpdateArrange()
end

local function writeTempo()
  local bpm = bpmNow()
  local playing = (R.GetPlayState() & 1) == 1
  if rec and playing then
    local t = R.GetPlayPosition()
    if (not lastBpm or math.abs(bpm - lastBpm) >= 0.01) and (t - lastT >= 0.05 or t < lastT) then
      R.SetTempoTimeSigMarker(0, -1, t, -1, -1, bpm, 0, 0, false)
      R.UpdateTimeline()
      lastBpm, lastT = bpm, t
    end
  elseif not rec then
    if not lastBpm or math.abs(bpm - lastBpm) >= 0.01 then
      R.SetCurrentBPM(0, bpm, true)
      lastBpm = bpm
    end
  end
end

-- 描画 ------------------------------------------------------------------
local W, H = 300, 360
local FX, FY, FH = 150, 70, 240   -- フェーダー溝の位置(中心x, 上端y, 高さ)

local function box(x, y, w, h, on, label)
  gfx.set(on and 0.95 or 0.16, on and 0.55 or 0.16, on and 0.1 or 0.16, 1)
  gfx.rect(x, y, w, h, 1)
  gfx.set(0.55, 0.55, 0.55, 1); gfx.rect(x, y, w, h, 0)
  gfx.set(on and 0 or 0.85, on and 0 or 0.85, on and 0 or 0.85, 1)
  local tw, th = gfx.measurestr(label)
  gfx.x, gfx.y = x + (w - tw) / 2, y + (h - th) / 2
  gfx.drawstr(label)
end

local function hit(mx, my, x, y, w, h) return mx >= x and mx < x + w and my >= y and my < y + h end

local prevDown = false
local function loop()
  local mx, my = gfx.mouse_x, gfx.mouse_y
  local down = (gfx.mouse_cap & 1) == 1
  local click = down and not prevDown
  local rng = ranges[ri]

  if click then
    if hit(mx, my, 20, 330, 60, 22) then rec = not rec; lastBpm = nil
    elseif hit(mx, my, 90, 330, 90, 22) then master = not master; applyMaster()
    elseif hit(mx, my, 190, 330, 90, 22) then base = bpmNow(); pct = 0 -- 現在のBPMを基準にする
    elseif hit(mx, my, 230, 70, 60, 22) then ri = ri % 4 + 1; rng = ranges[ri]
      pct = math.max(-rng, math.min(rng, pct))
    elseif hit(mx, my, FX - 30, FY - 10, 60, FH + 20) then drag = true
      if (gfx.mouse_cap & 4) == 4 then pct = 0 end -- Ctrl+クリックで0
    end
  end
  if drag and down then
    pct = ((my - FY) / FH - 0.5) * 2 * rng
    pct = math.max(-rng, math.min(rng, pct))
    if math.abs(pct) < rng * 0.01 then pct = 0 end   -- 中央のクリック感
  end
  if not down then drag = false end
  if gfx.mouse_wheel ~= 0 then
    pct = math.max(-rng, math.min(rng, pct - (gfx.mouse_wheel > 0 and 1 or -1) * rng / 200))
    gfx.mouse_wheel = 0
  end
  if (gfx.mouse_cap & 2) == 2 and hit(mx, my, FX - 30, FY - 10, 60, FH + 20) then pct = 0 end
  prevDown = down

  writeTempo()

  gfx.set(0.05, 0.05, 0.05, 1); gfx.rect(0, 0, W, H, 1)
  gfx.setfont(1, "Arial", 14)
  gfx.set(0.9, 0.9, 0.9, 1); gfx.x, gfx.y = 20, 14; gfx.drawstr("TEMPO")
  gfx.setfont(1, "Arial", 24)
  gfx.set(1, 1, 1, 1); gfx.x, gfx.y = 20, 34; gfx.drawstr(string.format("%.2f BPM", bpmNow()))
  gfx.setfont(1, "Arial", 14)
  gfx.set(0.95, 0.55, 0.1, 1); gfx.x, gfx.y = 20, 72; gfx.drawstr(string.format("%+.2f %%", pct))
  box(230, 70, 60, 22, true, rnames[ri])
  -- 溝と目盛り
  gfx.set(0.3, 0.3, 0.3, 1); gfx.rect(FX - 3, FY, 6, FH, 1)
  gfx.set(0.6, 0.6, 0.6, 1)
  for i = 0, 10 do local y = FY + FH * i / 10; gfx.line(FX - (i == 5 and 18 or 10), y, FX + (i == 5 and 18 or 10), y) end
  gfx.setfont(1, "Arial", 11)
  gfx.x, gfx.y = FX + 24, FY - 6; gfx.drawstr("-" .. rng .. "%")
  gfx.x, gfx.y = FX + 24, FY + FH - 6; gfx.drawstr("+" .. rng .. "%")
  -- ノブ(キャップ)
  local ky = FY + FH * (0.5 + pct / rng / 2) -- 上=マイナス、下=プラス(プレーヤーと同じ向き)
  gfx.set(0.85, 0.85, 0.85, 1); gfx.rect(FX - 22, ky - 12, 44, 24, 1)
  gfx.set(0.1, 0.1, 0.1, 1); gfx.rect(FX - 22, ky - 1, 44, 2, 1)
  gfx.setfont(1, "Arial", 13)
  box(20, 330, 60, 22, rec, "REC")
  box(90, 330, 90, 22, master, "MASTER TEMPO")
  box(190, 330, 90, 22, false, "SET BASE")

  if gfx.getchar() >= 0 then R.defer(loop) end
end

gfx.init("DJFX TEMPO", W, H)
applyMaster()
loop()
